#include "fil_private.h"

#include <assert.h>
#include <stddef.h>

#define BYTE_SIZE 8
#define MAX_BITS (sizeof(size_t) * BYTE_SIZE)

size_t fil_bitstream_peek(FilBitStream *bitstream, size_t bit_count, size_t *value_out) {
    assert(bit_count <= MAX_BITS);

    size_t bits = 0;
    bool ok = false;

    size_t byte_count = 1;
    while(byte_count * BYTE_SIZE < bit_count) byte_count <<= 1;
    if(byte_count > sizeof(size_t)) byte_count = sizeof(size_t);

    while(byte_count > 0) {
        switch(byte_count) {
            case 1: ok = fil_stream_peek_u8(bitstream->stream, 0, (uint8_t *) &bits); break;
            case 2: ok = fil_stream_peek_u16(bitstream->stream, 0, (uint16_t *) &bits); break;
            case 4: ok = fil_stream_peek_u32(bitstream->stream, 0, (uint32_t *) &bits); break;
            case 8: ok = fil_stream_peek_u64(bitstream->stream, 0, (uint64_t *) &bits); break;
        }
        if(ok) break;
        byte_count >>= 1;
    }

    if(!ok) {
        if(bitstream->cached_cursor >= 0) {
            size_t cached_bits = BYTE_SIZE - bitstream->cached_cursor;
            if(cached_bits >= bit_count) {
                *value_out = (bitstream->cached_byte >> bitstream->cached_cursor) & (((size_t) 1 << bit_count) - 1);
                return cached_bits;
            }
        }
        return 0;
    }

    size_t available_bits = byte_count * BYTE_SIZE;
    if(bitstream->cached_cursor >= 0) {
        size_t offset = BYTE_SIZE - bitstream->cached_cursor;
        bits <<= offset;
        bits |= bitstream->cached_byte >> bitstream->cached_cursor;
        available_bits += offset;
        if(available_bits > MAX_BITS) available_bits = MAX_BITS;
    }

    *value_out = bits & (((size_t) 1 << bit_count) - 1);
    return available_bits;
}

bool fil_bitstream_advance(FilBitStream *bitstream, size_t bit_count) {
    if(bitstream->cached_cursor >= 0) {
        size_t cursor_offset = BYTE_SIZE - bitstream->cached_cursor;
        if(bit_count <= cursor_offset) {
            bitstream->cached_cursor += bit_count;
            return true;
        }
        bit_count -= cursor_offset;
    }

    size_t byte_count = bit_count / BYTE_SIZE;
    bit_count %= BYTE_SIZE;

    if(bit_count == 0) {
        if(!fil_stream_advance(bitstream->stream, byte_count)) return false;
        bitstream->cached_cursor = -1;
    } else {
        if(byte_count > 0 && !fil_stream_advance(bitstream->stream, byte_count)) return false;
        if(!fil_stream_read_u8(bitstream->stream, &bitstream->cached_byte)) return false;
        bitstream->cached_cursor = bit_count;
    }

    return true;
}

bool fil_bitstream_read(FilBitStream *bitstream, size_t bit_count, size_t *value_out) {
    assert(bit_count <= MAX_BITS);
    if(fil_bitstream_peek(bitstream, bit_count, value_out) < bit_count) return false;
    return fil_bitstream_advance(bitstream, bit_count);
}
