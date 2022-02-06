#include "ProjectDescription.h"

#include <stdlib.h>
#include <string.h>

#include "DynamicStringArray.h"

void ProjectDescriptionInit(struct ProjectDescription* project_description, const char* executable_name,
                            const char* executable_hash) {
    const size_t name_length = strlen(executable_name);
    project_description->executable_name = (char*)malloc(name_length + 1);
    strcpy(project_description->executable_name, executable_name);

    const size_t hash_length = strlen(executable_hash);
    project_description->executable_hash = (char*)malloc(hash_length + 1);
    strcpy(project_description->executable_hash, executable_hash);

    DynamicStringArrayInit(&project_description->link_dependencies_for_executable);
    DynamicStringArrayInit(&project_description->link_dependencies_for_executable_hashes);
}

void ProjectDescriptionDeinit(struct ProjectDescription* project_description) {
    free(project_description->executable_name);
    free(project_description->executable_hash);
    DynamicStringArrayDeinit(&project_description->link_dependencies_for_executable);
    DynamicStringArrayDeinit(&project_description->link_dependencies_for_executable_hashes);
}
