#pragma once

#include "DynamicStringArray.h"

struct ProjectDescription {
    char* executable_name;
    char* executable_hash;
    struct DynamicStringArray link_dependencies_for_executable;
    struct DynamicStringArray link_dependencies_for_executable_hashes;
    struct DynamicStringArray executable_arguments;
};

void ProjectDescriptionInit(struct ProjectDescription*, const char* executable_name, const char* executable_hash);
void ProjectDescriptionDeinit(struct ProjectDescription*);
void ProjectDescriptionCopy(const struct ProjectDescription* source, struct ProjectDescription* dest);