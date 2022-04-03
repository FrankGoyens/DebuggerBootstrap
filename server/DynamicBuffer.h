#pragma once

#include <stddef.h>

typedef struct DynamicBuffer {
    char* data;
    size_t size;
    size_t capacity;
} DynamicBuffer;

void DynamicBufferInit(DynamicBuffer* buffer);
void DynamicBufferDeinit(DynamicBuffer* buffer);

void DynamicBufferAppend(DynamicBuffer* buffer, const char* new_data, size_t new_data_size);
void DynamicBufferTrimLeft(DynamicBuffer* buffer, size_t trim_amount);
