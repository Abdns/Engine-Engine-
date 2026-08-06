#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "Types.h"
#include "Memory.h"
#include "Camera.h"
#include "Entity.h"
#include "Material.h"
#include "DataLake.h"
#include "Physics.h"
#include "UI.h"

struct game_state
{
    real32 tSine;

    memory_arena WorldArena;
    data_lake Lake;
    materials Materials;

    uint32 SkyHandle;

    uint32 SpawnMeshHandles[2];
    uint32 SpawnMaterialHandles[3];

    camera Camera;

    collider *Colliders;
    uint32    ColliderCapacity;
    uint32    SelectedEntityID;

    ui_context UI;
    real32     SpinSpeed;
    bool32     SpinPaused;
};

#endif
