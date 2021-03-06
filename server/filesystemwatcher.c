#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event)) /*size of one event*/

#define READING_BUFFER_INITIAL_SIZE 512
#define READ_BUFFER_LENGTH sizeof(struct inotify_event) + NAME_MAX + 1

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s PATH [PATH ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        printf("Unable to open an inotify fd\n");
        exit(EXIT_FAILURE);
    }

    int directory_watch_fd = inotify_add_watch(inotify_fd, argv[1], IN_ALL_EVENTS);
    if (directory_watch_fd > 0) {
        printf("Watching directory %s\n", argv[1]);
        for (;;) {
            char read_buffer[READ_BUFFER_LENGTH];
            int read_length = read(inotify_fd, read_buffer, READ_BUFFER_LENGTH);
            int i = 0;
            while (i < read_length) {
                struct inotify_event* event = (struct inotify_event*)&read_buffer[i];
                if (event->mask & IN_CREATE) {
                    if (event->mask & IN_ISDIR) {
                        printf("The directory %s was created.\n", event->name);
                    } else {
                        printf("The file %s was created.\n", event->name);
                    }
                } else if (event->mask & IN_DELETE) {
                    if (event->mask & IN_ISDIR) {
                        printf("The directory %s was deleted.\n", event->name);
                    } else {
                        printf("The file %s was deleted.\n", event->name);
                    }
                } else if (event->mask & IN_MODIFY) {
                    if (event->mask & IN_ISDIR) {
                        printf("The directory %s was modified.\n", event->name);
                    } else {
                        printf("The file %s was modified.\n", event->name);
                    }
                }
                i += EVENT_SIZE + event->len;
            }
        }
        inotify_rm_watch(inotify_fd, directory_watch_fd);
    }
    return 0;
}
