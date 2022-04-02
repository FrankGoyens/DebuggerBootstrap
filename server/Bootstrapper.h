#pragma once

#include <stddef.h>

typedef struct ProjectDescription ProjectDescription;
typedef struct DynamicStringArray DynamicStringArray;

typedef struct Bootstrapper {
    void* userdata;
    int (*startGDBServer)(void*, const char* program_to_debug, const DynamicStringArray* executable_arguments);
    int (*stopGDBServer)(void*);
    int (*fileExists)(const char*, void*);
    void (*calculateHash)(const char*, char**, size_t*, void*);

    void* _internal;
} Bootstrapper;

void BootstrapperInit(Bootstrapper*);
void BootstrapperDeinit(Bootstrapper*);

ProjectDescription* GetProjectDescription(const Bootstrapper*);

// Ownership of the project description is transferred
void ReceiveNewProjectDescription(Bootstrapper*, ProjectDescription*);
int IsProjectLoaded(const Bootstrapper*);
// Up/Down status, according to the Boostrapper
int IsGDBServerUp(const Bootstrapper*);
// Output argument must be initialized and will be owned by the caller
void ReportMissingFiles(const Bootstrapper*, DynamicStringArray*);
// Output arguments must be initialized and will be owned by the caller
void ReportWantedVsActualHashes(const Bootstrapper*, DynamicStringArray* files, DynamicStringArray* actual_hashes,
                                DynamicStringArray* wanted_hashes);
// If the given file previously did not exist, the entry is updated when now it does exist
// The actual hash is calculated for the given file, if the file is present in the project description
// Will trigger a StartGDBServer when every file exists and matches
void UpdateFileActualHash(Bootstrapper*, const char* file_name);
// Same as UpdateFileActualHash, except checking whether the GDBServer should start happens after all files have been
// processed
void UpdateFileActualHashes(Bootstrapper*, const DynamicStringArray* file_names);
void IndicateRemovedFile(Bootstrapper*, const char* file_name);

void IndicateDebuggerHasStopped(Bootstrapper*);

// Returns FALSE when the debugger is already running
// Returns TRUE when the debugger was not running and was succesfully started
int ForceStartDebugger(Bootstrapper*);

// Returns FALSE when the debugger was not running
// Returns TRUE when the debugger was running and was succesfully stopped
int ForceStopDebugger(Bootstrapper*);