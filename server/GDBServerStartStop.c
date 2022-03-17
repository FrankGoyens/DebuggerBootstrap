#include "GDBServerStartStop.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>

void GDBInstanceInit(struct GDBInstance* instance, const char* debugger_path) {
    instance->pid = -1;
    instance->debugger_path = (char*)malloc(sizeof(char) * (strlen(debugger_path) + 1));
    strcpy(instance->debugger_path, debugger_path);
    DynamicStringArrayInit(&instance->debugger_args);
}

void GDBInstanceDeinit(struct GDBInstance* instance) {
    free(instance);
    free(instance->debugger_path);
}

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
        char* args[] = {instance->debugger_path, NULL};
        char* env[] = {NULL};
        errno = 0;
        execve(instance->debugger_path, args, env);
        fprintf(stderr, "Error calling execve in child process: %s\n", strerror(errno));
        exit(1);
    } else {
        close(pipefd[1]);
        close(pipefd_err[1]);

        char buffer[512];

        int bytes = 0;
        int bytes_err = 0;
        while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) > 0)
            printf("Output: (%.*s)\n", bytes, buffer);
        while ((bytes_err = read(pipefd_err[0], buffer, sizeof(buffer))) > 0)
            printf("Output (err): (%.*s)\n", bytes_err, buffer);

        close(pipefd[0]);
        close(pipefd_err[0]);
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