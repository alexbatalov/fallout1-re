#ifndef FALLOUT_GAME_ELEVATOR_H_
#define FALLOUT_GAME_ELEVATOR_H_

typedef enum Elevator {
    ELEVATOR_BROTHERHOOD_OF_STEEL_MAIN,
    ELEVATOR_BROTHERHOOD_OF_STEEL_SURFACE,
    ELEVATOR_MASTER_UPPER,
    ELEVATOR_MASTER_LOWER,
    ELEVATOR_MILITARY_BASE_UPPER,
    ELEVATOR_MILITARY_BASE_LOWER,
    ELEVATOR_GLOW_UPPER,
    ELEVATOR_GLOW_LOWER,
    ELEVATOR_VAULT_13,
    ELEVATOR_NECROPOLIS,
    ELEVATOR_SIERRA_1,
    ELEVATOR_SIERRA_2,
    ELEVATOR_COUNT,
} Elevator;

int elevator_select(int elevator, int* mapPtr, int* elevationPtr, int* tilePtr);

#endif /* FALLOUT_GAME_ELEVATOR_H_ */
