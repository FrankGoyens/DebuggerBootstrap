#pragma once

#include "DynamicStringArray.h"

typedef struct Bootstrapper Bootstrapper;

typedef struct ProjectFileDifferences {
    DynamicStringArray missing, existing, wanted_hashes, actual_hashes;
} ProjectFileDifferences;

/* Use the current differences as reported by the Bootstrapper
 * Parameter bootstrapper is nullable. When the given bootstrapper is NULL, an empty ProjectFileDifferences is made
 */
void ProjectFileDifferencesInit(ProjectFileDifferences*, Bootstrapper*);
void ProjectFileDifferencesDeinit(ProjectFileDifferences*);

int ProjectFileDifferencesEqual(const ProjectFileDifferences*, const ProjectFileDifferences*);