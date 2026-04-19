#include "fil_private.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define BYTE_SIZE 8
#define OUTPUT_STREAM_DEFAULT_CAPACITY 32

static int fil_stream_peek(FilStream *stream, size_t byte_count, size_t byte_offset, void *value_out) {
    if(stream->cursor + byte_offset + byte_count > stream->data_size) return -1;

    const void *ptr = stream->data + stream->cursor + byte_offset;
    switch(byte_count) {
        case 8:  *(uint64_t *) value_out = *(const uint64_t *) ptr; break;
        case 4:  *(uint32_t *) value_out = *(const uint32_t *) ptr; break;
        case 2:  *(uint16_t *) value_out = *(const uint16_t *) ptr; break;
        case 1:  *(uint8_t *) value_out = *(const uint8_t *) ptr; break;
        default: assert(false);
    }

    return 0;
}

FilStream fil_stream_new(const void *data, size_t data_size) {
    return (FilStream) { .data = data, .data_size = data_size, .cursor = 0 };
}

FilBitStream fil_bitstream_new(FilStream *stream) {
    return (FilBitStream) { .stream = stream, .cached_cursor = -1 };
}

void fil_stream_advance(FilStream *stream, size_t byte_count) {
    stream->cursor += byte_count;
}

int fil_stream_read(FilStream *stream, size_t byte_count, void *value_out) {
    int result = fil_stream_peek(stream, byte_count, 0, value_out);
    if(result == 0) fil_stream_advance(stream, byte_count);
    return result;
}

int fil_stream_read_u8(FilStream *stream, uint8_t *value_out) {
    return fil_stream_read(stream, 1, value_out);
}

int fil_stream_read_u16(FilStream *stream, uint16_t *value_out) {
    return fil_stream_read(stream, 2, value_out);
}

int fil_stream_read_u32(FilStream *stream, uint32_t *value_out) {
    return fil_stream_read(stream, 4, value_out);
}

int fil_stream_read_u64(FilStream *stream, uint64_t *value_out) {
    return fil_stream_read(stream, 8, value_out);
}

int fil_bitstream_advance(FilBitStream *bitstream, size_t bit_count) {
    assert(bit_count <= sizeof(size_t) * BYTE_SIZE);

    if(bitstream->cached_cursor != -1) {
        size_t offset = (size_t) (BYTE_SIZE - bitstream->cached_cursor);
        if(bit_count <= offset) {
            bitstream->cached_cursor += bit_count;
            return 0;
        }

        bit_count -= offset;
    }

    size_t byte_count = 1 + (bit_count / BYTE_SIZE);
    bit_count %= BYTE_SIZE;

    bitstream->cached_cursor = bit_count;
    if(byte_count > 1) fil_stream_advance(bitstream->stream, byte_count - 1);

    return fil_stream_read_u8(bitstream->stream, &bitstream->cached_byte);
}

size_t fil_bitstream_peek(FilBitStream *bitstream, size_t bit_count, size_t *value_out) {
    assert(bit_count <= sizeof(size_t) * BYTE_SIZE);

    size_t byte_offset = 0;
    int cursor = bitstream->cached_cursor;
    uint8_t byte = bitstream->cached_byte;

    size_t value = 0;
    for(size_t i = 0; i < bit_count; i++) {
        if(cursor == -1 || cursor == BYTE_SIZE) {
            if(fil_stream_peek(bitstream->stream, 1, byte_offset, &byte) != 0) return i;
            cursor = 0;
            byte_offset++;
        }

        uint8_t bit = (byte >> cursor) & 1;
        value |= bit << i;
        cursor++;
    }

    *value_out = value;

    return bit_count;
}

int fil_bitstream_read(FilBitStream *bitstream, size_t bit_count, size_t *value_out) {
    assert(bit_count <= sizeof(size_t) * BYTE_SIZE);

    size_t value = 0;
    for(size_t i = 0; i < bit_count; i++) {
        if(bitstream->cached_cursor == -1 || bitstream->cached_cursor == BYTE_SIZE) {
            if(fil_stream_read_u8(bitstream->stream, &bitstream->cached_byte) != 0) return -1;
            bitstream->cached_cursor = 0;
        }

        uint8_t bit = (bitstream->cached_byte >> bitstream->cached_cursor) & 1;
        value |= bit << i;
        bitstream->cached_cursor++;
    }

    *value_out = value;

    return 0;
}

int fil_bitstream_read_aligned_u16(FilBitStream *bitstream, uint16_t *value_out) {
    bitstream->cached_cursor = -1;
    return fil_stream_read_u16(bitstream->stream, value_out);
}

int fil_bitstream_read_aligned_u8(FilBitStream *bitstream, uint8_t *value_out) {
    bitstream->cached_cursor = -1;
    return fil_stream_read_u8(bitstream->stream, value_out);
}
