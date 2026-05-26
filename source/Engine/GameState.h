#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "Types.h"

struct game_state
{
    int    ToneHz;
    real32 tSine;

    // Day 027: позиция игрока в пикселях экрана.
    real32 PlayerX;
    real32 PlayerY;
};

#endif
