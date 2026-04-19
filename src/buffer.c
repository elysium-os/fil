#include "fil_private.h"

#define DYNAMIC_BUFFER_DEFAULT_SIZE (1 << 7)
#define DYNAMIC_BUFFER_SHIFT_AMOUNT 1

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
        .data = expand(nullptr, DYNAMIC_BUFFER_DEFAULT_SIZE),
        .capacity = DYNAMIC_BUFFER_DEFAULT_SIZE,
        .length = 0,
        .fn_expand = expand,
    };
}

FilResult fil_buffer_expand(FilBuffer *buffer, size_t minimum_amount) {
    if(buffer->fn_expand == nullptr) {
        return FIL_RESULT_NOMEM;
    }

    size_t required_capacity = buffer->capacity + minimum_amount;
    size_t new_capacity = buffer->capacity;
    do {
        new_capacity <<= DYNAMIC_BUFFER_SHIFT_AMOUNT;
    } while(new_capacity < required_capacity);

    if(new_capacity < buffer->capacity) {
        return FIL_RESULT_NOMEM;
    }

    void *new_data = buffer->fn_expand(buffer->data, new_capacity);
    if(new_data == nullptr) {
        return FIL_RESULT_NOMEM;
    }

    buffer->data = new_data;
    buffer->capacity = new_capacity;

    return FIL_RESULT_OK;
}
