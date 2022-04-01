#pragma once

#include "DynamicStringArray.h"

typedef struct DebuggerParameters {
    const char* debugger_path;
    DynamicStringArray debugger_args;
} DebuggerParameters;

// Debugger parameters are not free'd by this function
void StartEventDispatch(int port, DebuggerParameters*);