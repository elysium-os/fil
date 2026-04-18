#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    FIL_RESULT_OK = 0,
    FIL_RESULT_UNSUPPORTED_FEATURE,
    FIL_RESULT_INVALID_HEADER,
    FIL_RESULT_MALFORMED,
    FIL_RESULT_CHECKSUM_MISMATCH,
    FIL_RESULT_UNEXPECTED_EOF,
    FIL_RESULT_NOMEM,
} FilResult;

typedef void *(*FilBufferExpandFn)(void *cur, size_t size);

typedef struct {
    const void *data;
    size_t data_size;
    size_t cursor;
} FilStream;

typedef struct {
    FilStream *stream;
    uint8_t cached_byte;
    int cached_cursor;
} FilBitStream;

typedef struct {
    uint8_t *data;
    size_t length;
    size_t capacity;
    FilBufferExpandFn fn_expand;
} FilBuffer;

FilStream fil_stream_new(const void *data, size_t data_size);
FilBitStream fil_bitstream_new(FilStream *stream);

FilBuffer fil_buffer_new_static(void *buffer, size_t buffer_size);
FilBuffer fil_buffer_new_dynamic(FilBufferExpandFn expand);

FilResult fil_deflate(FilBitStream *input_stream, FilBuffer *output_buffer);
FilResult fil_gzip_decompress(FilStream *input_stream, FilBuffer *output_buffer);
