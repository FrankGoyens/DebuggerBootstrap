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
#include "GDBServerStartStop.h"
#include "ProjectDescription.h"
#include "ProjectDescription_json.h"
#include "SubscriberUpdate.h"
#include "protocol/Protocol.h"

#define CLIENT_MESSAGE_READ_BUFFER_SIZE 128

enum HandleType {
    HANDLE_TYPE_SERVER_SOCKET,
    HANDLE_TYPE_CLIENT_SOCKET,
    HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION, // This client socket will recieve status updates as well
    HANDLE_TYPE_DEBUGGER_STDOUT,
    HANDLE_TYPE_DEBUGGER_STDERR
};

typedef struct {
    GDBInstance gdbserver_instance;
} BoundBootstrapperParameters;

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} DynamicBuffer;

#define DYNAMIC_BUFFER_INITIAL_SIZE 16

void DynamicBufferInit(DynamicBuffer* buffer) {
    buffer->size = 0;
    buffer->capacity = DYNAMIC_BUFFER_INITIAL_SIZE;
    buffer->data = (char*)malloc(DYNAMIC_BUFFER_INITIAL_SIZE);
}

void DynamicBufferDeinit(DynamicBuffer* buffer) { free(buffer->data); }

void _dynamicBufferExtend(DynamicBuffer* buffer, size_t minimal_new_size) {
    const size_t new_size = minimal_new_size * 2;
    buffer->data = (char*)realloc(buffer->data, new_size);
    buffer->capacity = new_size;
}

void DynamicBufferAppend(DynamicBuffer* buffer, const char* new_data, size_t new_data_size) {
    if (buffer->size + new_data_size >= buffer->capacity)
        _dynamicBufferExtend(buffer, buffer->size + new_data_size);

    memcpy(buffer->data + buffer->size, new_data, new_data_size);
    buffer->size += new_data_size;
}

void DynamicBufferTrimLeft(DynamicBuffer* buffer, size_t trim_amount) {
    if (trim_amount == buffer->size) {
        buffer->size = 0;
        return;
    }

    const size_t remainder = buffer->size - trim_amount;
    for (size_t i = 0; i < remainder; ++i)
        buffer->data[i] = buffer->data[i + trim_amount];
    buffer->size -= trim_amount;
}

typedef struct {
    struct pollfd* pfds;
    enum HandleType* types;
    DynamicBuffer* reading_buffers;
    DynamicBuffer* writing_buffers;
    size_t size, capacity;
} PollingHandles;

static void Init(PollingHandles* handles) {
    handles->size = 0;
    handles->capacity = 1;
    handles->pfds = (struct pollfd*)calloc(sizeof(struct pollfd), handles->capacity);
    handles->types = (enum HandleType*)calloc(sizeof(enum HandleType), handles->capacity);
    handles->reading_buffers = (DynamicBuffer*)malloc(sizeof(DynamicBuffer) * handles->capacity);
    handles->writing_buffers = (DynamicBuffer*)malloc(sizeof(DynamicBuffer) * handles->capacity);
}

static void FreeDynamicBufferArray(DynamicBuffer* dynamic_buffers, size_t n) {
    for (size_t i = 0; i < n; ++i)
        DynamicBufferDeinit(&dynamic_buffers[i]);
    free(dynamic_buffers);
}

static void Deinit(PollingHandles* handles) {
    free(handles->pfds);
    free(handles->types);
    FreeDynamicBufferArray(handles->reading_buffers, handles->capacity);
    FreeDynamicBufferArray(handles->writing_buffers, handles->capacity);
}

static void _extend(PollingHandles* handles) {
    handles->capacity *= 2;
    handles->pfds = realloc(handles->pfds, handles->capacity * sizeof(struct pollfd));
    handles->types = realloc(handles->types, handles->capacity * sizeof(enum HandleType));
    memset(handles->pfds + handles->size, 0, handles->size * sizeof(struct pollfd));
    memset(handles->types + handles->size, 0, handles->size * sizeof(enum HandleType));
    handles->reading_buffers = realloc(handles->reading_buffers, handles->capacity * sizeof(DynamicBuffer));
    handles->writing_buffers = realloc(handles->writing_buffers, handles->capacity * sizeof(DynamicBuffer));
}

static void Append(PollingHandles* handles, int fd, short events, enum HandleType type) {
    if (handles->size == handles->capacity)
        _extend(handles);

    handles->pfds[handles->size].fd = fd;
    handles->pfds[handles->size].events = events;
    handles->types[handles->size] = type;
    DynamicBufferInit(&handles->reading_buffers[handles->size]);
    DynamicBufferInit(&handles->writing_buffers[handles->size]);
    ++handles->size;
}

static void Erase(PollingHandles* handles, size_t at) {
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

static void AddClientSocket(int socket_desc, PollingHandles* all_handles) {
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

static void AddDebuggerHandlesToPollingHandles(PollingHandles* all_handles, int debugger_stdout, int debugger_stderr) {
    Append(all_handles, debugger_stdout, POLLIN, HANDLE_TYPE_DEBUGGER_STDOUT);
    Append(all_handles, debugger_stderr, POLLIN, HANDLE_TYPE_DEBUGGER_STDERR);

    fcntl(debugger_stdout, F_SETFL, fcntl(debugger_stdout, F_GETFL, 0) | O_NONBLOCK);
    fcntl(debugger_stderr, F_SETFL, fcntl(debugger_stderr, F_GETFL, 0) | O_NONBLOCK);
}

static void AddDebuggerHandlesToPollingHandlesIfRunning(PollingHandles* all_handles, Bootstrapper* bootstrapper) {

    BoundBootstrapperParameters* userdata = (BoundBootstrapperParameters*)bootstrapper->userdata;
    if (!userdata)
        return;

    if (userdata->gdbserver_instance.pid == NO_PID)
        return;

    for (int fd_index = 0; fd_index < all_handles->size; ++fd_index) {
        if (all_handles->types[fd_index] == HANDLE_TYPE_DEBUGGER_STDOUT ||
            all_handles->types[fd_index] == HANDLE_TYPE_DEBUGGER_STDERR) {
            fprintf(stderr,
                    "FIXME: The debugger handles are already present, there should only be one set of handles.");
            return;
        }
    }
    AddDebuggerHandlesToPollingHandles(all_handles, userdata->gdbserver_instance.stdout_handle,
                                       userdata->gdbserver_instance.stderr_handle);
}

// When not found, returns all_handles->size
static int FindFirstItemWithType(PollingHandles* all_handles, enum HandleType handle_type) {
    for (int fd_index = 0; fd_index < all_handles->size; ++fd_index) {
        if (all_handles->types[fd_index] == handle_type)
            return fd_index;
    }

    return all_handles->size;
}

static void ExpectPresentAndErase(PollingHandles* all_handles, enum HandleType type,
                                  const char* message_when_not_present) {
    const int stdout_index = FindFirstItemWithType(all_handles, type);
    if (stdout_index == all_handles->size)
        fprintf(stderr, "%s", message_when_not_present);
    else
        Erase(all_handles, stdout_index);
}

static void ExpectAndEraseDebuggerHandles(PollingHandles* all_handles) {
    ExpectPresentAndErase(all_handles, HANDLE_TYPE_DEBUGGER_STDOUT,
                          "FIXME: The debugger is running, but its stdout handle is not present\n");
    ExpectPresentAndErase(all_handles, HANDLE_TYPE_DEBUGGER_STDERR,
                          "FIXME: The debugger is running, but its stderr handle is not present\n");
}

static void RemoveDebuggerHandlesFromPollingHandlesIfNotRunning(PollingHandles* all_handles,
                                                                Bootstrapper* bootstrapper) {
    BoundBootstrapperParameters* userdata = (BoundBootstrapperParameters*)bootstrapper->userdata;
    if (!userdata)
        return;

    if (userdata->gdbserver_instance.pid != NO_PID)
        return;

    ExpectAndEraseDebuggerHandles(all_handles);
}

// Returns True when data was successfully interpreted
static int InterpretProjectDescriptionClientData(DynamicBuffer* reading_buffer, Bootstrapper* bootstrapper,
                                                 size_t json_offset) {
    if (json_offset > reading_buffer->size)
        return 0;
    size_t null_terminator_index;
    if (FindNullTerminator(&reading_buffer->data[json_offset], reading_buffer->size - json_offset,
                           &null_terminator_index)) {
        null_terminator_index += json_offset;

        ProjectDescription description;
        if (ProjectDescriptionLoadFromJSON(&reading_buffer->data[json_offset], &description)) {
            printf("I got a valid project description!\n");
            DynamicBufferTrimLeft(reading_buffer, null_terminator_index + 1);
            const int debugger_is_running = IsGDBServerUp(bootstrapper);
            ReceiveNewProjectDescription(bootstrapper, &description);

            ProjectDescriptionDeinit(&description);
            return 1;
        }
    }
    return 0;
}

static void AppendMessageToBroadcast(DynamicStringArray* subscriber_broadcast, const char* tag, const char* message) {
    printf("Broadcasting:\nTAG=%s\nMESSAGE=%s\n", tag, message);
    char* encoded_message = EncodeSubscriberUpdateMessage(tag, message);
    DynamicStringArrayAppend(subscriber_broadcast, encoded_message);
    free(encoded_message);
}

// This will remove the data that is successfully interpreted
// Returns True when data was successfully interpreted
// When the data is unrecognizable, the buffer may be cleared without returning True
// When the data is incomplete, the buffer will not be cleared and False is returned
static int InterpretClientData(PollingHandles* all_handles, size_t fd_index, Bootstrapper* bootstrapper,
                               DynamicStringArray* subscriber_broadcast) {
    DynamicBuffer* reading_buffer = &all_handles->reading_buffers[fd_index];

    size_t json_offset;
    switch (DecodePacket(reading_buffer->data, reading_buffer->size, &json_offset)) {
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_PROJECT_DESCRIPTION: {
        const int result = InterpretProjectDescriptionClientData(reading_buffer, bootstrapper, json_offset);
        AppendMessageToBroadcast(subscriber_broadcast, "PROJECT DESCRIPTION", "New project description recieved");
        return result;
    }
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_REQUEST:
        printf("Got a subscribe request\n");
        all_handles->types[fd_index] = HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION;
        DynamicBufferTrimLeft(reading_buffer, PACKET_HEADER_SIZE);
        return 1;
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_SUBSCRIBE_RESPONSE: {
        size_t null_terminator_index;
        if (FindNullTerminator(&reading_buffer->data[PACKET_HEADER_SIZE], reading_buffer->size - PACKET_HEADER_SIZE,
                               &null_terminator_index)) {
            null_terminator_index += PACKET_HEADER_SIZE;
            printf("Got a subscribe response, that's odd because I'm the server\n");
            DynamicBufferTrimLeft(reading_buffer, null_terminator_index);
            return 1;
        }
        return 0;
    }
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_FORCE_DEBUGGER_START:
        printf("Got request to force start debugger\n");
        // The upper level function would check whether the debugger started/stopped
        (void)ForceStartDebugger(bootstrapper);
        DynamicBufferTrimLeft(reading_buffer, PACKET_HEADER_SIZE);
        return 1;
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_FORCE_DEBUGGER_STOP:
        printf("Got request to force stop debugger\n");
        // The upper level function would check whether the debugger started/stopped
        (void)ForceStopDebugger(bootstrapper);
        DynamicBufferTrimLeft(reading_buffer, PACKET_HEADER_SIZE);
        return 1;

    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_INCOMPLETE:
        return 0;
    case DEBUGGER_BOOTSTRAP_PROTOCOL_PACKET_TYPE_UNKNOWN:
        printf("Got complete nonsense, clearing the buffer...\n");
        DynamicBufferTrimLeft(reading_buffer, reading_buffer->size);
        return 0;
    }
}

static void RecieveClientSocketData(int client_sock, size_t fd_index, char* client_message, PollingHandles* all_handles,
                                    Bootstrapper* bootstrapper, DynamicStringArray* subscriber_broadcast) {
    errno = 0;
    int read_size = recv(client_sock, client_message, CLIENT_MESSAGE_READ_BUFFER_SIZE, 0);

    if (read_size > 0) {
        DynamicBufferAppend(&all_handles->reading_buffers[fd_index], client_message, read_size);
        while (InterpretClientData(all_handles, fd_index, bootstrapper, subscriber_broadcast)) {
        }
    } else if (read_size < 0) {
        close(client_sock);
        fprintf(stderr, "recv failed: %s (%d)\n", strerror(errno), errno);
        Erase(all_handles, fd_index);
    } else {
        close(client_sock);
        printf("Client disconnected\n");
        Erase(all_handles, fd_index);
    }
}

static int FileExists(const char* file) { return access(file, F_OK) == 0; }
static int FileExists_Bound(const char* file, void* userdata) { return FileExists(file); }

static void CalculateFileHash(const char* file, char** hash, size_t* hash_size, void* userdata) {
    const char* dummy_hash = "abcd";
    *hash_size = strlen(dummy_hash);
    *hash = (char*)malloc(*hash_size + 1);
    strcpy(*hash, dummy_hash);
}

static int StartGDBServer_Bound(void* userdata) {
    BoundBootstrapperParameters* bootstrapper_userdata = (BoundBootstrapperParameters*)userdata;
    if (!bootstrapper_userdata) {
        return 1;
    }
    return StartGDBServer(&bootstrapper_userdata->gdbserver_instance);
}

static int StopGDBServer_Bound(void* userdata) {
    BoundBootstrapperParameters* bootstrapper_userdata = (BoundBootstrapperParameters*)userdata;
    if (!bootstrapper_userdata) {
        return 1;
    }
    return StopGDBServer(&bootstrapper_userdata->gdbserver_instance);
}

static void BindBootstrapper(Bootstrapper* bootstrapper, void* userdata) {
    bootstrapper->userdata = userdata;
    bootstrapper->startGDBServer = &StartGDBServer_Bound;
    bootstrapper->stopGDBServer = &StopGDBServer_Bound;
    bootstrapper->fileExists = &FileExists_Bound;
    bootstrapper->calculateHash = &CalculateFileHash;
    BootstrapperInit(bootstrapper);
}

static void CreatePollingHandlesStartingWithServerSocket(PollingHandles* all_handles, int socket_desc) {
    Init(all_handles);

    Append(all_handles, socket_desc, POLLIN, HANDLE_TYPE_SERVER_SOCKET);

    fcntl(socket_desc, F_SETFL, fcntl(socket_desc, F_GETFL, 0) | O_NONBLOCK);
}

static void ValidateMissingFiles(Bootstrapper* bootstrapper) {
    DynamicStringArray missing;
    DynamicStringArrayInit(&missing);
    ReportMissingFiles(bootstrapper, &missing);

    for (int i = 0; i < missing.size; ++i) {
        if (FileExists(missing.data[i]))
            UpdateFileActualHash(bootstrapper, missing.data[i]);
    }
    DynamicStringArrayDeinit(&missing);
}

static void ValidateMismatchingHashes(Bootstrapper* bootstrapper) {

    DynamicStringArray files, actual_hashes, wanted_hashes;
    DynamicStringArrayInit(&files);
    DynamicStringArrayInit(&actual_hashes);
    DynamicStringArrayInit(&wanted_hashes);
    ReportWantedVsActualHashes(bootstrapper, &files, &actual_hashes, &wanted_hashes);

    DynamicStringArray files_to_update;
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

static void ValidateMismatches(Bootstrapper* bootstrapper) {
    ValidateMissingFiles(bootstrapper);
    ValidateMismatchingHashes(bootstrapper);
}

static void PutBroadcastMessagesInSubscriptionBuffer(DynamicStringArray* subscriber_broadcast,
                                                     DynamicBuffer* subscription_buffer) {
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

static void PutBroadcastMessagesInSubscriptionBuffers(PollingHandles* all_handles,
                                                      DynamicStringArray* subscriber_broadcast) {
    for (int i = 0; i < all_handles->size; ++i) {
        if (all_handles->types[i] == HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION)
            PutBroadcastMessagesInSubscriptionBuffer(subscriber_broadcast, &all_handles->writing_buffers[i]);
    }
}

static void SetPollWriteFlagsWhereWritebuffersHaveData(PollingHandles* polling_handles,
                                                       const DynamicStringArray* broadcast_buffer) {

    for (size_t i = 0; i < polling_handles->size; ++i) {
        if (polling_handles->writing_buffers[i].size > 0)
            polling_handles->pfds[i].events |= POLLOUT;
    }
}

static void ClearPollWriteFlags(PollingHandles* polling_handles) {
    for (size_t i = 0; i < polling_handles->size; ++i) {
        polling_handles->pfds[i].events &= ~POLLOUT;
    }
}

// Actually checks whether the PID is set, instead of relying on the bootstrapper's perspective
static int DebuggerProcessIsRunning(Bootstrapper* bootstrapper) {
    BoundBootstrapperParameters* bootstrapper_userdata = (BoundBootstrapperParameters*)bootstrapper->userdata;
    if (!bootstrapper_userdata)
        return 0;
    return bootstrapper_userdata->gdbserver_instance.pid != NO_PID;
}

// Returns true when the current poll result is invalidated
static int ReceivePollAware(PollingHandles* all_handles, size_t fd_index, char* client_message,
                            Bootstrapper* bootstrapper, DynamicStringArray* subscriber_broadcast) {
    size_t current_size = all_handles->size;
    const int debugger_is_running = DebuggerProcessIsRunning(bootstrapper);

    RecieveClientSocketData(all_handles->pfds[fd_index].fd, fd_index, client_message, all_handles, bootstrapper,
                            subscriber_broadcast);

    if ((debugger_is_running != DebuggerProcessIsRunning(bootstrapper))) {
        AddDebuggerHandlesToPollingHandlesIfRunning(all_handles, bootstrapper);
        RemoveDebuggerHandlesFromPollingHandlesIfNotRunning(all_handles, bootstrapper);
        return 1;
    }

    if (current_size != all_handles->size)
        return 1;

    return 0;
}

// Returns true when the current poll result is invalidated
static int WritePollAware(PollingHandles* all_handles, size_t fd_index) {
    const int fd = all_handles->pfds[fd_index].fd;
    errno = 0;
    int bytes_written =
        write(fd, all_handles->writing_buffers[fd_index].data, all_handles->writing_buffers[fd_index].size);
    if (bytes_written > 0)
        DynamicBufferTrimLeft(&all_handles->writing_buffers[fd_index], bytes_written);
    else if (bytes_written == 0) {
        close(fd);
        printf("Client disconnected\n");
        Erase(all_handles, fd_index);
        return 1;
    } else {
        close(fd);
        printf("Error writing: %s\n", strerror(errno));
        Erase(all_handles, fd_index);
        return 1;
    }
    return 0;
}

static void PutDebuggerArgsInDebuggerInstance(const DebuggerParameters* parameters, GDBInstance* instance) {
    DynamicStringArrayCopy(&parameters->debugger_args, &instance->debugger_args);
}

// The result should be freed
static char* MakeDebuggerOutputTag(const char* human_readable_handle_name) {
    const char* debugger_name = "GDB ";
    const size_t debugger_name_length = strlen(debugger_name);
    char* tag = calloc(debugger_name_length + strlen(human_readable_handle_name) + 1, sizeof(char));
    strcpy(tag, debugger_name);
    strcpy(tag + debugger_name_length, human_readable_handle_name);
    return tag;
}

// The result should be freed
static char* MakeNullterminatedStringFromBuffer(char* buffer, size_t buffer_size) {
    char* null_terminated_string = calloc(buffer_size + 1, sizeof(char));
    memcpy(null_terminated_string, buffer, buffer_size);
    return null_terminated_string;
}

static void PutDataAsMessageIntoBroadcast(DynamicStringArray* subscriber_broadcast, char* data, size_t data_size,
                                          const char* human_readable_handle_name) {
    char* tag = MakeDebuggerOutputTag(human_readable_handle_name);
    char* message = MakeNullterminatedStringFromBuffer(data, data_size);
    AppendMessageToBroadcast(subscriber_broadcast, tag, message);
    free(tag);
    free(message);
}

// Cleans up the debugger handles from all the high level objects
static void CleanupDebuggerInstance(PollingHandles* all_handles, Bootstrapper* bootstrapper) {
    IndicateDebuggerHasStopped(bootstrapper);
    BoundBootstrapperParameters* userdata = (BoundBootstrapperParameters*)(bootstrapper->userdata);
    if (!userdata)
        return;

    ExpectAndEraseDebuggerHandles(all_handles);
    GDBInstanceClear(&userdata->gdbserver_instance);
}

// Returns TRUE when the polling handles are changed (so the current polling iteration becomes invalid)
static int PollAwareBroadcastDebuggerOutput(PollingHandles* all_handles, int fd_index, Bootstrapper* bootstrapper,
                                            DynamicStringArray* subscriber_broadcast, char* client_message,
                                            const char* human_readable_handle_name) {
    const int fd = all_handles->pfds[fd_index].fd;
    errno = 0;
    int bytes_read = read(fd, client_message, CLIENT_MESSAGE_READ_BUFFER_SIZE);
    if (bytes_read > 0) {
        PutDataAsMessageIntoBroadcast(subscriber_broadcast, client_message, (size_t)bytes_read,
                                      human_readable_handle_name);
    } else if (bytes_read == 0) {
        CleanupDebuggerInstance(all_handles, bootstrapper);
        return 1;
    } else {
        CleanupDebuggerInstance(all_handles, bootstrapper);
        fprintf(stderr, "Error reading debugger %s: %s\n", human_readable_handle_name, strerror(errno));
        return 1;
    }

    return 0;
}

// See 'PollAwareBroadcastDebuggerOutput' comment
static int PollAwareBroadcastDebuggerStdout(PollingHandles* all_handles, int fd_index, Bootstrapper* bootstrapper,
                                            DynamicStringArray* subscriber_broadcast, char* client_message) {
    return PollAwareBroadcastDebuggerOutput(all_handles, fd_index, bootstrapper, subscriber_broadcast, client_message,
                                            "stdout");
}

// See 'PollAwareBroadcastDebuggerOutput' comment
static int PollAwareBroadcastDebuggerStderr(PollingHandles* all_handles, int fd_index, Bootstrapper* bootstrapper,
                                            DynamicStringArray* subscriber_broadcast, char* client_message) {
    return PollAwareBroadcastDebuggerOutput(all_handles, fd_index, bootstrapper, subscriber_broadcast, client_message,
                                            "stderr");
}

typedef struct {
    PollingHandles all_handles;
    DynamicStringArray subscriber_broadcast;
    size_t idle_counter; // Used for logging a message when the poll exits through its timeout
    char client_message[CLIENT_MESSAGE_READ_BUFFER_SIZE]; // A buffer used for reading data from poll handles
    Bootstrapper bootstrapper;
    BoundBootstrapperParameters bound_bootstrapper_parameters;
} ToplevelPolling;

static void InitToplevelPolling(ToplevelPolling* toplevel_polling, int socket_desc,
                                DebuggerParameters* debugger_parameters) {
    CreatePollingHandlesStartingWithServerSocket(&toplevel_polling->all_handles, socket_desc);

    DynamicStringArrayInit(&toplevel_polling->subscriber_broadcast);

    toplevel_polling->idle_counter = 0;
    GDBInstanceInit(&toplevel_polling->bound_bootstrapper_parameters.gdbserver_instance,
                    debugger_parameters->debugger_path);
    BindBootstrapper(&toplevel_polling->bootstrapper, &toplevel_polling->bound_bootstrapper_parameters);
}

static void DeinitToplevelPolling(ToplevelPolling* toplevel_polling) {
    GDBInstanceDeinit(&toplevel_polling->bound_bootstrapper_parameters.gdbserver_instance);
    DynamicStringArrayDeinit(&toplevel_polling->subscriber_broadcast);
    Deinit(&toplevel_polling->all_handles);
    BootstrapperDeinit(&toplevel_polling->bootstrapper);
}

// Returns TRUE when the poll result is invalidated
static int DoPollIn(ToplevelPolling* toplevel_polling, size_t fd_index) {
    PollingHandles* all_handles = &toplevel_polling->all_handles;
    switch (all_handles->types[fd_index]) {
    case HANDLE_TYPE_SERVER_SOCKET:
        AddClientSocket(all_handles->pfds[fd_index].fd, all_handles);
        // A new handle is added in all_handles, so further indexes might be invalid now
        return 1;
    case HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION:
    case HANDLE_TYPE_CLIENT_SOCKET: {
        if (ReceivePollAware(all_handles, fd_index, toplevel_polling->client_message, &toplevel_polling->bootstrapper,
                             &toplevel_polling->subscriber_broadcast)) {
            return 1;
        }
        break;
    }
    case HANDLE_TYPE_DEBUGGER_STDOUT:
        if (PollAwareBroadcastDebuggerStdout(all_handles, fd_index, &toplevel_polling->bootstrapper,
                                             &toplevel_polling->subscriber_broadcast,
                                             toplevel_polling->client_message)) {
            return 1;
        }
        break;
    case HANDLE_TYPE_DEBUGGER_STDERR:
        if (PollAwareBroadcastDebuggerStderr(all_handles, fd_index, &toplevel_polling->bootstrapper,
                                             &toplevel_polling->subscriber_broadcast,
                                             toplevel_polling->client_message)) {
            return 1;
        }
        break;
    }

    PutBroadcastMessagesInSubscriptionBuffers(all_handles, &toplevel_polling->subscriber_broadcast);
    DynamicStringArrayClear(&toplevel_polling->subscriber_broadcast);
    return 0;
}

// Returns TRUE when the poll result is invalidated
static int DoPollOut(PollingHandles* all_handles, size_t fd_index) {
    switch (all_handles->types[fd_index]) {
    case HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION:
        if (WritePollAware(all_handles, fd_index)) {
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

// Returns TRUE when the poll result is invalidated
static int DoPollHup(PollingHandles* all_handles, Bootstrapper* bootstrapper, size_t fd_index) {
    if (all_handles->types[fd_index] != HANDLE_TYPE_DEBUGGER_STDOUT &&
        all_handles->types[fd_index] != HANDLE_TYPE_DEBUGGER_STDERR) {
        fprintf(stderr, "FIXME: When polling, a POLLHUP has occurred on a non debugger fd. This is not "
                        "handled because this is not expected to happen.\n");
    } else {
        CleanupDebuggerInstance(all_handles, bootstrapper);
        return 1;
    }
    return 0;
}

static void PollIteration(int ready, ToplevelPolling* toplevel_polling, int* running) {
    if (ready > 0) {
        int poll_result_invalidated = 0;
        for (size_t fd_index = 0; fd_index < toplevel_polling->all_handles.size && !poll_result_invalidated;
             ++fd_index) {
            if (toplevel_polling->all_handles.pfds[fd_index].revents & POLLIN) {
                if (DoPollIn(toplevel_polling, fd_index))
                    break;
            }
            if (toplevel_polling->all_handles.pfds[fd_index].revents & POLLOUT) {
                if (DoPollOut(&toplevel_polling->all_handles, fd_index))
                    break;
            }
            if (toplevel_polling->all_handles.pfds[fd_index].revents & POLLHUP) {
                if (DoPollHup(&toplevel_polling->all_handles, &toplevel_polling->bootstrapper, fd_index))
                    break;
            }
            if (toplevel_polling->all_handles.pfds[fd_index].revents & POLLERR) {
                fprintf(stderr, "FIXME: When polling, a POLLERR has occurred. This is not handled because this is not "
                                "expected to happen.\n");
            }
            if (toplevel_polling->all_handles.pfds[fd_index].revents & POLLNVAL) {
                fprintf(stderr, "FIXME: When polling, a POLLNVAL has occurred. This is not handled because this is not "
                                "expected to happen.\n");
                Erase(&toplevel_polling->all_handles, fd_index);
                break;
            }
        }

    } else if (ready < 0) {
        fprintf(stderr, "poll failed\n");
        *running = 0;
    } else {
        printf("I do nothing this time %lu\n", ++toplevel_polling->idle_counter);
    }
}

#define POLL_TIMEOUT_MS 1000

static void StartRecievingData(int socket_desc, struct sockaddr_in* server, DebuggerParameters* debugger_parameters) {
    listen(socket_desc, 3);

    printf("Waiting for incoming connections\n");

    ToplevelPolling toplevel_polling;
    InitToplevelPolling(&toplevel_polling, socket_desc, debugger_parameters);

    int running = 1;
    while (running) {
        ClearPollWriteFlags(&toplevel_polling.all_handles);
        SetPollWriteFlagsWhereWritebuffersHaveData(&toplevel_polling.all_handles,
                                                   &toplevel_polling.subscriber_broadcast);
        int ready = poll(toplevel_polling.all_handles.pfds, toplevel_polling.all_handles.size, POLL_TIMEOUT_MS);
        PollIteration(ready, &toplevel_polling, &running);

        ValidateMismatches(&toplevel_polling.bootstrapper);
    }
    DeinitToplevelPolling(&toplevel_polling);
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

void StartEventDispatch(int port, DebuggerParameters* debugger_parameters) {
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

    StartRecievingData(socket_desc, &server, debugger_parameters);
}