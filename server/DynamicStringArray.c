#include "DynamicStringArray.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DYNAMIC_STRING_ARRAY_INITIAL_CAPACITY 16

void DynamicStringArrayInit(struct DynamicStringArray* string_array) {
    string_array->size = 0;
    string_array->capacity = DYNAMIC_STRING_ARRAY_INITIAL_CAPACITY;
    string_array->data = (char**)malloc(DYNAMIC_STRING_ARRAY_INITIAL_CAPACITY * sizeof(char*));
}

void DynamicStringArrayDeinit(struct DynamicStringArray* string_array) {
    DynamicStringArrayClear(string_array);
    free(string_array->data);
}

static void _extend(struct DynamicStringArray* string_array) {
    string_array->capacity *= 2;
    string_array->data = (char**)realloc(string_array->data, string_array->capacity * sizeof(char*));
}

void DynamicStringArrayAppend(struct DynamicStringArray* string_array, const char* item) {
    if (string_array->size == string_array->capacity)
        _extend(string_array);
    const size_t item_length = strlen(item);
    string_array->data[string_array->size] = (char*)malloc(item_length + 1);
    strncpy(string_array->data[string_array->size], item, item_length + 1);
    ++string_array->size;
}

void DynamicStringArrayClear(struct DynamicStringArray* string_array) {
    for (int i = 0; i < string_array->size; ++i)
        free(string_array->data[i]);
    string_array->size = 0;
}