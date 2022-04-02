#pragma once

#include "DynamicStringArray.h"

#define NO_PID -1

typedef struct GDBInstance {
    int pid;                          // NO_PID when the debugger is not running
    int stdout_handle, stderr_handle; // When the debugger is not running, these are undefined
    char* debugger_path;
    DynamicStringArray debugger_args;
} GDBInstance;

void GDBInstanceInit(GDBInstance*, const char* debugger_path);
void GDBInstanceDeinit(GDBInstance*);

// Put defaults into the instance for when the debugger process has ended by external means
void GDBInstanceClear(GDBInstance*);

int StartGDBServer(GDBInstance*, const DynamicStringArray* executable_arguments);
int StopGDBServer(GDBInstance*);