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

//! \brief Ownership of the project description is transferred
void ReceiveNewProjectDescription(struct Bootstrapper*, struct ProjectDescription*);
void ChangeFileHash(struct Bootstrapper*, const char* file_name, const char* new_hash);
int IsProjectLoaded(const struct Bootstrapper*);
// Up/Down status, according to the Boostrapper
int IsGDBServerUp(const struct Bootstrapper*);
void ReportMissingFiles(const struct Bootstrapper*, struct DynamicStringArray*);
void ReportDifferentFiles(const struct Bootstrapper*, struct DynamicStringArray* files,
                          struct DynamicStringArray* actual_hashes, struct DynamicStringArray* wanted_hashes);