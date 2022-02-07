#include "Bootstrapper.h"

#include <stdlib.h>
#include <string.h>

#include "DynamicStringArray.h"
#include "ProjectDescription.h"

struct BootstrapperInternal {
    int gdbIsRunning;
    int projectIsLoaded;
    struct ProjectDescription projectDescription;

    struct DynamicStringArray existing, missing, hashesForExisting;
};

void BootstrapperInit(struct Bootstrapper* bootstrapper) {
    struct BootstrapperInternal* internal = (struct BootstrapperInternal*)malloc(sizeof(struct BootstrapperInternal));
    internal->gdbIsRunning = 0;
    internal->projectIsLoaded = 0;
    bootstrapper->_internal = internal;
}
void BootstrapperDeinit(struct Bootstrapper* bootstrapper) { free(bootstrapper->_internal); }

static void FindFiles(struct Bootstrapper* bootstrapper, struct BootstrapperInternal* internal,
                      struct DynamicStringArray* existing, struct DynamicStringArray* missing) {
    DynamicStringArrayInit(existing);
    DynamicStringArrayInit(missing);
    if (bootstrapper->fileExists(internal->projectDescription.executable_name, bootstrapper->userdata))
        DynamicStringArrayAppend(existing, internal->projectDescription.executable_name);
    else
        DynamicStringArrayAppend(missing, internal->projectDescription.executable_name);
    for (size_t i = 0; i < internal->projectDescription.link_dependencies_for_executable.size; ++i) {
        const char* link_dependency = internal->projectDescription.link_dependencies_for_executable.data[i];
        if (bootstrapper->fileExists(link_dependency, bootstrapper->userdata))
            DynamicStringArrayAppend(existing, link_dependency);
        else
            DynamicStringArrayAppend(missing, link_dependency);
    }
}

static void CalculateHashes(struct Bootstrapper* bootstrapper, struct BootstrapperInternal* internal,
                            struct DynamicStringArray* hashes) {
    DynamicStringArrayInit(hashes);
    for (int i = 0; i < internal->existing.size; ++i) {
        char* hash;
        size_t hash_size;
        bootstrapper->calculateHash(internal->existing.data[i], &hash, &hash_size, bootstrapper->userdata);
        if (hash_size > 0) {
            hash = realloc(hash, hash_size + 1);
            hash[hash_size - 1] = '\0';
            DynamicStringArrayAppend(hashes, hash);
            free(hash);
        }
    }
}

static int FindExistingFile(const char* existing_file, const struct ProjectDescription* description, char** hash) {
    if (strcmp(existing_file, description->executable_name) == 0) {
        *hash = description->executable_hash;
        return 1;
    }
    for (int i = 0; i < description->link_dependencies_for_executable.size; ++i) {
        if (strcmp(existing_file, description->link_dependencies_for_executable.data[i]) == 0) {
            *hash = description->link_dependencies_for_executable_hashes.data[i];
            return 1;
        }
    }
    return 0;
}

static int ShouldStartGDBServer(struct BootstrapperInternal* internal) {
    for (int existingIndex = 0; existingIndex < internal->existing.size; ++existingIndex) {
        const char* actual_hash = internal->hashesForExisting.data[existingIndex];
        char* wanted_hash;
        if (FindExistingFile(internal->existing.data[existingIndex], &internal->projectDescription, &wanted_hash)) {
            if (strcmp(actual_hash, wanted_hash) != 0)
                return 0;
        } else {
            return 0; // An existing file that doesn't exist in the project description, very strange
        }
    }
    return 1;
}

static void Start(struct Bootstrapper* bootstrapper, struct BootstrapperInternal* internal) {
    if (internal->gdbIsRunning)
        bootstrapper->stopGDBServer(bootstrapper->userdata);
    bootstrapper->startGDBServer(bootstrapper->userdata);
    internal->gdbIsRunning = 1;
}

static void Stop(struct Bootstrapper* bootstrapper, struct BootstrapperInternal* internal) {
    if (internal->gdbIsRunning)
        bootstrapper->stopGDBServer(bootstrapper->userdata);
    internal->gdbIsRunning = 0;
}

void RecieveNewProjectDescription(struct Bootstrapper* bootstrapper, struct ProjectDescription* description) {
    struct BootstrapperInternal* internal = (struct BootstrapperInternal*)bootstrapper->_internal;
    if (!internal)
        return;

    if (internal->projectIsLoaded) {
        ProjectDescriptionDeinit(&internal->projectDescription);
        DynamicStringArrayDeinit(&internal->existing);
        DynamicStringArrayDeinit(&internal->missing);
        DynamicStringArrayDeinit(&internal->hashesForExisting);
        Stop(bootstrapper, internal);
    }

    internal->projectDescription = *description;
    internal->projectIsLoaded = 1;

    FindFiles(bootstrapper, internal, &internal->existing, &internal->missing);
    CalculateHashes(bootstrapper, internal, &internal->hashesForExisting);
    if (ShouldStartGDBServer(internal))
        Start(bootstrapper, internal);
}

int IsProjectLoaded(const struct Bootstrapper* bootstrapper) {
    struct BootstrapperInternal* internal = (struct BootstrapperInternal*)bootstrapper->_internal;
    if (!internal)
        return 0;

    return internal->projectIsLoaded;
}

int IsGDBServerUp(const struct Bootstrapper* bootstrapper) {
    struct BootstrapperInternal* internal = (struct BootstrapperInternal*)bootstrapper->_internal;
    if (!internal)
        return 0;

    return internal->gdbIsRunning;
}

void ReportMissingFiles(const struct Bootstrapper* bootstrapper, struct DynamicStringArray* missing_files) {
    struct BootstrapperInternal* internal = (struct BootstrapperInternal*)bootstrapper->_internal;
    if (!internal || !internal->projectIsLoaded)
        return;

    *missing_files = internal->missing;
}

void ReportDifferentFiles(const struct Bootstrapper* bootstrapper, struct DynamicStringArray* files,
                          struct DynamicStringArray* actual_hashes, struct DynamicStringArray* wanted_hashes) {
    struct BootstrapperInternal* internal = (struct BootstrapperInternal*)bootstrapper->_internal;
    if (!internal || !internal->projectIsLoaded)
        return;
    // TODO
}