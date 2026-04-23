#include "pst_io.h"
#include "pst_types.h"

#include <string.h>

void print_utf16le(uint8_t *buffer, size_t size) {
    if (size % 2 != 0) size--; //safety

    for (size_t i = 0; i < size; i += 2) {
        uint16_t c = buffer[i] | (buffer[i+1] << 8);

        if (c >= 32 && c < 127) {
            printf("%c", (char)c);
        }
    }
    printf("\n");
}

void print_filetime(uint8_t *buffer, uint16_t size){
    if (size < 8) return;

    uint64_t ft = *(uint64_t*)buffer;

    //FILETIME to Unix epoch
    uint64_t seconds = ft / 10000000ULL;
    if (seconds < 11644473600ULL) return;
    seconds -= 11644473600ULL;

    //time (hh:mm:ss)
    uint64_t days = seconds / 86400;
    uint64_t rem  = seconds % 86400;

    int hour = rem / 3600;
    rem %= 3600;
    int min = rem / 60;
    int sec = rem % 60;

    //date (dd-mm-yyyy)
    int year = 1970;

    while (1) {
        int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        int d = leap ? 366 : 365;

        if (days < d) break;
        days -= d;
        year++;
    }

    static const int month_days[] = {
        31,28,31,30,31,30,31,31,30,31,30,31
    };

    int month = 0;
    while (1) {
        int d = month_days[month];

        if (month == 1) {
            int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
            if (leap) d = 29;
        }

        if (days < d) break;

        days -= d;
        month++;
    }

    int day = days + 1;

    printf("%02d-%02d-%04d %02d:%02d:%02d UTC",
        day,
        month + 1,
        year,
        hour,
        min,
        sec
    );
}

uint64_t read_byte(FILE *f, int nByte, long int offset){
	uint8_t b[8];
	
	if (nByte > 8)
		return 0;
	
	if (offset != NO_OFFSET)
		pst_seek(f, offset, SEEK_SET);
	
	if (fread(b, 1, nByte, f) != nByte)
		return 0;

	uint64_t res = 0;
	for (size_t i = 0; i < nByte; i++)
		res |= ((uint64_t)b[i]) << (i * 8);

    return res;
}

uint64_t read_buffer(const uint8_t *buf, int nByte, long int offset){
    uint64_t val = 0;

    for (int i = 0; i < nByte; i++) {
        val |= ((uint64_t)buf[offset + i]) << (8 * i);
    }

    return val;
}

uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

void print_hex_dump(const void* buffer, size_t size){
    const unsigned char* p = (const unsigned char*)buffer;
    size_t i, j;

    for (i = 0; i < size; i += 16) {
        //OFFSET
        printf("%08zx  ", i);

        //HEX BYTES
        for (j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02x ", p[i + j]);
            } else {
                printf("   ");
            }
            if (j == 7) printf(" ");
        }

        printf(" |");

        //ASCII
        for (j = 0; j < 16; j++) {
            if (i + j < size) {
                unsigned char c = p[i + j];
                printf("%c", isprint(c) ? c : '.');
            }
        }
        printf("|\n");
    }
}

void print_buffer_clean(uint8_t *buf, const size_t size){
    for (size_t i = 0; i < size; i++) {
        unsigned char c = buf[i];

        if (c >= 32 && c <= 126) {
            putchar(c);
        } else if (c == '\n' || c == '\r' || c == '\t') {
            putchar(c);
        }

    }
    putchar('\n');
}

size_t lzfu_get_size(const uint8_t *buf, size_t size) {
    const uint32_t *hdr = (const uint32_t *)buf;

    if (hdr[2] != LZFU_MAGIC)
        return 0;

    return hdr[1]; //uncompressedSize
}

int lzfu_decompress_raw(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size) {
    uint8_t window[4096];
    memset(window, 0, sizeof(window));

    size_t win_pos = 0;
    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < in_size) {
        uint8_t flags = in[in_pos++];

        for (int i = 0; i < 8; i++) {
            if (in_pos >= in_size)
                break;

            if (flags & (1 << i)) {
                //reference
                if (in_pos + 1 >= in_size)
                    return -2;

                uint8_t b1 = in[in_pos++];
                uint8_t b2 = in[in_pos++];

                uint16_t offset = ((uint16_t)(b2 & 0xF0) << 4) | b1;
                uint16_t length = (b2 & 0x0F) + 2;

                for (uint16_t j = 0; j < length; j++) {
                    uint8_t c = window[(offset + j) & 0x0FFF];

                    if (out_pos >= out_size)
                        return -3;

                    out[out_pos++] = c;
                    window[win_pos] = c;
                    win_pos = (win_pos + 1) & 0x0FFF;
                }
            } else {
                //literal
                uint8_t c = in[in_pos++];

                if (out_pos >= out_size)
                    return -3;

                out[out_pos++] = c;
                window[win_pos] = c;
                win_pos = (win_pos + 1) & 0x0FFF;
            }
        }
    }

    return (int)out_pos;
}

int lzfu_decompress_into(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size) {
    if (!in || !out || in_size < 16)
        return -1;

    //header PST LZFu
    uint32_t compressed_size = *(const uint32_t *)(in + 0);
    uint32_t uncompressed_size = *(const uint32_t *)(in + 4);
    uint32_t magic = *(const uint32_t *)(in + 8);

    //"LZFu"
    if (magic != 0x75465A4C) {
        size_t to_copy = in_size < out_size ? in_size : out_size;
        memcpy(out, in, to_copy);
        return (int)to_copy;
    }

    const uint8_t *data = in + 16;
    size_t data_size = in_size - 16;

    //sanity check
    if (compressed_size != 0 && compressed_size != data_size)
        return -2;

    int ret = lzfu_decompress_raw(data, data_size, out, out_size);
    if (ret < 0)
        return ret;

    if (uncompressed_size != 0 && (uint32_t)ret != uncompressed_size) {
		return -3;
    }

    return ret;
}


void pst_print_property(uint16_t propType, uint8_t *buffer, size_t size){
    switch (propType){
        case PT_UTF16LE:
            print_utf16le(buffer, size);
            break;

        case PT_ASCII:
            printf("%.*s", size, buffer);
            break;

        case PT_BINARY:
			size_t out_size = lzfu_get_size(buffer, size);
			if(out_size == 0){
				print_buffer_clean(buffer, size);
			}else{
				uint8_t out_buffer[out_size];
				int written = lzfu_decompress_into(buffer, size, out_buffer, out_size);
				print_buffer_clean(buffer, size);
			}
            break;
		
		case PT_TIMESTAMP:
			print_filetime(buffer, size);
			printf("\n");
			break;		
    }
}
