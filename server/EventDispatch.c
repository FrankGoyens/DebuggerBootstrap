#include "EventDispatch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>

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

    // for poll
    int nfds, num_open_fds;
    struct pollfd* pfds;

    num_open_fds = nfds = 1;
    pfds = calloc(nfds, sizeof(struct pollfd));

    pfds[0].fd = client_sock;
    pfds[0].events = POLLIN;

    fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);

    size_t write_amount = 0;
    char client_message[2000];
    size_t read_size;
    for (;;) {
        int ready = poll(pfds, nfds, 1000);
        if (ready != 0) {

            if (pfds[0].revents & POLLIN) {

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
    free(pfds);
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