#include <sys/inotify.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_EVENTS 1024							  /* Maximum number of events to process*/
#define LEN_NAME 16								  /* Assuming that the length of the filename \
							  won't exceed 16 bytes*/
#define EVENT_SIZE (sizeof(struct inotify_event)) /*size of one event*/
#define BUF_LEN (MAX_EVENTS * (EVENT_SIZE + LEN_NAME))

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: %s PATH [PATH ...]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int inotify_fd = inotify_init();
	if (inotify_fd < 0)
	{
		printf("Unable to open an inotify fd\n");
		exit(EXIT_FAILURE);
	}

	int directory_watch_fd = inotify_add_watch(inotify_fd, argv[1], IN_ALL_EVENTS);
	if (directory_watch_fd > 0)
	{
		printf("Watching directory %s\n", argv[1]);
		for (;;)
		{
			char watch_buffer[BUF_LEN];
			int read_length = read(inotify_fd, watch_buffer, BUF_LEN);
			int i = 0;
			while (i < read_length)
			{
				struct inotify_event *event = (struct inotify_event *)&watch_buffer[i];
				if (event->mask & IN_CREATE)
				{
					if (event->mask & IN_ISDIR)
					{
						printf("The directory %s was created.\n", event->name);
					}
					else
					{
						printf("The file %s was created.\n", event->name);
					}
				}
				else if (event->mask & IN_DELETE)
				{
					if (event->mask & IN_ISDIR)
					{
						printf("The directory %s was deleted.\n", event->name);
					}
					else
					{
						printf("The file %s was deleted.\n", event->name);
					}
				}
				else if (event->mask & IN_MODIFY)
				{
					if (event->mask & IN_ISDIR)
					{
						printf("The directory %s was modified.\n", event->name);
					}
					else
					{
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
