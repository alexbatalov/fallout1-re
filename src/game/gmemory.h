#ifndef FALLOUT_GAME_GMEMORY_H_
#define FALLOUT_GAME_GMEMORY_H_

#include <stddef.h>

int gmemory_init();
void* gmalloc(size_t size);
void* grealloc(void* ptr, size_t newSize);
void gfree(void* ptr);

#endif /* FALLOUT_GAME_GMEMORY_H_ */
