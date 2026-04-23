#include "fil.h"
#include "fil_private.h"

#include <assert.h>

static bool fil_stream_peek(FilStream *stream, size_t byte_count, size_t byte_offset, void *value_out) {
    if(stream->cursor + byte_offset + byte_count > stream->data_size) return false;

    const void *ptr = stream->data + stream->cursor + byte_offset;
    __builtin_memcpy(value_out, ptr, byte_count);

    return true;
}

static bool fil_stream_read(FilStream *stream, size_t byte_count, void *value_out) {
    if(!fil_stream_peek(stream, byte_count, 0, value_out)) return false;
    stream->cursor += byte_count;
    return true;
}

FilStream fil_stream_new(const void *data, size_t data_size) {
    return (FilStream) { .data = data, .data_size = data_size, .cursor = 0 };
}

FilBitStream fil_bitstream_new(FilStream *stream) {
    return (FilBitStream) { .stream = stream, .cached_cursor = -1 };
}

bool fil_stream_advance(FilStream *stream, size_t byte_count) {
    if(stream->cursor + byte_count > stream->data_size) return false;
    stream->cursor += byte_count;
    return true;
}

bool fil_stream_peek_u8(FilStream *stream, size_t byte_offset, uint8_t *value_out) {
    return fil_stream_peek(stream, 1, byte_offset, value_out);
}

bool fil_stream_peek_u16(FilStream *stream, size_t byte_offset, uint16_t *value_out) {
    return fil_stream_peek(stream, 2, byte_offset, value_out);
}

bool fil_stream_peek_u32(FilStream *stream, size_t byte_offset, uint32_t *value_out) {
    return fil_stream_peek(stream, 4, byte_offset, value_out);
}

bool fil_stream_peek_u64(FilStream *stream, size_t byte_offset, uint64_t *value_out) {
    return fil_stream_peek(stream, 8, byte_offset, value_out);
}

bool fil_stream_read_u8(FilStream *stream, uint8_t *value_out) {
    return fil_stream_read(stream, 1, value_out);
}

bool fil_stream_read_u16(FilStream *stream, uint16_t *value_out) {
    return fil_stream_read(stream, 2, value_out);
}

bool fil_stream_read_u32(FilStream *stream, uint32_t *value_out) {
    return fil_stream_read(stream, 4, value_out);
}

bool fil_stream_read_u64(FilStream *stream, uint64_t *value_out) {
    return fil_stream_read(stream, 8, value_out);
}

bool fil_stream_read_bytes(FilStream *stream, size_t byte_count, uint8_t *dest_buffer) {
    if(stream->cursor + byte_count > stream->data_size) return false;

    __builtin_memcpy(dest_buffer, &stream->data[stream->cursor], byte_count);
    stream->cursor += byte_count;

    return true;
}
