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

/*
 * Function pointer to a function that expands the buffer.
 *
 * Takes a pointer to the current data allocation and
 * a size for the new allocation. The pointer may be a
 * nullptr.
 *
 * Returns a pointer to the existing or a new allocation
 * with the new size. The new allocation must contain the
 * data from the original allocation.
 */
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

/*
 * Create a buffer of static size.
 *
 * Note that if the decompressed data does not
 * fit within the buffer an error will be returned.
 */
FilBuffer fil_buffer_new_static(void *buffer, size_t buffer_size);

/**
 * Create a buffer of dynamic size.
 *
 * This buffer will expand as needed.
 *
 * Note that at the end of decompression the `length` field of
 * the buffer indicates actual size of decompressed data.
 *
 * Also, remember to free the data buffer :)
 */
FilBuffer fil_buffer_new_dynamic(FilBufferExpandFn expand);

/**
 * Decompress `input_stream` to `output_buffer` using
 * the deflate algorithm.
 */
FilResult fil_deflate(FilBitStream *input_stream, FilBuffer *output_buffer);

/**
 * Decompress a GZIP in `input_stream` to `output_buffer`.
 */
FilResult fil_gzip_decompress(FilStream *input_stream, FilBuffer *output_buffer);

/**
 * Compute CRC32 for data.
 */
uint32_t fil_crc32(const void *data, size_t data_length);
