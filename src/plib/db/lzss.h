#ifndef FALLOUT_PLIB_DB_LZSS_H_
#define FALLOUT_PLIB_DB_LZSS_H_

#include <stdio.h>

int lzss_decode_to_buf(FILE* in, unsigned char* dest, unsigned int length);
void lzss_decode_to_file(FILE* in, FILE* out, unsigned int length);

#endif /* FALLOUT_PLIB_DB_LZSS_H_ */
