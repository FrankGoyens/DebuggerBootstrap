#include "Bootstrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DynamicStringArray.h"
#include "ProjectDescription.h"

typedef struct {
    int gdbIsRunning;
    ProjectDescription projectDescription;

    DynamicStringArray existing, missing, hashesForExisting;
} BootstrapperInternal;

void BootstrapperInit(Bootstrapper* bootstrapper) {
    BootstrapperInternal* internal = (BootstrapperInternal*)malloc(sizeof(BootstrapperInternal));
    internal->gdbIsRunning = 0;
    ProjectDescriptionInit(&internal->projectDescription, "", "");
    DynamicStringArrayInit(&internal->existing);
    DynamicStringArrayInit(&internal->missing);
    DynamicStringArrayInit(&internal->hashesForExisting);
    bootstrapper->_internal = internal;
}

void BootstrapperDeinit(Bootstrapper* bootstrapper) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (internal) {
        ProjectDescriptionDeinit(&internal->projectDescription);
        DynamicStringArrayDeinit(&internal->existing);
        DynamicStringArrayDeinit(&internal->missing);
        DynamicStringArrayDeinit(&internal->hashesForExisting);

        free(bootstrapper->_internal);
    }
}

ProjectDescription* GetProjectDescription(const Bootstrapper* bootstrapper) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (internal)
        return &internal->projectDescription;

    return NULL;
}

static void FindFiles(Bootstrapper* bootstrapper, BootstrapperInternal* internal, DynamicStringArray* existing,
                      DynamicStringArray* missing) {
    DynamicStringArrayClear(existing);
    DynamicStringArrayClear(missing);
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

static void CalculateHashForHashArray(Bootstrapper* bootstrapper, const char* file, DynamicStringArray* hashes) {
    char* hash;
    size_t hash_size = 0;
    bootstrapper->calculateHash(file, &hash, &hash_size, bootstrapper->userdata);
    if (hash_size > 0) {
        DynamicStringArrayAppend(hashes, hash);
        free(hash);
    }
}

static void CalculateHashes(Bootstrapper* bootstrapper, BootstrapperInternal* internal, DynamicStringArray* hashes) {
    DynamicStringArrayClear(hashes);
    for (int i = 0; i < internal->existing.size; ++i) {
        CalculateHashForHashArray(bootstrapper, internal->existing.data[i], hashes);
    }
}

static int FindExistingFile(const char* existing_file, const ProjectDescription* description, char** hash) {
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

static int ShouldStartGDBServer(BootstrapperInternal* internal) {
    if (internal->missing.size > 0)
        return 0;

    if (internal->existing.size > internal->hashesForExisting.size)
        return 0;

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

static int Start(Bootstrapper* bootstrapper, BootstrapperInternal* internal) {
    if (internal->gdbIsRunning)
        return 0;
    if (bootstrapper->startGDBServer(bootstrapper->userdata, &internal->projectDescription.executable_arguments)) {
        internal->gdbIsRunning = 1;
        return 1;
    }
    return 0;
}

static int Stop(Bootstrapper* bootstrapper, BootstrapperInternal* internal) {
    if (!internal->gdbIsRunning)
        return 0;
    if (bootstrapper->stopGDBServer(bootstrapper->userdata)) {
        internal->gdbIsRunning = 0;
        return 1;
    }
    return 0;
}

static int ProjectIsLoaded(BootstrapperInternal* internal) {
    return strcmp(internal->projectDescription.executable_name, "") != 0;
}

void ReceiveNewProjectDescription(Bootstrapper* bootstrapper, ProjectDescription* description) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal)
        return;

    ProjectDescriptionDeinit(&internal->projectDescription);
    ProjectDescriptionCopy(description, &internal->projectDescription);

    Stop(bootstrapper, internal);

    FindFiles(bootstrapper, internal, &internal->existing, &internal->missing);
    CalculateHashes(bootstrapper, internal, &internal->hashesForExisting);
    if (ShouldStartGDBServer(internal))
        Start(bootstrapper, internal);
}

int IsProjectLoaded(const Bootstrapper* bootstrapper) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal)
        return 0;

    return ProjectIsLoaded(internal);
}

int IsGDBServerUp(const Bootstrapper* bootstrapper) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal)
        return 0;

    return internal->gdbIsRunning;
}

void ReportMissingFiles(const Bootstrapper* bootstrapper, DynamicStringArray* missing_files) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal || !ProjectIsLoaded(internal))
        return;

    DynamicStringArrayDeinit(missing_files);
    DynamicStringArrayCopy(&internal->missing, missing_files);
}

static int FindFile(const char* file, const DynamicStringArray* files, size_t* position) {
    for (int i = 0; i < files->size; ++i) {
        if (strcmp(file, files->data[i]) == 0) {
            *position = i;
            return 1;
        }
    }
    return 0;
}

void ReportWantedVsActualHashes(const Bootstrapper* bootstrapper, DynamicStringArray* files,
                                DynamicStringArray* actual_hashes, DynamicStringArray* wanted_hashes) {

    DynamicStringArrayClear(files);
    DynamicStringArrayClear(actual_hashes);
    DynamicStringArrayClear(wanted_hashes);

    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal || !ProjectIsLoaded(internal))
        return;

    DynamicStringArrayDeinit(files);
    DynamicStringArrayCopy(&internal->existing, files);

    for (int i = 0; i < files->size; ++i) {
        size_t position;

        if (strcmp(internal->projectDescription.executable_name, files->data[i]) == 0) {
            DynamicStringArrayAppend(wanted_hashes, internal->projectDescription.executable_hash);
            DynamicStringArrayAppend(actual_hashes, internal->projectDescription.executable_name);
        }

        if (FindFile(files->data[i], &internal->projectDescription.link_dependencies_for_executable, &position)) {
            DynamicStringArrayAppend(
                wanted_hashes, internal->projectDescription.link_dependencies_for_executable_hashes.data[position]);
            DynamicStringArrayAppend(actual_hashes, internal->hashesForExisting.data[i]);
        }
    }

    DynamicStringArrayDeinit(actual_hashes);
    DynamicStringArrayCopy(&internal->hashesForExisting, actual_hashes);

    if (files->size != wanted_hashes->size || files->size != actual_hashes->size)
        fprintf(stderr, "FIXME: %s:%d -- The three arrays should have the same size\n", __FILE__, __LINE__);
}

static void UpdateFileActualHashWithoutGDBStartCheck(Bootstrapper* bootstrapper, BootstrapperInternal* internal,
                                                     const char* file_name) {
    if (!bootstrapper->fileExists(file_name, bootstrapper->userdata))
        return;

    size_t find_index;
    if (FindFile(file_name, &internal->missing, &find_index)) {
        DynamicStringArrayErase(&internal->missing, find_index);
        DynamicStringArrayAppend(&internal->existing, file_name);

        char* hash;
        size_t hash_size;
        bootstrapper->calculateHash(file_name, &hash, &hash_size, bootstrapper->userdata);
        DynamicStringArrayAppend(&internal->hashesForExisting, hash);
        free(hash);
    } else if (FindFile(file_name, &internal->existing, &find_index)) {
        free(internal->hashesForExisting.data[find_index]);
        size_t hash_size;
        bootstrapper->calculateHash(file_name, &internal->hashesForExisting.data[find_index], &hash_size,
                                    bootstrapper->userdata);
    }
}

void UpdateFileActualHash(Bootstrapper* bootstrapper, const char* file_name) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal || !ProjectIsLoaded(internal))
        return;

    UpdateFileActualHashWithoutGDBStartCheck(bootstrapper, internal, file_name);

    if (ShouldStartGDBServer(internal))
        Start(bootstrapper, internal);
    else
        Stop(bootstrapper, internal);
}

void UpdateFileActualHashes(Bootstrapper* bootstrapper, const DynamicStringArray* file_names) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal || !ProjectIsLoaded(internal))
        return;

    for (int i = 0; i < file_names->size; ++i)
        UpdateFileActualHashWithoutGDBStartCheck(bootstrapper, internal, file_names->data[i]);

    if (ShouldStartGDBServer(internal))
        Start(bootstrapper, internal);
    else
        Stop(bootstrapper, internal);
}

void IndicateRemovedFile(Bootstrapper* bootstrapper, const char* file_name) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal || !ProjectIsLoaded(internal))
        return;

    if (bootstrapper->fileExists(file_name, bootstrapper->userdata))
        return;

    size_t find_index;
    if (FindFile(file_name, &internal->missing, &find_index)) {
        return;
    } else if (FindFile(file_name, &internal->existing, &find_index)) {
        DynamicStringArrayErase(&internal->existing, find_index);

        DynamicStringArrayAppend(&internal->missing, file_name);
        Stop(bootstrapper, internal);
    }
}

void IndicateDebuggerHasStopped(Bootstrapper* bootstrapper) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal)
        return;
    internal->gdbIsRunning = 0;
}

int ForceStartDebugger(Bootstrapper* bootstrapper) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal)
        return 0;
    return Start(bootstrapper, internal);
}

int ForceStopDebugger(Bootstrapper* bootstrapper) {
    BootstrapperInternal* internal = (BootstrapperInternal*)bootstrapper->_internal;
    if (!internal)
        return 0;
    return Stop(bootstrapper, internal);
}