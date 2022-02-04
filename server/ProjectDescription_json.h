#pragma once

struct ProjectDescription;

int ProjectDescriptionLoadFromJSON(const char* json_string, struct ProjectDescription*);

// Don't forget to free() the result
char* ProjectDescriptionDumpToJSON(struct ProjectDescription* const);