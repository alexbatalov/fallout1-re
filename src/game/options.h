#ifndef FALLOUT_GAME_OPTIONS_H_
#define FALLOUT_GAME_OPTIONS_H_

#include <stdbool.h>

#include "plib/db/db.h"

int do_options();
int PauseWindow(bool is_world_map);
int init_options_menu();
int save_options(DB_FILE* stream);
int load_options(DB_FILE* stream);
void IncGamma();
void DecGamma();

#endif /* FALLOUT_GAME_OPTIONS_H_ */
