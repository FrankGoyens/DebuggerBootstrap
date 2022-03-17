#pragma once

#include "DynamicStringArray.h"

struct DebuggerParameters {
    const char* debugger_path;
    struct DynamicStringArray debugger_args;
};

// Debugger parameters are not free'd by this function
void StartEventDispatch(int port, struct DebuggerParameters*);