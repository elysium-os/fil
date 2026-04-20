#include "fil_private.h"

#define ID_1 0x1F
#define ID_2 0x8B

#define FLG_FTEXT (1 << 0)
#define FLG_FHCRC (1 << 1)
#define FLG_FEXTRA (1 << 2)
#define FLG_FNAME (1 << 3)
#define FLG_FCOMMENT (1 << 4)

#define CM_DEFLATE 8

static FilResult gzip_decompress_member(FilStream *input_stream, FilBuffer *output_buffer) {
    uint8_t id_1;
    if(!fil_stream_read_u8(input_stream, &id_1)) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    uint8_t id_2;
    if(!fil_stream_read_u8(input_stream, &id_2)) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    if(id_1 != ID_1 || id_2 != ID_2) {
        return FIL_RESULT_INVALID_HEADER;
    }

    uint8_t cm;
    if(!fil_stream_read_u8(input_stream, &cm)) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    uint8_t flg;
    if(!fil_stream_read_u8(input_stream, &flg)) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    // Skip mtime
    if(!fil_stream_advance(input_stream, 4)) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    // Skip XFL
    if(!fil_stream_advance(input_stream, 1)) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    // Skip OS
    if(!fil_stream_advance(input_stream, 1)) {
        return FIL_RESULT_UNEXPECTED_EOF;
    }

    if((flg & FLG_FEXTRA) != 0) {
        uint16_t xlen;
        if(!fil_stream_read_u16(input_stream, &xlen)) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }

        if(!fil_stream_advance(input_stream, xlen)) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }
    }

    if((flg & FLG_FNAME) != 0) {
        while(true) {
            uint8_t byte;
            if(!fil_stream_read_u8(input_stream, &byte)) {
                return FIL_RESULT_UNEXPECTED_EOF;
            }
            if(byte == '\0') break;
        }
    }

    if((flg & FLG_FCOMMENT) != 0) {
        while(true) {
            uint8_t byte;
            if(!fil_stream_read_u8(input_stream, &byte)) {
                return FIL_RESULT_UNEXPECTED_EOF;
            }
            if(byte == '\0') break;
        }
    }

    if((flg & FLG_FHCRC) != 0) {
        uint16_t crc16;
        if(!fil_stream_read_u16(input_stream, &crc16)) {
            return FIL_RESULT_UNEXPECTED_EOF;
        }
    }

    switch(cm) {
        case CM_DEFLATE: {
            FilBitStream bitstream = fil_bitstream_new(input_stream);
            int result = fil_deflate(&bitstream, output_buffer);
            if(result != FIL_RESULT_OK) {
                return result;
            }
            break;
        }
        default: return FIL_RESULT_UNSUPPORTED_FEATURE;
    }

    return FIL_RESULT_OK;
}

FilResult fil_gzip_decompress(FilStream *input_stream, FilBuffer *output_buffer) {
    return gzip_decompress_member(input_stream, output_buffer);
}
