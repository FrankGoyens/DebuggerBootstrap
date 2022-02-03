#pragma once

#include "DynamicStringArray.h"

struct ProjectDescription {
    char* executable_name;
    struct DynamicStringArray link_dependencies_for_executable;
};

void ProjectDescriptionInit(struct ProjectDescription*, const char* executable_name);
void ProjectDescriptionDeinit(struct ProjectDescription*);