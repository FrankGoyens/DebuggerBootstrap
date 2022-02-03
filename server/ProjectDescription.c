#include "ProjectDescription.h"

#include <string.h>
#include <stdlib.h>

#include "DynamicStringArray.h"

void ProjectDescriptionInit(struct ProjectDescription* project_description, const char* executable_name){
    const size_t name_length = strlen(executable_name);
    project_description->executable_name = (char*)malloc(name_length + 1);
    strcpy(project_description->executable_name, executable_name);
    DynamicStringArrayInit(&project_description->link_dependencies_for_executable);
}

void ProjectDescriptionDeinit(struct ProjectDescription* project_description){
    free(project_description->executable_name);
    DynamicStringArrayDeinit(&project_description->link_dependencies_for_executable);
}

