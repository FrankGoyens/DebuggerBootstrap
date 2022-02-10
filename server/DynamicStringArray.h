#pragma once

#include <stddef.h>

struct DynamicStringArray {
    char** data;
    size_t size;
    size_t capacity;
};

void DynamicStringArrayInit(struct DynamicStringArray* string_array);
void DynamicStringArrayCopy(const struct DynamicStringArray* source, struct DynamicStringArray* destination);
void DynamicStringArrayDeinit(struct DynamicStringArray* string_array);

void DynamicStringArrayAppend(struct DynamicStringArray* string_array, const char* item);
void DynamicStringArrayErase(struct DynamicStringArray* string_array, size_t at);
void DynamicStringArrayClear(struct DynamicStringArray*);
