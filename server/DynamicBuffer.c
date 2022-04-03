#include "DynamicBuffer.h"

#include <stdlib.h>
#include <string.h>

#define DYNAMIC_BUFFER_INITIAL_SIZE 16

void DynamicBufferInit(DynamicBuffer* buffer) {
    buffer->size = 0;
    buffer->capacity = DYNAMIC_BUFFER_INITIAL_SIZE;
    buffer->data = (char*)malloc(DYNAMIC_BUFFER_INITIAL_SIZE);
}

void DynamicBufferDeinit(DynamicBuffer* buffer) { free(buffer->data); }

void _dynamicBufferExtend(DynamicBuffer* buffer, size_t minimal_new_size) {
    const size_t new_size = minimal_new_size * 2;
    buffer->data = (char*)realloc(buffer->data, new_size);
    buffer->capacity = new_size;
}

void DynamicBufferAppend(DynamicBuffer* buffer, const char* new_data, size_t new_data_size) {
    if (buffer->size + new_data_size >= buffer->capacity)
        _dynamicBufferExtend(buffer, buffer->size + new_data_size);

    memcpy(buffer->data + buffer->size, new_data, new_data_size);
    buffer->size += new_data_size;
}

void DynamicBufferTrimLeft(DynamicBuffer* buffer, size_t trim_amount) {
    if (trim_amount == buffer->size) {
        buffer->size = 0;
        return;
    }

    const size_t remainder = buffer->size - trim_amount;
    for (size_t i = 0; i < remainder; ++i)
        buffer->data[i] = buffer->data[i + trim_amount];
    buffer->size -= trim_amount;
}