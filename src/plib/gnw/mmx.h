#ifndef MMX_H
#define MMX_H

#include <stdbool.h>

bool mmxTest();
void srcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);
void transSrcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);

#endif /* MMX_H */
