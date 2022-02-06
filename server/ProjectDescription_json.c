#include "ProjectDescription_json.h"

#include <string.h>

#include <json.h>

#include "ProjectDescription.h"

static void ReadJSONArray(json_object* array, struct DynamicStringArray* dynamic_array) {
    int array_length = json_object_array_length(array);
    for (int i = 0; i < array_length; ++i) {
        json_object* dependency = json_object_array_get_idx(array, i);
        if (json_object_is_type(dependency, json_type_string))
            DynamicStringArrayAppend(dynamic_array, json_object_get_string(dependency));
    }
}

int ProjectDescriptionLoadFromJSON(const char* json_string, struct ProjectDescription* project_description) {
    json_object* root = json_tokener_parse(json_string);

    json_object* executable_name_json = json_object_object_get(root, "executable_name");
    json_object* link_dependencies_for_executable_json =
        json_object_object_get(root, "link_dependencies_for_executable");
    json_object* link_dependencies_for_executable_hashes_json =
        json_object_object_get(root, "link_dependencies_for_executable_hashes");

    if (executable_name_json == NULL || link_dependencies_for_executable_json == NULL)
        return json_object_put(root), 0;

    if (!json_object_is_type(executable_name_json, json_type_string))
        return json_object_put(root), 0;

    if (!json_object_is_type(link_dependencies_for_executable_json, json_type_array))
        return json_object_put(root), 0;

    if (!json_object_is_type(link_dependencies_for_executable_hashes_json, json_type_array))
        return json_object_put(root), 0;

    ProjectDescriptionInit(project_description, json_object_get_string(executable_name_json));

    ReadJSONArray(link_dependencies_for_executable_json, &project_description->link_dependencies_for_executable);
    ReadJSONArray(link_dependencies_for_executable_hashes_json,
                  &project_description->link_dependencies_for_executable_hashes);

    json_object_put(root);
    return 1;
}

static void DumpIntoJSONArray(const struct DynamicStringArray* dynamic_array, json_object* array) {
    for (int i = 0; i < dynamic_array->size; ++i)
        json_object_array_add(array, json_object_new_string(dynamic_array->data[i]));
}

char* ProjectDescriptionDumpToJSON(struct ProjectDescription* const description) {
    json_object* root = json_object_new_object();
    json_object_object_add(root, "executable_name", json_object_new_string(description->executable_name));
    json_object* link_dependencies_for_executable_json_array = json_object_new_array();
    json_object* link_dependencies_for_executable_hashes_json_array = json_object_new_array();

    json_object_object_add(root, "link_dependencies_for_executable", link_dependencies_for_executable_json_array);
    DumpIntoJSONArray(&description->link_dependencies_for_executable, link_dependencies_for_executable_json_array);

    json_object_object_add(root, "link_dependencies_for_executable_hashes",
                           link_dependencies_for_executable_hashes_json_array);
    DumpIntoJSONArray(&description->link_dependencies_for_executable_hashes,
                      link_dependencies_for_executable_hashes_json_array);

    const char* project_description_json_string = json_object_to_json_string(root);
    const size_t json_length = strlen(project_description_json_string);
    char* result = (char*)malloc(json_length + 1);
    strcpy(result, project_description_json_string);

    json_object_put(root);
    return result;
}