#include "EventDispatch.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>

#include "Bootstrapper.h"
#include "ProjectDescription.h"
#include "ProjectDescription_json.h"
#include "SubscriberUpdate.h"
#include "protocol/Protocol.h"

#define CLIENT_MESSAGE_READ_BUFFER_SIZE 128

enum HandleType {
    HANDLE_TYPE_SERVER_SOCKET,
    HANDLE_TYPE_CLIENT_SOCKET,
    HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION // This client socket will recieve status updates as well
};

struct BootstrapperUserdata {
    int gdbserver_pid;
};

struct DynamicBuffer {
    char* data;
    size_t size;
    size_t capacity;
};

#define DYNAMIC_BUFFER_INITIAL_SIZE 16

void DynamicBufferInit(struct DynamicBuffer* buffer) {
    buffer->size = 0;
    buffer->capacity = DYNAMIC_BUFFER_INITIAL_SIZE;
    buffer->data = (char*)malloc(DYNAMIC_BUFFER_INITIAL_SIZE);
}

void DynamicBufferDeinit(struct DynamicBuffer* buffer) { free(buffer->data); }

void _dynamicBufferExtend(struct DynamicBuffer* buffer, size_t minimal_new_size) {
    const size_t new_size = minimal_new_size * 2;
    buffer->data = (char*)realloc(buffer->data, new_size);
    buffer->capacity = new_size;
}

void DynamicBufferAppend(struct DynamicBuffer* buffer, const char* new_data, size_t new_data_size) {
    if (buffer->size + new_data_size >= buffer->capacity)
        _dynamicBufferExtend(buffer, buffer->size + new_data_size);

    memcpy(buffer->data + buffer->size, new_data, new_data_size);
    buffer->size += new_data_size;
}

void DynamicBufferTrimLeft(struct DynamicBuffer* buffer, size_t trim_amount) {
    if (trim_amount == buffer->size) {
        buffer->size = 0;
        return;
    }

    const size_t remainder = buffer->size - trim_amount;
    for (size_t i = 0; i < remainder; ++i)
        buffer->data[i] = buffer->data[i + trim_amount];
    buffer->size -= trim_amount;
}

struct PollingHandles {
    struct pollfd* pfds;
    enum HandleType* types;
    struct DynamicBuffer* reading_buffers;
    struct DynamicBuffer* writing_buffers;
    size_t size, capacity;
};

static void Init(struct PollingHandles* handles) {
    handles->size = 0;
    handles->capacity = 1;
    handles->pfds = (struct pollfd*)calloc(sizeof(struct pollfd), handles->capacity);
    handles->types = (enum HandleType*)calloc(sizeof(enum HandleType), handles->capacity);
    handles->reading_buffers = (struct DynamicBuffer*)malloc(sizeof(struct DynamicBuffer) * handles->capacity);
    handles->writing_buffers = (struct DynamicBuffer*)malloc(sizeof(struct DynamicBuffer) * handles->capacity);
}

static void FreeDynamicBufferArray(struct DynamicBuffer* dynamic_buffers, size_t n) {
    for (size_t i = 0; i < n; ++i)
        DynamicBufferDeinit(&dynamic_buffers[i]);
    free(dynamic_buffers);
}

static void Deinit(struct PollingHandles* handles) {
    free(handles->pfds);
    free(handles->types);
    FreeDynamicBufferArray(handles->reading_buffers, handles->capacity);
    FreeDynamicBufferArray(handles->writing_buffers, handles->capacity);
}

static void _extend(struct PollingHandles* handles) {
    handles->capacity *= 2;
    handles->pfds = realloc(handles->pfds, handles->capacity * sizeof(struct pollfd));
    handles->types = realloc(handles->types, handles->capacity * sizeof(enum HandleType));
    memset(handles->pfds + handles->size, 0, handles->size * sizeof(struct pollfd));
    memset(handles->types + handles->size, 0, handles->size * sizeof(enum HandleType));
    handles->reading_buffers = realloc(handles->reading_buffers, handles->capacity * sizeof(struct DynamicBuffer));
    handles->writing_buffers = realloc(handles->writing_buffers, handles->capacity * sizeof(struct DynamicBuffer));
}

static void Append(struct PollingHandles* handles, int fd, short events, enum HandleType type) {
    if (handles->size == handles->capacity)
        _extend(handles);

    handles->pfds[handles->size].fd = fd;
    handles->pfds[handles->size].events = events;
    handles->types[handles->size] = type;
    DynamicBufferInit(&handles->reading_buffers[handles->size]);
    DynamicBufferInit(&handles->writing_buffers[handles->size]);
    ++handles->size;
}

static void Erase(struct PollingHandles* handles, size_t at) {
    if (at < 0 || at >= handles->size)
        return;
    DynamicBufferDeinit(&handles->reading_buffers[at]);
    DynamicBufferDeinit(&handles->writing_buffers[at]);
    for (size_t i = at + 1; i < handles->size; ++i) {
        handles->pfds[i - 1] = handles->pfds[i];
        handles->types[i - 1] = handles->types[i];
        handles->reading_buffers[i - 1] = handles->reading_buffers[i];
        handles->writing_buffers[i - 1] = handles->writing_buffers[i];
    }
    --handles->size;
}

static void AddClientSocket(int socket_desc, struct PollingHandles* all_handles) {
    socklen_t c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;

    int client_sock = accept(socket_desc, (struct sockaddr*)&client, &c);
    if (client_sock < 0) {
        fprintf(stderr, "accept failed\n");
        exit(1);
    }

    printf("Connection accepted\n");

    Append(all_handles, client_sock, POLLIN, HANDLE_TYPE_CLIENT_SOCKET);

    fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);
}

static void InterpretProjectDescriptionClientData(struct DynamicBuffer* reading_buffer,
                                                  struct Bootstrapper* bootstrapper, size_t json_offset) {
    if (json_offset > reading_buffer->size)
        return;
    size_t null_terminator_index;
    if (FindNullTerminator(&reading_buffer->data[json_offset], reading_buffer->size - json_offset,
                           &null_terminator_index)) {
        null_terminator_index += json_offset;

        struct ProjectDescription description;
        if (ProjectDescriptionLoadFromJSON(&reading_buffer->data[json_offset], &description)) {
            printf("I got a valid project description!\n");
            DynamicBufferTrimLeft(reading_buffer, null_terminator_index + 1);
            ReceiveNewProjectDescription(bootstrapper, &description);
            ProjectDescriptionDeinit(&description);
        }
    }
}

static void AppendMessageToBroadcast(struct DynamicStringArray* subscriber_broadcast, const char* tag,
                                     const char* message) {
    char* encoded_message = EncodeSubscriberUpdateMessage(tag, message);
    DynamicStringArrayAppend(subscriber_broadcast, encoded_message);
    free(encoded_message);
}

// This will remove the data that is successfully interpreted
static void InterpretClientData(struct PollingHandles* all_handles, size_t fd_index, struct Bootstrapper* bootstrapper,
                                struct DynamicStringArray* subscriber_broadcast) {
    struct DynamicBuffer* reading_buffer = &all_handles->reading_buffers[fd_index];

    size_t json_offset;
    switch (DecodePacket(reading_buffer->data, reading_buffer->size, &json_offset)) {
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION:
        InterpretProjectDescriptionClientData(reading_buffer, bootstrapper, json_offset);
        AppendMessageToBroadcast(subscriber_broadcast, "PROJECT DESCRIPTION", "New project description recieved");
        break;
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_REQUEST:
        printf("Got a subscribe request\n");
        all_handles->types[fd_index] = HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION;
        DynamicBufferTrimLeft(reading_buffer, PACKET_HEADER_SIZE);
        break;
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_RESPONSE: {
        size_t null_terminator_index;
        if (FindNullTerminator(&reading_buffer->data[PACKET_HEADER_SIZE], reading_buffer->size - PACKET_HEADER_SIZE,
                               &null_terminator_index)) {
            null_terminator_index += PACKET_HEADER_SIZE;
            printf("Got a subscribe response, that's odd because I'm the server\n");
            DynamicBufferTrimLeft(reading_buffer, null_terminator_index);
        }
        break;
    }
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_INCOMPLETE:
        break;
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN:
        printf("Got complete nonsense, clearing the buffer...\n");
        DynamicBufferTrimLeft(reading_buffer, reading_buffer->size);
        break;
    }
}

static void RecieveClientSocketData(int client_sock, size_t fd_index, char* client_message,
                                    struct PollingHandles* all_handles, struct Bootstrapper* bootstrapper, int* running,
                                    struct DynamicStringArray* subscriber_broadcast) {
    errno = 0;
    int read_size = recv(client_sock, client_message, CLIENT_MESSAGE_READ_BUFFER_SIZE, 0);

    if (read_size > 0) {
        DynamicBufferAppend(&all_handles->reading_buffers[fd_index], client_message, read_size);
        if (client_message[0] == 'q') {
            printf("exit requested\n");
            *running = 0;
        }
        InterpretClientData(all_handles, fd_index, bootstrapper, subscriber_broadcast);
    } else if (read_size < 0) {
        fprintf(stderr, "recv failed: %s (%d)\n", strerror(errno), errno);
        Erase(all_handles, fd_index);
    } else {
        printf("Client disconnected\n");
        Erase(all_handles, fd_index);
    }
}

static int StartGDBServer(void* userdata) {
    struct BootstrapperUserdata* bootstrapper_userdata = (struct BootstrapperUserdata*)userdata;
    if (bootstrapper_userdata) {
        if (bootstrapper_userdata->gdbserver_pid != -1)
            return 1; // Already running
    }

    int pipefd[2];
    int pipefd_err[2];
    int p1 = pipe(pipefd);
    int p2 = pipe(pipefd_err);
    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd_err[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        close(pipefd_err[0]);
        close(pipefd_err[1]);
        char* args[] = {"gdbserver", "--help", NULL};
        char* env[] = {NULL};
        execve("/usr/bin/gdbserver", args, env);
    } else {
        close(pipefd[1]);
        close(pipefd_err[1]);
        printf("parent is sleeping...\n");

        char buffer[512];

        int bytes = 0;
        int bytes_err = 0;
        while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) > 0)
            printf("Output: (%.*s)\n", bytes, buffer);
        while ((bytes_err = read(pipefd_err[0], buffer, sizeof(buffer))) > 0)
            printf("Output (err): (%.*s)\n", bytes, buffer);
    };

    if (bootstrapper_userdata) {
        bootstrapper_userdata->gdbserver_pid = pid;
    }

    return 1;
}

#define STOPPING_WAIT_TIME_MS 1000
#define SLEEP_WAIT_TIME_MS 10
#define MAX_WAIT_LOOPS (STOPPING_WAIT_TIME_MS / SLEEP_WAIT_TIME_MS)

// First signals SIGTERM, then waits up to STOPPING_WAIT_TIME_MS for GDB server to stop
// When the GDB server has not stopped after STOPPING_WAIT_TIME_MS, SIGKILL is sent, and it is assumed that the GDB
// server will stop
static int StopGDBServer(void* userdata) {
    struct BootstrapperUserdata* bootstrapper_userdata = (struct BootstrapperUserdata*)userdata;
    if (bootstrapper_userdata) {
        if (bootstrapper_userdata->gdbserver_pid == -1)
            return 1;
        kill(bootstrapper_userdata->gdbserver_pid, SIGTERM);
        int status;
        int loops = 0;
        do {
            waitpid(bootstrapper_userdata->gdbserver_pid, &status, WNOHANG);

            sleep(SLEEP_WAIT_TIME_MS);
            ++loops;
        } while (!WIFEXITED(status) && !WIFSIGNALED(status) && loops <= MAX_WAIT_LOOPS);
        if (loops == MAX_WAIT_LOOPS)
            kill(bootstrapper_userdata->gdbserver_pid, SIGKILL);
        bootstrapper_userdata->gdbserver_pid = -1;
        return 1;
    }
    return 0;
}

static int FileExists(const char* file) { return access(file, F_OK) == 0; }
static int FileExists_Bound(const char* file, void* userdata) { return FileExists(file); }

static void CalculateFileHash(const char* file, char** hash, size_t* hash_size, void* userdata) {
    const char* dummy_hash = "abcd";
    *hash_size = strlen(dummy_hash);
    *hash = (char*)malloc(*hash_size + 1);
    strcpy(*hash, dummy_hash);
}

static void BindBootstrapper(struct Bootstrapper* bootstrapper, void* userdata) {
    bootstrapper->userdata = userdata;
    bootstrapper->startGDBServer = &StartGDBServer;
    bootstrapper->stopGDBServer = &StopGDBServer;
    bootstrapper->fileExists = &FileExists_Bound;
    bootstrapper->calculateHash = &CalculateFileHash;
    BootstrapperInit(bootstrapper);
}

static void CreatePollingHandlesStartingWithServerSocket(struct PollingHandles* all_handles, int socket_desc) {
    Init(all_handles);

    Append(all_handles, socket_desc, POLLIN, HANDLE_TYPE_SERVER_SOCKET);

    fcntl(socket_desc, F_SETFL, fcntl(socket_desc, F_GETFL, 0) | O_NONBLOCK);
}

static void ValidateMissingFiles(struct Bootstrapper* bootstrapper) {
    struct DynamicStringArray missing;
    DynamicStringArrayInit(&missing);
    ReportMissingFiles(bootstrapper, &missing);

    for (int i = 0; i < missing.size; ++i) {
        if (FileExists(missing.data[i]))
            UpdateFileActualHash(bootstrapper, missing.data[i]);
    }
    DynamicStringArrayDeinit(&missing);
}

static void ValidateMismatchingHashes(struct Bootstrapper* bootstrapper) {

    struct DynamicStringArray files, actual_hashes, wanted_hashes;
    DynamicStringArrayInit(&files);
    DynamicStringArrayInit(&actual_hashes);
    DynamicStringArrayInit(&wanted_hashes);
    ReportWantedVsActualHashes(bootstrapper, &files, &actual_hashes, &wanted_hashes);

    struct DynamicStringArray files_to_update;
    DynamicStringArrayInit(&files_to_update);

    for (int i = 0; i < files.size; ++i) {
        if (strcmp(actual_hashes.data[i], wanted_hashes.data[i]) != 0)
            DynamicStringArrayAppend(&files_to_update, files.data[i]);
    }

    UpdateFileActualHashes(bootstrapper, &files_to_update);

    DynamicStringArrayDeinit(&files_to_update);

    DynamicStringArrayDeinit(&files);
    DynamicStringArrayDeinit(&actual_hashes);
    DynamicStringArrayDeinit(&wanted_hashes);
}

static void ValidateMismatches(struct Bootstrapper* bootstrapper) {
    ValidateMissingFiles(bootstrapper);
    ValidateMismatchingHashes(bootstrapper);
}

static void PutBroadcastMessagesInSubscriptionBuffer(struct DynamicStringArray* subscriber_broadcast,
                                                     struct DynamicBuffer* subscription_buffer) {
    for (int i = 0; i < subscriber_broadcast->size; ++i) {
        uint8_t* header;
        size_t packet_size;
        MakeSubscriptionResponsePacketHeader(&header, &packet_size);
        DynamicBufferAppend(subscription_buffer, (char*)header, packet_size);
        free(header);
        DynamicBufferAppend(subscription_buffer, subscriber_broadcast->data[i],
                            strlen(subscriber_broadcast->data[i]) + 1);
    }
}

static void PutBroadcastMessagesInSubscriptionBuffers(struct PollingHandles* all_handles,
                                                      struct DynamicStringArray* subscriber_broadcast) {
    for (int i = 0; i < all_handles->size; ++i) {
        if (all_handles->types[i] == HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION)
            PutBroadcastMessagesInSubscriptionBuffer(subscriber_broadcast, &all_handles->writing_buffers[i]);
    }
}

static void SetPollWriteFlagsWhereWritebuffersHaveData(struct PollingHandles* polling_handles,
                                                       const struct DynamicStringArray* broadcast_buffer) {
    if (broadcast_buffer->size > 0) {

        for (size_t i = 0; i < polling_handles->size; ++i)
            polling_handles->pfds[i].events |= POLLOUT;
        return; // Write flags are all set, no need to check write buffers individually in the following code
    }

    for (size_t i = 0; i < polling_handles->size; ++i) {
        if (polling_handles->writing_buffers[i].size > 0)
            polling_handles->pfds[i].events |= POLLOUT;
    }
}

static void ClearPollWriteFlags(struct PollingHandles* polling_handles) {
    for (size_t i = 0; i < polling_handles->size; ++i) {
        if (polling_handles[i].writing_buffers->size > 0)
            polling_handles->pfds[i].events &= ~POLLOUT;
    }
}

// Returns true when the current poll result is invalidated
static int ReceivePollAware(struct PollingHandles* all_handles, size_t fd_index, char* client_message,
                            struct Bootstrapper* bootstrapper, int* running,
                            struct DynamicStringArray* subscriber_broadcast) {
    size_t current_size = all_handles->size;
    RecieveClientSocketData(all_handles->pfds[fd_index].fd, fd_index, client_message, all_handles, bootstrapper,
                            running, subscriber_broadcast);
    PutBroadcastMessagesInSubscriptionBuffers(all_handles, subscriber_broadcast);
    DynamicStringArrayClear(subscriber_broadcast);
    if (current_size != all_handles->size)
        return 1;
    return 0;
}

// Returns true when the current poll result is invalidated
static int WritePollAware(struct PollingHandles* all_handles, size_t fd_index) {
    errno = 0;
    int bytes_written = write(all_handles->pfds[fd_index].fd, all_handles->writing_buffers[fd_index].data,
                              all_handles->writing_buffers[fd_index].size);
    if (bytes_written > 0)
        DynamicBufferTrimLeft(&all_handles->writing_buffers[fd_index], bytes_written);
    else if (bytes_written == 0) {
        printf("Error writing: %s\n", strerror(errno));
        Erase(all_handles, fd_index);
        return 1;
    } else {
        printf("Client disconnected\n");
        Erase(all_handles, fd_index);
        return 1;
    }
    return 0;
}

#define POLL_TIMEOUT_MS 1000

static void StartRecievingData(int socket_desc, struct sockaddr_in* server) {
    listen(socket_desc, 3);

    printf("Waiting for incoming connections\n");

    struct PollingHandles all_handles;
    CreatePollingHandlesStartingWithServerSocket(&all_handles, socket_desc);

    struct DynamicStringArray subscriber_broadcast;
    DynamicStringArrayInit(&subscriber_broadcast);

    size_t write_amount = 0;
    char client_message[CLIENT_MESSAGE_READ_BUFFER_SIZE];

    struct Bootstrapper bootstrapper;
    BindBootstrapper(&bootstrapper, NULL);

    int running = 1;
    while (running) {
        ClearPollWriteFlags(&all_handles);
        SetPollWriteFlagsWhereWritebuffersHaveData(&all_handles, &subscriber_broadcast);
        int ready = poll(all_handles.pfds, all_handles.size, POLL_TIMEOUT_MS);
        if (ready > 0) {
            int poll_result_invalidated = 0;
            for (size_t fd_index = 0; fd_index < all_handles.size && !poll_result_invalidated; ++fd_index) {
                if (all_handles.pfds[fd_index].revents & POLLIN) {
                    switch (all_handles.types[fd_index]) {
                    case HANDLE_TYPE_SERVER_SOCKET:
                        AddClientSocket(all_handles.pfds[fd_index].fd, &all_handles);
                        // A new handle is added in all_handles, so further indexes might be invalid now
                        poll_result_invalidated = 1;
                        continue;
                    case HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION:
                    case HANDLE_TYPE_CLIENT_SOCKET: {
                        if (ReceivePollAware(&all_handles, fd_index, client_message, &bootstrapper, &running,
                                             &subscriber_broadcast)) {
                            poll_result_invalidated = 1;
                            continue;
                        }
                        break;
                    }
                    }
                }
                if (all_handles.pfds[fd_index].revents & POLLOUT) {
                    switch (all_handles.types[fd_index]) {
                    case HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION:
                        if (WritePollAware(&all_handles, fd_index)) {
                            poll_result_invalidated = 1;
                            continue;
                        }
                        break;
                    default:
                        break;
                    }
                }
            }

        } else if (ready < 0) {
            fprintf(stderr, "poll failed\n");
            break;
        } else {
            printf("I do nothing this time %lu\n", ++write_amount);
        }

        ValidateMismatches(&bootstrapper);
    }
    DynamicStringArrayDeinit(&subscriber_broadcast);
    Deinit(&all_handles);
    BootstrapperDeinit(&bootstrapper);
}

static void ReportSocketPort(int socket_desc) {
    struct sockaddr_in server;
    memset(&server, 0, sizeof(struct sockaddr_in));
    socklen_t length = sizeof(server);

    if (getsockname(socket_desc, (struct sockaddr*)&server, &length) == 0)
        printf("Server socket running on port: %d\n", ntohs(server.sin_port));
    else
        fprintf(stderr, "Something went wrong retrieving server socket name: %s\n", strerror(errno));
}

void StartEventDispatch(int port) {
    int socket_desc;
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_desc == -1) {
        fprintf(stderr, "Could not create socket\n");
        exit(1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0) {
        fprintf(stderr, "bind failed. Error\n");
        exit(1);
    }

    ReportSocketPort(socket_desc);

    StartRecievingData(socket_desc, &server);
}