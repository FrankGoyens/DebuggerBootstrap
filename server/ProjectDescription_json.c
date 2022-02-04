#include "ProjectDescription_json.h"

#include <string.h>

#include <json.h>

#include "ProjectDescription.h"

int ProjectDescriptionLoadFromJSON(const char* json_string, struct ProjectDescription* project_description) {
    json_object* root = json_tokener_parse(json_string);

    json_object* executable_name_json = json_object_object_get(root, "executable_name");
    json_object* link_dependencies_for_executable_json =
        json_object_object_get(root, "link_dependencies_for_executable");

    if (executable_name_json == NULL || link_dependencies_for_executable_json == NULL) {
        json_object_put(root);
        return 0;
    }

    if (!json_object_is_type(executable_name_json, json_type_string)) {
        json_object_put(root);
        return 0;
    }

    if (!json_object_is_type(link_dependencies_for_executable_json, json_type_array)) {
        json_object_put(root);
        return 0;
    }

    ProjectDescriptionInit(project_description, json_object_get_string(executable_name_json));

    int array_length = json_object_array_length(link_dependencies_for_executable_json);
    for (int i = 0; i < array_length; ++i) {
        json_object* dependency = json_object_array_get_idx(link_dependencies_for_executable_json, i);
        if (json_object_is_type(executable_name_json, json_type_string))
            DynamicStringArrayAppend(&project_description->link_dependencies_for_executable,
                                     json_object_get_string(dependency));
    }

    json_object_put(root);
    return 1;
}

char* ProjectDescriptionDumpToJSON(struct ProjectDescription* const description) {
    json_object* root = json_object_new_object();
    json_object_object_add(root, "executable_name", json_object_new_string(description->executable_name));
    json_object* link_dependencies_for_executable_json_array = json_object_new_array();

    json_object_object_add(root, "link_dependencies_for_executable", link_dependencies_for_executable_json_array);

    for (int i = 0; i < description->link_dependencies_for_executable.size; ++i)
        json_object_array_add(link_dependencies_for_executable_json_array,
                              json_object_new_string(description->link_dependencies_for_executable.data[i]));

    const char* project_description_json_string = json_object_to_json_string(root);
    const size_t json_length = strlen(project_description_json_string);
    char* result = (char*)malloc(json_length + 1);
    strcpy(result, project_description_json_string);

    json_object_put(root);
    return result;
}