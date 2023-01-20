#ifndef FALLOUT_GAME_ARTLOAD_H_
#define FALLOUT_GAME_ARTLOAD_H_

#include "game/art.h"
#include "plib/db/db.h"

int load_frame(const char* path, Art** artPtr);
int load_frame_into(const char* path, unsigned char* data);
int art_writeSubFrameData(unsigned char* data, DB_FILE* stream, int count);
int art_writeFrameData(Art* art, DB_FILE* stream);
int save_frame(const char* path, unsigned char* data);

#endif /* FALLOUT_GAME_ARTLOAD_H_ */
