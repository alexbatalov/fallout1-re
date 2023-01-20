#include "game/version.h"

#include <stdio.h>

// 0x4A10C0
char* getverstr(char* dest)
{
    sprintf(dest, "FALLOUT %d.%d", VERSION_MAJOR, VERSION_MINOR);
    return dest;
}
