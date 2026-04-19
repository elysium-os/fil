#pragma once

#include "fil.h"

void fil_stream_advance(FilStream *stream, size_t byte_count);
int fil_stream_read_u8(FilStream *stream, uint8_t *value_out);
int fil_stream_read_u16(FilStream *stream, uint16_t *value_out);
int fil_stream_read_u32(FilStream *stream, uint32_t *value_out);
int fil_stream_read_u64(FilStream *stream, uint64_t *value_out);

int fil_bitstream_advance(FilBitStream *bitstream, size_t bit_count);
size_t fil_bitstream_peek(FilBitStream *bitstream, size_t bits, size_t *value_out);
int fil_bitstream_read(FilBitStream *bitstream, size_t bits, size_t *value_out);
int fil_bitstream_read_aligned_u8(FilBitStream *bitstream, uint8_t *value_out);
int fil_bitstream_read_aligned_u16(FilBitStream *bitstream, uint16_t *value_out);

FilResult fil_buffer_expand(FilBuffer *buffer, size_t minimum_amount);
