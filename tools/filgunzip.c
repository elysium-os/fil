#include "fil.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *g_error_messages[] = {
    [FIL_RESULT_OK] = "OK",
    [FIL_RESULT_UNSUPPORTED_FEATURE] = "Unsupported feature",
    [FIL_RESULT_INVALID_HEADER] = "Invalid header",
    [FIL_RESULT_MALFORMED] = "Malformed data",
    [FIL_RESULT_CHECKSUM_MISMATCH] = "Checksum mismatch",
    [FIL_RESULT_UNEXPECTED_EOF] = "Unexpected end of file",
    [FIL_RESULT_NOMEM] = "Out of memory",
};

static void *expand(void *cur, size_t size) {
    return realloc(cur, size);
}

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "usage: %s <input.gz> <output>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if(fd < 0) {
        fprintf(stderr, "error: cannot open '%s'\n", argv[1]);
        return 1;
    }

    struct stat st;
    if(fstat(fd, &st) != 0) {
        fprintf(stderr, "error: cannot stat '%s'\n", argv[1]);
        close(fd);
        return 1;
    }

    void *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if(data == MAP_FAILED) {
        fprintf(stderr, "error: cannot mmap '%s'\n", argv[1]);
        return 1;
    }

    FilStream stream = fil_stream_new(data, st.st_size);
    FilBuffer output = fil_buffer_new_dynamic(expand);

    FilResult result = fil_gzip_decompress(&stream, &output);
    munmap(data, st.st_size);

    if(result != FIL_RESULT_OK) {
        fprintf(stderr, "error: decompression failed `%s`\n", g_error_messages[result]);
        free(output.data);
        return 1;
    }

    FILE *output_file = fopen(argv[2], "wb");
    if(!output_file) {
        fprintf(stderr, "error: cannot open '%s'\n", argv[2]);
        free(output.data);
        return 1;
    }

    if(output.length > 0 && fwrite(output.data, output.length, 1, output_file) != 1) {
        fprintf(stderr, "error: failed to write '%s'\n", argv[2]);
        fclose(output_file);
        free(output.data);
        return 1;
    }

    fclose(output_file);
    free(output.data);

    return 0;
}
