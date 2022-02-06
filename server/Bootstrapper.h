#pragma once

struct ProjectDescription;
struct DynamicStringArray;

struct Bootstrapper {
    void* userdata;
    int(startGDBServer)(void*);
    int(stopGDBServer)(void*);

    void* _internal;
};

void BootstrapperInit(struct Bootstrapper*);
void BootstrapperDeinit(struct Bootstrapper*);

void RecieveNewProjectDescription(struct Bootstrapper*, struct ProjectDescription*);
void ChangeFileHash(struct Bootstrapper*, const char* file_name, const char* new_hash);
int IsProjectLoaded(const struct Boostrapper*);
// Up/Down status, according to the Boostrapper
int IsGDBServerUp(const struct Bootstrapper*);
void ReportMissingFiles(const struct Bootstrapper*, struct DynamicStringArray*);
void ReportDifferentFiles(const struct Bootstrapper*, struct DynamicStringArray* files,
                          struct DynamicStringArray* actual_hashes, struct DynamicStringArray* wanted_hashes);