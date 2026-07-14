#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "Types.h"
#include "Memory.h"
#include "Entity.h"

struct game_state
{
    int    ToneHz;
    real32 tSine;

    memory_arena WorldArena;

    entities Entities;

    Vector3 CameraP;
    real32  CameraYaw;
    real32  CameraPitch;
    real32  LastMouseX;
    real32  LastMouseY;
};

#endif
