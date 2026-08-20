#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "Types.h"
#include "Memory.h"

struct game_state
{
    real32 tSine;

    memory_arena WorldArena;
    data_lake Lake;
    materials Materials;

    uint32 SkyHandle;
    uint32 FontHandle;

    uint32 SpawnMeshHandles[2];
    uint32 SpawnMaterialHandles[3];

    camera Camera;

    collider *Colliders;
    uint32    ColliderCapacity;
    uint32    SelectedEntityID;

    physics_world Physics;

    ui_context UI;
    bool32     Paused;

    uint32 PauseButton;
    uint32 SpawnButton;
    uint32 ClearButton;
};

#endif
