#pragma once

#include <stddef.h>

struct ProjectDescription;
struct DynamicStringArray;

struct Bootstrapper {
    void* userdata;
    int (*startGDBServer)(void*);
    int (*stopGDBServer)(void*);
    int (*fileExists)(const char*, void*);
    void (*calculateHash)(const char*, char**, size_t*, void*);

    void* _internal;
};

void BootstrapperInit(struct Bootstrapper*);
void BootstrapperDeinit(struct Bootstrapper*);

struct ProjectDescription* GetProjectDescription(const struct Bootstrapper*);

// Ownership of the project description is transferred
void ReceiveNewProjectDescription(struct Bootstrapper*, struct ProjectDescription*);
int IsProjectLoaded(const struct Bootstrapper*);
// Up/Down status, according to the Boostrapper
int IsGDBServerUp(const struct Bootstrapper*);
// Output argument must be initialized and will be owned by the caller
void ReportMissingFiles(const struct Bootstrapper*, struct DynamicStringArray*);
// Output arguments must be initialized and will be owned by the caller
void ReportWantedVsActualHashes(const struct Bootstrapper*, struct DynamicStringArray* files,
                                struct DynamicStringArray* actual_hashes, struct DynamicStringArray* wanted_hashes);
// If the given file previously did not exist, the entry is updated when now it does exist
// The actual hash is calculated for the given file, if the file is present in the project description
// Will trigger a StartGDBServer when every file exists and matches
void UpdateFileActualHash(struct Bootstrapper*, const char* file_name);