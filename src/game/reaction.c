#include "game/reaction.h"

#include "game/game.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/scripts.h"
#include "game/stat.h"

// 0x490C40
int reaction_set(Object* critter, int value)
{
    scr_set_local_var(critter->sid, 0, value);
    return 0;
}

// 0x490C54
int level_to_reaction()
{
    return 0;
}

// 0x490C58
int reaction_to_level_internal(int sid, int reaction)
{
    int level;

    if (reaction > 75) {
        scr_set_local_var(sid, 1, 3);
        level = 2;
    } else if (reaction > 25) {
        scr_set_local_var(sid, 1, 2);
        level = 1;
    } else {
        scr_set_local_var(sid, 1, 1);
        level = 0;
    }

    return 0;
}

// 0x490CA0
int reaction_to_level(int reaction)
{
    if (reaction > 75) {
        return 2;
    } else if (reaction > 25) {
        return 1;
    } else {
        return 0;
    }
}

// 0x490CA8
int reaction_roll(int a1, int a2, int a3)
{
    return 0;
}

// 0x490CA8
int reaction_influence(int a1, int a2, int a3)
{
    return 0;
}

// 0x490CBC
int reaction_get(Object* critter)
{
    int sid;
    int v1 = 0;
    int v2 = 0;

    sid = critter->sid;
    if (scr_get_local_var(sid, 2, &v1) == -1) {
        return -1;
    }

    if (v1 != 0) {
        if (scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        return v2;
    }

    scr_set_local_var(sid, 0, 50);
    scr_set_local_var(sid, 1, 2);
    scr_set_local_var(sid, 2, 1);

    if (scr_get_local_var(sid, 0, &v2) == -1) {
        return -1;
    }

    scr_set_local_var(sid, 0, v2 + 5 * stat_level(obj_dude, STAT_CHARISMA) - 25);

    if (scr_get_local_var(sid, 0, &v2) == -1) {
        return -1;
    }

    scr_set_local_var(sid, 0, v2 + 10 * perk_level(PERK_PRESENCE));

    if (perk_level(PERK_CULT_OF_PERSONALITY) > 0) {
        if (scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        if (game_get_global_var(GVAR_PLAYER_REPUATION) > 0) {
            scr_set_local_var(sid, 0, game_get_global_var(GVAR_PLAYER_REPUATION) + v2);
        } else {
            scr_set_local_var(sid, 0, v2 - game_get_global_var(GVAR_PLAYER_REPUATION));
        }
    } else {
        if (scr_get_local_var(sid, 3, &v1) == -1) {
            return -1;
        }

        if (scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        if (v1 != 1) {
            scr_set_local_var(sid, 0, game_get_global_var(GVAR_PLAYER_REPUATION) + v2);
        } else {
            scr_set_local_var(sid, 0, v2 - game_get_global_var(GVAR_PLAYER_REPUATION));
        }
    }

    if (game_get_global_var(GVAR_CHILDKILLER_REPUATION) > 2) {
        if (scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        scr_set_local_var(sid, 0, v2 - 30);
    }

    if (game_get_global_var(GVAR_BAD_MONSTER) > 3 * game_get_global_var(GVAR_GOOD_MONSTER) || game_get_global_var(GVAR_CHAMPION_REPUTATION) == 1) {
        if (scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        scr_set_local_var(sid, 0, v2 + 20);
    }

    if (game_get_global_var(GVAR_GOOD_MONSTER) > 2 * game_get_global_var(GVAR_BAD_MONSTER) || game_get_global_var(GVAR_BERSERKER_REPUTATION) == 1) {
        if (scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        scr_set_local_var(sid, 0, v2 - 20);
    }

    if (scr_get_local_var(sid, 0, &v2) == -1) {
        return -1;
    }

    reaction_to_level_internal(sid, v2);

    if (scr_get_local_var(sid, 0, &v2) == -1) {
        return -1;
    }

    return v2;
}
