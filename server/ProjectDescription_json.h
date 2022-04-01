#pragma once

typedef struct ProjectDescription ProjectDescription;

int ProjectDescriptionLoadFromJSON(const char* json_string, ProjectDescription*);

// Don't forget to free() the result
char* ProjectDescriptionDumpToJSON(ProjectDescription* const);