#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "Types.h"
#include "Memory.h"
#include "Entity.h"
#include "AssetPack.h"

struct game_state
{
    real32 tSine;

    memory_arena WorldArena;

    game_assets Assets;

    entities Entities;

    Vector3 CameraP;
    real32  CameraYaw;
    real32  CameraPitch;
    real32  LastMouseX;
    real32  LastMouseY;
};

#endif
