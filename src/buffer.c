#include "fil_private.h"

#define DEFAULT_DYNAMIC_BUFFER_SIZE 128

FilBuffer fil_buffer_new_static(void *buffer, size_t buffer_size) {
    return (FilBuffer) {
        .data = buffer,
        .capacity = buffer_size,
        .length = 0,
        .fn_expand = nullptr,
    };
}

FilBuffer fil_buffer_new_dynamic(FilBufferExpandFn expand) {
    return (FilBuffer) {
        .data = expand(nullptr, DEFAULT_DYNAMIC_BUFFER_SIZE),
        .capacity = DEFAULT_DYNAMIC_BUFFER_SIZE,
        .length = 0,
        .fn_expand = expand,
    };
}

FilResult fil_buffer_insert(FilBuffer *buffer, uint8_t value) {
    if(buffer->length == buffer->capacity) {
        if(buffer->fn_expand == nullptr) return FIL_RESULT_NOMEM;

        size_t expanded_capacity = buffer->capacity * 2;
        void *expanded_data = buffer->fn_expand(buffer->data, expanded_capacity);
        if(expanded_data == nullptr) return FIL_RESULT_NOMEM;

        buffer->data = expanded_data;
        buffer->capacity = expanded_capacity;
    }

    buffer->data[buffer->length++] = value;

    return FIL_RESULT_OK;
}

int fil_buffer_peek(const FilBuffer *buffer, size_t negative_offset) {
    if(negative_offset > buffer->length) return -1;
    return buffer->data[buffer->length - negative_offset];
}
