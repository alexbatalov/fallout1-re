#include "plib/gnw/mmx.h"

#include <string.h>

#include "plib/gnw/svga.h"

// Return `true` if CPU supports MMX.
//
// 0x4CD640
bool mmxTest()
{
    int v1;

    // TODO: There are other ways to determine MMX using FLAGS register.

    __asm
    {
        mov eax, 1
        cpuid
        and edx, 0x800000
        mov v1, edx
    }

    return v1 != 0;
}

// 0x4CDB50
void srcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height)
{
    if (mmxEnabled) {
        // TODO: Blit with MMX.
        mmxEnabled = false;
        srcCopy(dest, destPitch, src, srcPitch, width, height);
        mmxEnabled = true;
    } else {
        for (int y = 0; y < height; y++) {
            memcpy(dest, src, width);
            dest += destPitch;
            src += srcPitch;
        }
    }
}

// 0x4CDC75
void transSrcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height)
{
    if (mmxEnabled) {
        // TODO: Blit with MMX.
        mmxEnabled = false;
        transSrcCopy(dest, destPitch, src, srcPitch, width, height);
        mmxEnabled = true;
    } else {
        int destSkip = destPitch - width;
        int srcSkip = srcPitch - width;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                unsigned char c = *src++;
                if (c != 0) {
                    *dest = c;
                }
                dest++;
            }
            src += srcSkip;
            dest += destSkip;
        }
    }
}
