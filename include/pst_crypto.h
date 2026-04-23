#ifndef PST_CRYPTO_H
#define PST_CRYPTO_H

#include <stdint.h>

void pst_crypt_permute(uint8_t *pv, int cb, int fEncrypt);
size_t lzfu_get_size(const uint8_t *buf, size_t size);
int lzfu_decompress_raw(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size);
int lzfu_decompress_into(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size);

#endif