#pragma once

struct ProjectDescription;

int ProjectDescriptionLoadFromJSON(const char* json_string, struct ProjectDescription*);