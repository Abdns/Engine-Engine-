#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "Types.h"
#include "Memory.h"

struct game_state
{
    real32 tSine;

    memory_arena WorldArena;
    memory_arena FrameArena;

    asset_store    Assets;
    materials      Materials;
    world         *World;
    entity_storage Storage;
    physics_state  Physics;

    uint32 SkyHandle;
    uint32 FontHandle;

    uint32 SpawnMeshHandles[2];
    uint32 SpawnMaterialHandles[3];

    camera Camera;

    uint32 SelectedStorageIndex;

    ui_context UI;
    bool32     Paused;
};

#endif
