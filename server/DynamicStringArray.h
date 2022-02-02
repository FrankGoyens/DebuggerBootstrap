#pragma once

#include <stddef.h>

struct DynamicStringArray{
	char** data;
	size_t size;
	size_t capacity;
};

void init(struct DynamicStringArray* string_array);

void deinit(struct DynamicStringArray* string_array);

void append(struct DynamicStringArray* string_array, const char* item);
