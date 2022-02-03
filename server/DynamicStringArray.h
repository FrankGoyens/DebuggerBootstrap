#pragma once

#include <stddef.h>

struct DynamicStringArray{
	char** data;
	size_t size;
	size_t capacity;
};

void DynamicStringArrayInit(struct DynamicStringArray* string_array);

void DynamicStringArrayDeinit(struct DynamicStringArray* string_array);

void DynamicStringArrayAppend(struct DynamicStringArray* string_array, const char* item);
