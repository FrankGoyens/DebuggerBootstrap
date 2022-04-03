#include "GDBServerStartStop.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>

static void SetHandleDefaults(GDBInstance* instance) {
    instance->pid = NO_PID;
    instance->stdout_handle = -1;
    instance->stderr_handle = -1;
}

void GDBInstanceInit(GDBInstance* instance, const char* debugger_path, const DynamicStringArray* debugger_args) {
    SetHandleDefaults(instance);
    instance->debugger_path = (char*)malloc(sizeof(char) * (strlen(debugger_path) + 1));
    strcpy(instance->debugger_path, debugger_path);
    DynamicStringArrayCopy(debugger_args, &instance->debugger_args);
}

void GDBInstanceDeinit(GDBInstance* instance) {
    free(instance);
    free(instance->debugger_path);
}

static void CloseStdOutputs(GDBInstance* instance) {
    close(instance->stdout_handle);
    close(instance->stderr_handle);
}

void GDBInstanceClear(GDBInstance* instance) {
    CloseStdOutputs(instance);
    SetHandleDefaults(instance);
}

// Result should be free'd!
static char** ConcatArguments(GDBInstance* instance, char* program_to_debug,
                              const DynamicStringArray* executable_arguments) {
    const size_t argument_amount = 1 /*debugger executable*/ + instance->debugger_args.size + 1 /*program to debug*/ +
                                   executable_arguments->size + 1 /*null terminator*/;
    char** concatenated_arguments = (char**)malloc(sizeof(char*) * argument_amount);
    concatenated_arguments[0] = instance->debugger_path;
    memcpy(concatenated_arguments + 1, instance->debugger_args.data, instance->debugger_args.size * sizeof(char*));
    concatenated_arguments[instance->debugger_args.size + 1] = program_to_debug;
    memcpy(concatenated_arguments + 1 + instance->debugger_args.size + 1, executable_arguments->data,
           executable_arguments->size * sizeof(char*));
    concatenated_arguments[argument_amount - 1] = NULL;
    return concatenated_arguments;
}

static void PrintExecCall(char** args) {
    printf("GDBStart:\n");
    int i = 0;
    while (args[i] != NULL) {
        printf("%s ", args[i]);
        ++i;
    }
    printf("\n");
}

static int ReportPipeCreationStatus(int pipe) {
    errno = 0;
    if (pipe != 0) {
        fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

int StartGDBServer(GDBInstance* instance, char* program_to_debug, const DynamicStringArray* executable_arguments) {

    if (instance->pid != NO_PID)
        return 1; // Already running

    char** args = ConcatArguments(instance, program_to_debug, executable_arguments);
    PrintExecCall(args);

    int pipefd[2];
    int pipefd_err[2];
    int p1 = pipe(pipefd);
    if (!ReportPipeCreationStatus(p1))
        return 0;
    int p2 = pipe(pipefd_err);
    if (!ReportPipeCreationStatus(p2))
        return 0;
    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd_err[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        close(pipefd_err[0]);
        close(pipefd_err[1]);
        char* env[] = {NULL};
        errno = 0;
        execve(instance->debugger_path, args, env);
        free(args);
        fprintf(stderr, "Error calling execve in child process: %s\n", strerror(errno));
        exit(1);
    }

    // Only parent process continues here
    close(pipefd[1]);
    close(pipefd_err[1]);

    free(args);

    instance->pid = pid;
    instance->stdout_handle = pipefd[0];
    instance->stderr_handle = pipefd_err[0];

    return 1;
}

static int ChildProcessExited(int waitpid_status) { return WIFEXITED(waitpid_status) || WIFSIGNALED(waitpid_status); }

#define STOPPING_WAIT_TIME_MS 1000
#define SLEEP_WAIT_TIME_MS 10
#define MAX_WAIT_LOOPS (STOPPING_WAIT_TIME_MS / SLEEP_WAIT_TIME_MS)

// First signals SIGTERM, then waits up to STOPPING_WAIT_TIME_MS for GDB server to stop
// When the GDB server has not stopped after STOPPING_WAIT_TIME_MS, SIGKILL is sent, and it is assumed that the GDB
// server will stop
int StopGDBServer(GDBInstance* instance) {
    if (instance->pid == NO_PID)
        return 1;

    CloseStdOutputs(instance);

    kill(instance->pid, SIGTERM);
    int status;
    int loops = 0;

    waitpid(instance->pid, &status, WNOHANG);

    if (!ChildProcessExited(status)) {

        do {
            waitpid(instance->pid, &status, WNOHANG);

            sleep(SLEEP_WAIT_TIME_MS);
            ++loops;
        } while (!ChildProcessExited(status) && loops <= MAX_WAIT_LOOPS);
    }

    if (loops == MAX_WAIT_LOOPS)
        kill(instance->pid, SIGKILL);

    SetHandleDefaults(instance);

    return 1;
}