#pragma once

#include "DynamicStringArray.h"

struct GDBInstance {
    int pid;
    char* debugger_path;
    struct DynamicStringArray debugger_args;
};

void GDBInstanceInit(struct GDBInstance*, const char* debugger_path);
void GDBInstanceDeinit(struct GDBInstance*);

int StartGDBServer(struct GDBInstance*);
int StopGDBServer(struct GDBInstance*);