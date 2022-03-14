#pragma once

struct GDBInstance {
    int pid;
};

void GDBInstanceInit(struct GDBInstance*);
void GDBInstanceDeinit(struct GDBInstance*);

int StartGDBServer(struct GDBInstance*);
int StopGDBServer(struct GDBInstance*);