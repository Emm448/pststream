#ifndef PST_IO_H
#define PST_IO_H

#if defined(__linux__) || defined(__unix__)
    #ifndef _FILE_OFFSET_BITS
        #define _FILE_OFFSET_BITS 64
    #endif
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

#define	NO_OFFSET	-1
#define LZFU_MAGIC	0x75465A4C  // "LZFu"

static inline int pst_seek(FILE *stream, uint64_t offset, int whence) {
#if defined(_WIN32) || defined(_WIN64)
    return _fseeki64(stream, (int64_t)offset, whence);
#else
    return fseeko(stream, (off_t)offset, whence);
#endif
}

static inline uint64_t pst_tell(FILE *stream) {
#if defined(_WIN32) || defined(_WIN64)
    return (uint64_t)_ftelli64(stream);
#else
    return (uint64_t)ftello(stream);
#endif
}

int pst_seek(FILE *stream, uint64_t offset, int whence);
uint64_t pst_tell(FILE *stream);

uint64_t read_byte(FILE *f, int nByte, long int offset);
uint64_t read_buffer(const uint8_t *buf, int nByte, long int offset);
uint16_t read_le16(const uint8_t *p);

void print_filetime(uint8_t *buffer, uint16_t size);
void print_utf16le(uint8_t *buffer, size_t size);
void print_hex_dump(const void* buffer, size_t size);

void pst_print_property(uint16_t propType, uint8_t *buffer, size_t size);
void print_buffer_clean(uint8_t *buf, const size_t size);

size_t lzfu_get_size(const uint8_t *buf, size_t size);
int lzfu_decompress_into(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size);
#endif