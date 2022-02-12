#include "EventDispatch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>

enum HandleType {
    HANDLE_TYPE_CLIENT_SOCKET,
    HANDLE_TYPE_CLIENT_SOCKET_WITH_SUBSCRIPTION // This client socket will recieve status updates as well
};

struct PollingHandles {
    struct pollfd* pfds;
    enum HandleType* types;
    size_t size, capacity;
};

static void Init(struct PollingHandles* handles) {
    handles->size = 0;
    handles->capacity = 1;
    handles->pfds = (struct pollfd*)calloc(sizeof(struct pollfd), handles->capacity);
    handles->types = (enum HandleType*)calloc(sizeof(enum HandleType), handles->capacity);
}

static void Deinit(struct PollingHandles* handles) {
    free(handles->pfds);
    free(handles->types);
}

static void Append(struct PollingHandles* handles, int fd, short events, enum HandleType type) {
    if (handles->size == handles->capacity) {
        handles->capacity *= 2;
        handles->pfds = realloc(handles->pfds, handles->capacity);
        handles->types = realloc(handles->types, handles->capacity);
        memset(handles->pfds + handles->size, 0, handles->size * sizeof(struct pollfd));
        memset(handles->types + handles->size, 0, handles->size * sizeof(enum HandleType));
    }

    handles->pfds[handles->size].fd = fd;
    handles->pfds[handles->size].events = events;
    handles->types[handles->size] = type;
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

static void StartRecievingData(int socket_desc, struct sockaddr_in* server) {
    listen(socket_desc, 3);

    printf("Waiting for incoming connections\n");

    socklen_t c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;
    int client_sock = accept(socket_desc, (struct sockaddr*)&client, &c);
    if (client_sock < 0) {
        fprintf(stderr, "accept failed\n");
        exit(1);
    }

    printf("Connection accepted\n");

    struct PollingHandles all_handles;
    Init(&all_handles);

    Append(&all_handles, client_sock, POLLIN, HANDLE_TYPE_CLIENT_SOCKET);

    fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);

    size_t write_amount = 0;
    char client_message[2000];
    size_t read_size;
    for (;;) {
        int ready = poll(all_handles.pfds, all_handles.size, 1000);
        if (ready != 0) {

            if (all_handles.pfds[0].revents & POLLIN) {

                read_size = recv(client_sock, client_message, 2000, 0);

                if (read_size > 0) {
                    if (client_message[0] == 'q') {
                        printf("exit requested\n");
                        break;
                    }
                    printf("I will write something now %lu\n", ++write_amount);
                    write(client_sock, client_message, strlen(client_message));
                } else {
                    printf("Client disconnected\n");
                    break;
                }

                if (read_size == -1) {
                    fprintf(stderr, "recv failed\n");
                    break;
                }
            } else {
                fprintf(stderr, "poll failed\n");
                break;
            }
        } else {
            printf("I do nothing this time %lu\n", ++write_amount);
        }
    }
    Deinit(&all_handles);
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