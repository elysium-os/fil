#include "fil_private.h"

#include <assert.h>
#include <string.h>

#define MAX_CODE_LENGTH 15
#define MAX_CLEN_CODE_LENGTH 7

#define CLEN_ALPHABET_MAX_SIZE 19
#define LITERAL_ALPHABET_MAX_SIZE 288
#define DISTANCE_ALPHABET_MAX_SIZE 32

#define TREE_COUNTS 16

typedef struct {
    uint8_t length_counts[TREE_COUNTS];
    uint16_t sorted_symbols[CLEN_ALPHABET_MAX_SIZE];
} clen_tree_t;

typedef struct {
    uint8_t length_counts[TREE_COUNTS];
    uint16_t sorted_symbols[LITERAL_ALPHABET_MAX_SIZE];
} literal_tree_t;

typedef struct {
    uint8_t length_counts[TREE_COUNTS];
    uint16_t sorted_symbols[DISTANCE_ALPHABET_MAX_SIZE];
} distance_tree_t;

static const uint16_t g_base_length[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};

static const uint8_t g_length_extra_bits[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};

static const uint16_t g_base_dist[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
};

static const uint8_t g_dist_extra_bits[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};

static const uint8_t g_clen_alphabet_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
};

static FilResult build_huffman_tree(uint8_t *length_counts, uint16_t *sorted_symbols, [[maybe_unused]] size_t max_symbols, uint8_t *lengths, size_t symbol_count) {
    assert(symbol_count <= max_symbols);

    for(int i = 0; i < TREE_COUNTS; i++) {
        length_counts[i] = 0;
    }

    // Count codes per length
    for(size_t symbol = 0; symbol < symbol_count; symbol++) {
        uint8_t symbol_length = lengths[symbol];
        if(symbol_length == 0) continue;

        assert(symbol_length < TREE_COUNTS);

        length_counts[symbol_length] += 1;
    }

    // Sort symbols by length, then value
    uint16_t insert_offsets[TREE_COUNTS] = {};

    int offset = 0, available = 1;
    for(int length = 0; length < TREE_COUNTS; length++) {
        uint8_t count = length_counts[length];

        if(count > available) {
            return FIL_RESULT_MALFORMED;
        }
        available = (available - count) * 2;

        insert_offsets[length] = offset;
        offset += count;
    }

    if((offset > 1 && available > 0) || (offset == 1 && length_counts[1] != 1)) {
        return FIL_RESULT_MALFORMED;
    }

    for(size_t symbol = 0; symbol < symbol_count; symbol++) {
        uint8_t symbol_length = lengths[symbol];
        if(symbol_length == 0) continue;

        uint16_t offset = insert_offsets[symbol_length]++;

        assert(offset < max_symbols);

        sorted_symbols[offset] = symbol;
    }

    if(offset == 1) {
        length_counts[1] = 2;
        sorted_symbols[1] = UINT16_MAX;
    }

    return FIL_RESULT_OK;
}

static FilResult build_clen_tree(clen_tree_t *tree, uint8_t *lengths, size_t symbol_count) {
    return build_huffman_tree(tree->length_counts, tree->sorted_symbols, CLEN_ALPHABET_MAX_SIZE, lengths, symbol_count);
}

static FilResult build_literal_tree(literal_tree_t *tree, uint8_t *lengths, size_t symbol_count) {
    return build_huffman_tree(tree->length_counts, tree->sorted_symbols, LITERAL_ALPHABET_MAX_SIZE, lengths, symbol_count);
}

static FilResult build_distance_tree(distance_tree_t *tree, uint8_t *lengths, size_t symbol_count) {
    return build_huffman_tree(tree->length_counts, tree->sorted_symbols, DISTANCE_ALPHABET_MAX_SIZE, lengths, symbol_count);
}

static FilResult huffman_tree_decode(const uint8_t *length_counts, const uint16_t *sorted_symbols, FilBitStream *input_stream, uint16_t *out_symbol) {
    size_t bits;
    size_t available_bits = fil_bitstream_peek(input_stream, 15, &bits);

    if(available_bits == 0) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    size_t code = 0;
    size_t first_code = 0;
    size_t index = 0;
    for(size_t length = 1; length < TREE_COUNTS; length++) {
        if(length > available_bits) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }

        code = (code << 1) | ((bits >> (length - 1)) & 1);

        size_t count = length_counts[length];
        if(code - first_code < count) {
            if(fil_bitstream_advance(input_stream, length) != 0) {
                return FIL_RESULT_UNEXPECTED_EOF;
            }

            *out_symbol = sorted_symbols[index + (code - first_code)];
            return FIL_RESULT_OK;
        }

        first_code = (first_code + count) << 1;
        index += count;
    }

    return FIL_RESULT_MALFORMED;
}

static FilResult clen_tree_decode(const clen_tree_t *tree, FilBitStream *input_stream, uint16_t *out_symbol) {
    return huffman_tree_decode(tree->length_counts, tree->sorted_symbols, input_stream, out_symbol);
}

static FilResult literal_tree_decode(const literal_tree_t *tree, FilBitStream *input_stream, uint16_t *out_symbol) {
    return huffman_tree_decode(tree->length_counts, tree->sorted_symbols, input_stream, out_symbol);
}

static FilResult distance_tree_decode(const distance_tree_t *tree, FilBitStream *input_stream, uint16_t *out_symbol) {
    return huffman_tree_decode(tree->length_counts, tree->sorted_symbols, input_stream, out_symbol);
}

static FilResult deflate_block_compressed(bool dynamic, FilBitStream *input_stream, FilBuffer *output_buffer) {
    FilResult result;

    uint8_t combined_lengths[LITERAL_ALPHABET_MAX_SIZE + DISTANCE_ALPHABET_MAX_SIZE] = {};

    size_t literal_count = LITERAL_ALPHABET_MAX_SIZE;
    size_t distance_count = DISTANCE_ALPHABET_MAX_SIZE;
    if(dynamic) {
        size_t hlit;
        if(fil_bitstream_read(input_stream, 5, &hlit) != 0) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }

        size_t hdist;
        if(fil_bitstream_read(input_stream, 5, &hdist) != 0) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }

        size_t hclen;
        if(fil_bitstream_read(input_stream, 4, &hclen) != 0) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }

        hlit += 257;
        hdist += 1;
        hclen += 4;

        uint8_t clen_lengths[CLEN_ALPHABET_MAX_SIZE] = {};
        for(size_t i = 0; i < hclen; i++) {
            size_t length;
            if(fil_bitstream_read(input_stream, 3, &length) != 0) {
                return FIL_RESULT_UNEXPECTED_EOF;
            }

            assert(length <= UINT8_MAX);

            clen_lengths[g_clen_alphabet_order[i]] = length;
        }

        clen_tree_t clen_tree = {};
        if((result = build_clen_tree(&clen_tree, clen_lengths, CLEN_ALPHABET_MAX_SIZE)) != FIL_RESULT_OK) {
            return result;
        }

        size_t total_length_count = hlit + hdist;
        for(size_t i = 0; i < total_length_count;) {
            uint16_t clen_symbol;
            if((result = clen_tree_decode(&clen_tree, input_stream, &clen_symbol)) != FIL_RESULT_OK) {
                return FIL_RESULT_OK;
            }
            assert(clen_symbol < 19);

            switch(clen_symbol) {
                case 0 ... 15: combined_lengths[i++] = clen_symbol; break;
                case 16:       {
                    size_t count;
                    if(fil_bitstream_read(input_stream, 2, &count) != 0) {
                        return FIL_RESULT_UNEXPECTED_EOF;
                    }

                    if(i + 3 + count >= LITERAL_ALPHABET_MAX_SIZE + DISTANCE_ALPHABET_MAX_SIZE) {
                        return FIL_RESULT_MALFORMED;
                    }

                    if(i == 0) {
                        return FIL_RESULT_MALFORMED;
                    }

                    uint8_t value = combined_lengths[i - 1];
                    for(size_t j = 0; j < 3 + count; j++, i++) {
                        combined_lengths[i] = value;
                    }
                    break;
                }
                case 17: {
                    size_t count;
                    if(fil_bitstream_read(input_stream, 3, &count) != 0) {
                        return FIL_RESULT_UNEXPECTED_EOF;
                    }

                    if(i + 3 + count >= LITERAL_ALPHABET_MAX_SIZE + DISTANCE_ALPHABET_MAX_SIZE) {
                        return FIL_RESULT_MALFORMED;
                    }

                    for(size_t j = 0; j < 3 + count; j++, i++) {
                        combined_lengths[i] = 0;
                    }
                    break;
                }
                case 18: {
                    size_t count;
                    if(fil_bitstream_read(input_stream, 7, &count) != 0) {
                        return FIL_RESULT_UNEXPECTED_EOF;
                    }

                    if(i + 11 + count >= LITERAL_ALPHABET_MAX_SIZE + DISTANCE_ALPHABET_MAX_SIZE) {
                        return FIL_RESULT_MALFORMED;
                    }

                    for(size_t j = 0; j < 11 + count; j++, i++) {
                        combined_lengths[i] = 0;
                    }
                    break;
                }
            }
        }

        literal_count = hlit;
        distance_count = hdist;
    } else {
        for(size_t i = 0; i < 144; i++) combined_lengths[i] = 8;
        for(size_t i = 144; i < 256; i++) combined_lengths[i] = 9;
        for(size_t i = 256; i < 280; i++) combined_lengths[i] = 7;
        for(size_t i = 280; i < 288; i++) combined_lengths[i] = 8;
        for(size_t i = 0; i < DISTANCE_ALPHABET_MAX_SIZE; i++) combined_lengths[LITERAL_ALPHABET_MAX_SIZE + i] = 5;
    }

    literal_tree_t literal_tree;
    if((result = build_literal_tree(&literal_tree, combined_lengths, literal_count)) != FIL_RESULT_OK) {
        return result;
    }

    distance_tree_t distance_tree;
    if(dynamic) {
        if((result = build_distance_tree(&distance_tree, &combined_lengths[literal_count], distance_count)) != FIL_RESULT_OK) {
            return result;
        }
    }

    while(true) {
        uint16_t literal_sym;
        if((result = literal_tree_decode(&literal_tree, input_stream, &literal_sym)) != FIL_RESULT_OK) {
            return result;
        }

        if(literal_sym == 256) break;

        if(literal_sym < 256) {
            assert(output_buffer->length <= output_buffer->capacity);
            if(output_buffer->length == output_buffer->capacity) {
                if((result = fil_buffer_expand(output_buffer, 1)) != FIL_RESULT_OK) {
                    return result;
                }
            }

            output_buffer->data[output_buffer->length++] = literal_sym;
            continue;
        }

        if(literal_sym == 286 || literal_sym == 287 || literal_sym == UINT16_MAX) {
            return FIL_RESULT_MALFORMED;
        }
        assert(literal_sym < 286);

        size_t extra_length = 0;
        size_t extra_length_bits = g_length_extra_bits[literal_sym - 257];
        if(extra_length_bits > 0) {
            if(fil_bitstream_read(input_stream, extra_length_bits, &extra_length) != 0) {
                return FIL_RESULT_UNEXPECTED_EOF;
            }
        }
        size_t extended_length = g_base_length[literal_sym - 257] + extra_length;

        uint16_t distance_sym;
        if(dynamic) {
            if((result = distance_tree_decode(&distance_tree, input_stream, &distance_sym)) != FIL_RESULT_OK) {
                return result;
            }
        } else {
            size_t raw_distance_sym;
            if(fil_bitstream_read(input_stream, 5, &raw_distance_sym) != 0) {
                return FIL_RESULT_UNEXPECTED_EOF;
            }

            uint16_t reversed_dist_sym = 0;
            for(size_t i = 0; i < 5; i++) {
                reversed_dist_sym = (reversed_dist_sym << 1) | (raw_distance_sym & 1);
                raw_distance_sym >>= 1;
            }
            distance_sym = reversed_dist_sym;
        }

        if(distance_sym == 30 || distance_sym == 31 || literal_sym == UINT16_MAX) {
            return FIL_RESULT_MALFORMED;
        }
        assert(distance_sym < 30);

        size_t extra_dist = 0;
        size_t extra_dist_bits = g_dist_extra_bits[distance_sym];
        if(extra_dist_bits > 0) {
            if(fil_bitstream_read(input_stream, extra_dist_bits, &extra_dist) != 0) {
                return FIL_RESULT_UNEXPECTED_EOF;
            }
        }
        size_t extended_distance = g_base_dist[distance_sym] + extra_dist;

        if(extended_distance > output_buffer->length) {
            return FIL_RESULT_MALFORMED;
        }

        size_t capacity_left = output_buffer->capacity - output_buffer->length;
        if(capacity_left < extended_length) {
            if(output_buffer->fn_expand == nullptr) {
                return FIL_RESULT_NOMEM;
            }

            if((result = fil_buffer_expand(output_buffer, extended_length - capacity_left)) != FIL_RESULT_OK) {
                return result;
            }
        }

        if(extended_distance == 1) {
            memset(&output_buffer->data[output_buffer->length], output_buffer->data[output_buffer->length - 1], extended_length);
            output_buffer->length += extended_length;
        } else {
            for(size_t i = 0; i < extended_length;) {
                size_t copy_length = extended_length - i;
                if(extended_distance < copy_length) copy_length = extended_distance;

                memcpy(&output_buffer->data[output_buffer->length], &output_buffer->data[output_buffer->length - extended_distance], copy_length);
                output_buffer->length += copy_length;
                i += copy_length;
            }
        }
    }

    return FIL_RESULT_OK;
}

static FilResult deflate_block_uncompressed(FilBitStream *input_stream, FilBuffer *output_buffer) {
    uint16_t length;
    if(fil_bitstream_read_aligned_u16(input_stream, &length) != 0) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    uint16_t negated_length;
    if(fil_bitstream_read_aligned_u16(input_stream, &negated_length) != 0) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    if(length != (~negated_length & 0xFFFF)) {
        return FIL_RESULT_CHECKSUM_MISMATCH;
    }

    if(output_buffer->capacity < output_buffer->length + length) {
        size_t diff = output_buffer->length + length - output_buffer->capacity;

        FilResult result = fil_buffer_expand(output_buffer, diff);
        if(result != FIL_RESULT_OK) {
            return result;
        }
    }

    for(size_t i = 0; i < length; i++) {
        uint8_t byte;
        if(fil_bitstream_read_aligned_u8(input_stream, &byte) != 0) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }

        output_buffer->data[output_buffer->length++] = byte;
    }

    return FIL_RESULT_OK;
}

FilResult fil_deflate(FilBitStream *input_stream, FilBuffer *output_buffer) {
    while(true) {
        size_t is_final;
        if(fil_bitstream_read(input_stream, 1, &is_final) != 0) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }

        size_t block_type;
        if(fil_bitstream_read(input_stream, 2, &block_type) != 0) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }

        int result;
        switch(block_type) {
            case 0b00: result = deflate_block_uncompressed(input_stream, output_buffer); break;
            case 0b01: result = deflate_block_compressed(false, input_stream, output_buffer); break;
            case 0b10: result = deflate_block_compressed(true, input_stream, output_buffer); break;
            default:   return FIL_RESULT_MALFORMED;
        }

        if(result != FIL_RESULT_OK) return result;

        if(is_final) break;
    }

    return FIL_RESULT_OK;
}

#ifdef FIL_FUZZ

#define OUTPUT_BUFFER_SIZE 0x20000

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static uint8_t output_data[OUTPUT_BUFFER_SIZE];

    Stream stream = fil_stream_new(data, size);
    BitStream bit_stream = fil_bitstream_new(&stream);

    Buffer output_buffer = (Buffer) { .capacity = OUTPUT_BUFFER_SIZE, .length = 0, .data = output_data };

    FilResult result = fil_deflate(&bit_stream, &output_buffer);

    return result == FIL_RESULT_OK;
}
#endif
