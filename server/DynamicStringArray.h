#pragma once

#include <stddef.h>

typedef struct DynamicStringArray {
    char** data;
    size_t size;
    size_t capacity;
} DynamicStringArray;

void DynamicStringArrayInit(DynamicStringArray* string_array);
void DynamicStringArrayCopy(const DynamicStringArray* source, DynamicStringArray* destination);
void DynamicStringArrayDeinit(DynamicStringArray* string_array);

void DynamicStringArrayAppend(DynamicStringArray* string_array, const char* item);
void DynamicStringArrayErase(DynamicStringArray* string_array, size_t at);
void DynamicStringArrayClear(DynamicStringArray*);
