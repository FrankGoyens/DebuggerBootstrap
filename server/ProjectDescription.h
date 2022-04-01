#pragma once

#include "DynamicStringArray.h"

typedef struct ProjectDescription {
    char* executable_name;
    char* executable_hash;
    DynamicStringArray link_dependencies_for_executable;
    DynamicStringArray link_dependencies_for_executable_hashes;
    DynamicStringArray executable_arguments;
} ProjectDescription;

void ProjectDescriptionInit(ProjectDescription*, const char* executable_name, const char* executable_hash);
void ProjectDescriptionDeinit(ProjectDescription*);
void ProjectDescriptionCopy(const ProjectDescription* source, ProjectDescription* dest);