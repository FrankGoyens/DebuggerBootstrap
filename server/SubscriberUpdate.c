#include "SubscriberUpdate.h"

#include <string.h>

#include <json.h>

char* EncodeSubscriberUpdateMessage(const char* tag, const char* message) {
    json_object* root = json_object_new_object();
    json_object_object_add(root, "tag", json_object_new_string(tag));
    json_object_object_add(root, "message", json_object_new_string(message));
    const char* project_description_json_string = json_object_to_json_string(root);

    size_t json_length = strlen(project_description_json_string);

    char* result = (char*)malloc(json_length + 1);
    strcpy(result, project_description_json_string);

    json_object_put(root);
    return result;
}