#include "GDBServerStartStop.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>

void GDBInstanceInit(struct GDBInstance* instance) {
    instance = (struct GDBInstance*)malloc(sizeof(struct GDBInstance));
    instance->pid = -1;
}
void GDBInstanceDeinit(struct GDBInstance* instance) { free(instance); }

int StartGDBServer(struct GDBInstance* instance) {

    if (instance->pid != -1)
        return 1; // Already running

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

    instance->pid = pid;

    return 1;
}

#define STOPPING_WAIT_TIME_MS 1000
#define SLEEP_WAIT_TIME_MS 10
#define MAX_WAIT_LOOPS (STOPPING_WAIT_TIME_MS / SLEEP_WAIT_TIME_MS)

// First signals SIGTERM, then waits up to STOPPING_WAIT_TIME_MS for GDB server to stop
// When the GDB server has not stopped after STOPPING_WAIT_TIME_MS, SIGKILL is sent, and it is assumed that the GDB
// server will stop
int StopGDBServer(struct GDBInstance* instance) {
    if (instance->pid == -1)
        return 1;

    kill(instance->pid, SIGTERM);
    int status;
    int loops = 0;
    do {
        waitpid(instance->pid, &status, WNOHANG);

        sleep(SLEEP_WAIT_TIME_MS);
        ++loops;
    } while (!WIFEXITED(status) && !WIFSIGNALED(status) && loops <= MAX_WAIT_LOOPS);
    if (loops == MAX_WAIT_LOOPS)
        kill(instance->pid, SIGKILL);
    instance->pid = -1;
    return 1;
}