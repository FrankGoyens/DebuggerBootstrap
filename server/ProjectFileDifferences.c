#include "ProjectFileDifferences.h"

#include <string.h>

#include "Bootstrapper.h"
#include "DynamicStringArray.h"

void ProjectFileDifferencesInit(ProjectFileDifferences* project_differences, Bootstrapper* bootstrapper) {
    DynamicStringArrayInit(&project_differences->missing);
    DynamicStringArrayInit(&project_differences->existing);
    DynamicStringArrayInit(&project_differences->actual_hashes);
    DynamicStringArrayInit(&project_differences->wanted_hashes);

    if (bootstrapper) {
        ReportMissingFiles(bootstrapper, &project_differences->missing);

        ReportWantedVsActualHashes(bootstrapper, &project_differences->existing, &project_differences->actual_hashes,
                                   &project_differences->wanted_hashes);
    }
}

void ProjectFileDifferencesDeinit(ProjectFileDifferences* project_differences) {
    DynamicStringArrayDeinit(&project_differences->missing);
    DynamicStringArrayDeinit(&project_differences->existing);
    DynamicStringArrayDeinit(&project_differences->actual_hashes);
    DynamicStringArrayDeinit(&project_differences->wanted_hashes);
}

static int DynamicStringArraysEqual(const DynamicStringArray* first, const DynamicStringArray* second) {
    if (first->size != second->size)
        return 0;

    for (int i = 0; i < first->size; ++i)
        if (strcmp(first->data[i], second->data[i]) != 0)
            return 0;
    return 1;
}

int ProjectFileDifferencesEqual(const ProjectFileDifferences* first, const ProjectFileDifferences* second) {
    return DynamicStringArraysEqual(&first->existing, &second->existing) &&
           DynamicStringArraysEqual(&first->missing, &second->missing) &&
           DynamicStringArraysEqual(&first->actual_hashes, &second->actual_hashes) &&
           DynamicStringArraysEqual(&first->wanted_hashes, &second->wanted_hashes);
}