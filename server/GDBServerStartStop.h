#pragma once

#include "DynamicStringArray.h"

#define NO_PID -1

struct GDBInstance {
    int pid;                          // NO_PID when the debugger is not running
    int stdout_handle, stderr_handle; // When the debugger is not running, these are undefined
    char* debugger_path;
    struct DynamicStringArray debugger_args;
};

void GDBInstanceInit(struct GDBInstance*, const char* debugger_path);
void GDBInstanceDeinit(struct GDBInstance*);

// Put defaults into the instance for when the debugger process has ended by external means
void GDBInstanceClear(struct GDBInstance*);

int StartGDBServer(struct GDBInstance*);
int StopGDBServer(struct GDBInstance*);