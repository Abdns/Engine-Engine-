#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "Types.h"
#include "Memory.h"
#include "Entity.h"
#include "DataLake.h"

struct game_state
{
    real32 tSine;

    memory_arena WorldArena;
    data_lake Lake;
    entities Entities;

    uint32 SkyHandle;

    Vector3 CameraP;
    real32  CameraYaw;
    real32  CameraPitch;
    real32  LastMouseX;
    real32  LastMouseY;
};

#endif
