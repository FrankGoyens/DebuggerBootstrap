#include "EventDispatch.h"

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

struct ReadingBuffer {
    char* data;
    size_t size;
    size_t capacity;
};

#define READING_BUFFER_INITIAL_SIZE 16

void ReadingBufferInit(struct ReadingBuffer* buffer) {
    buffer->size = 0;
    buffer->capacity = READING_BUFFER_INITIAL_SIZE;
    buffer->data = (char*)malloc(READING_BUFFER_INITIAL_SIZE);
}

void ReadingBufferDeinit(struct ReadingBuffer* buffer) { free(buffer->data); }

void _readingBufferExtend(struct ReadingBuffer* buffer, size_t minimal_new_size) {
    buffer->data = (char*)realloc(buffer->data, minimal_new_size * 2);
}

void ReadingBufferAppend(struct ReadingBuffer* buffer, const char* new_data, size_t new_data_size) {
    if (buffer->size + new_data_size >= buffer->capacity)
        _readingBufferExtend(buffer, buffer->size + new_data_size);

    memcpy(buffer->data + buffer->size, new_data, new_data_size);
    buffer->size += new_data_size;
}

void ReadingBufferTrimLeft(struct ReadingBuffer* buffer, size_t trim_amount) {
    if (trim_amount = buffer->size) {
        buffer->size = 0;
        return;
    }

    const size_t remainder = buffer->size - trim_amount;
    for (size_t i = 0; i < remainder; ++i)
        buffer->data[i] = buffer->data[i + trim_amount];
    buffer->size = trim_amount;
}

struct PollingHandles {
    struct pollfd* pfds;
    enum HandleType* types;
    struct ReadingBuffer* reading_buffers;
    size_t size, capacity;
};

static void Init(struct PollingHandles* handles) {
    handles->size = 0;
    handles->capacity = 1;
    handles->pfds = (struct pollfd*)calloc(sizeof(struct pollfd), handles->capacity);
    handles->types = (enum HandleType*)calloc(sizeof(enum HandleType), handles->capacity);
    handles->reading_buffers = (struct ReadingBuffer*)malloc(sizeof(struct ReadingBuffer) * handles->capacity);
}

static void Deinit(struct PollingHandles* handles) {
    free(handles->pfds);
    free(handles->types);
    for (size_t i = 0; i < handles->capacity; ++i)
        ReadingBufferDeinit(&handles->reading_buffers[i]);
    free(handles->reading_buffers);
}

static void _extend(struct PollingHandles* handles) {
    handles->capacity *= 2;
    handles->pfds = realloc(handles->pfds, handles->capacity * sizeof(struct pollfd));
    handles->types = realloc(handles->types, handles->capacity * sizeof(enum HandleType));
    memset(handles->pfds + handles->size, 0, handles->size * sizeof(struct pollfd));
    memset(handles->types + handles->size, 0, handles->size * sizeof(enum HandleType));
    handles->reading_buffers = realloc(handles->reading_buffers, handles->capacity * sizeof(struct ReadingBuffer));
}

static void Append(struct PollingHandles* handles, int fd, short events, enum HandleType type) {
    if (handles->size == handles->capacity)
        _extend(handles);

    handles->pfds[handles->size].fd = fd;
    handles->pfds[handles->size].events = events;
    handles->types[handles->size] = type;
    ReadingBufferInit(&handles->reading_buffers[handles->size]);
    ++handles->size;
}

static void Erase(struct PollingHandles* handles, size_t at) {
    if (at < 0 || at >= handles->size)
        return;
    for (size_t i = at + 1; i < handles->size; ++i) {
        handles->pfds[i - 1] = handles->pfds[i];
        handles->types[i - 1] = handles->types[i];
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

// This will remove the data that is successfully interpreted
static void InterpretClientData(struct ReadingBuffer* reading_buffer, struct PollingHandles* all_handles,
                                struct Bootstrapper* bootstrapper) {
    size_t json_offset;
    switch (DecodePacket(reading_buffer->data, reading_buffer->size, &json_offset)) {
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION: {
        const size_t null_terminator_index =
            FindNullTerminator(&reading_buffer->data[json_offset], reading_buffer->size);
        if (null_terminator_index < reading_buffer->size) {
            struct ProjectDescription description;
            if (ProjectDescriptionLoadFromJSON(&reading_buffer->data[json_offset], &description)) {
                printf("I got a valid project description!\n");
                ReadingBufferTrimLeft(reading_buffer, null_terminator_index);
                ReceiveNewProjectDescription(bootstrapper, &description);
            }
        }
        break;
    }
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_REQUEST:
        printf("Got a subscribe request\n");
        ReadingBufferTrimLeft(reading_buffer, PACKET_HEADER_SIZE);
        break;
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_RESPONSE:
        printf("Got a subscribe response, that's odd because I'm the server\n");
        ReadingBufferTrimLeft(reading_buffer, PACKET_HEADER_SIZE);
        break;
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_INCOMPLETE:
        break;
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN:
        printf("Got complete nonsense, clearing the buffer...\n");
        ReadingBufferTrimLeft(reading_buffer, reading_buffer->size);
        break;
    }
}

static void RecieveClientSocketData(int client_sock, size_t fd_index, char* client_message,
                                    struct PollingHandles* all_handles, struct Bootstrapper* bootstrapper, int* running,
                                    size_t* write_amount) {
    size_t read_size;

    read_size = recv(client_sock, client_message, CLIENT_MESSAGE_READ_BUFFER_SIZE, 0);

    if (read_size > 0) {
        ReadingBufferAppend(&all_handles->reading_buffers[fd_index], client_message, read_size);
        if (client_message[0] == 'q') {
            printf("exit requested\n");
            *running = 0;
        }
        printf("I will write something now %lu\n", ++*write_amount);
        write(client_sock, client_message, strlen(client_message));
        InterpretClientData(&all_handles->reading_buffers[fd_index], all_handles, bootstrapper);
    } else if (read_size < 0) {
        fprintf(stderr, "recv failed\n");
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

static int FileExists(const char* file, void* userdata) { return access(file, F_OK) == 0; }

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
    bootstrapper->fileExists = &FileExists;
    bootstrapper->calculateHash = &CalculateFileHash;
    BootstrapperInit(bootstrapper);
}

static void CreatePollingHandlesStartingWithServerSocket(struct PollingHandles* all_handles, int socket_desc) {
    Init(all_handles);

    Append(all_handles, socket_desc, POLLIN, HANDLE_TYPE_SERVER_SOCKET);

    fcntl(socket_desc, F_SETFL, fcntl(socket_desc, F_GETFL, 0) | O_NONBLOCK);
}

static void StartRecievingData(int socket_desc, struct sockaddr_in* server) {
    listen(socket_desc, 3);

    printf("Waiting for incoming connections\n");

    struct PollingHandles all_handles;
    CreatePollingHandlesStartingWithServerSocket(&all_handles, socket_desc);

    size_t write_amount = 0;
    char client_message[CLIENT_MESSAGE_READ_BUFFER_SIZE];

    struct Bootstrapper bootstrapper;
    BindBootstrapper(&bootstrapper, NULL);

    int running = 1;
    while (running) {
        int ready = poll(all_handles.pfds, all_handles.size, 1000);
        if (ready > 0) {
            for (size_t fd_index = 0; fd_index < all_handles.size; ++fd_index) {
                if (all_handles.pfds[fd_index].revents & POLLIN) {
                    switch (all_handles.types[fd_index]) {
                    case HANDLE_TYPE_SERVER_SOCKET:
                        AddClientSocket(all_handles.pfds[fd_index].fd, &all_handles);
                        break;
                    case HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION:
                    case HANDLE_TYPE_CLIENT_SOCKET:
                        RecieveClientSocketData(all_handles.pfds[fd_index].fd, fd_index, client_message, &all_handles,
                                                &bootstrapper, &running, &write_amount);

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
    }
    Deinit(&all_handles);
    BootstrapperDeinit(&bootstrapper);
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

    StartRecievingData(socket_desc, &server);
}