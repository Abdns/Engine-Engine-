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
#include "Text.h"

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

    ui_context UI;
    bool32     SpinPaused;

    uint32 PauseButton;
    uint32 SpeedSlider;
    uint32 SpawnButton;
    uint32 ClearButton;
    uint32 EntityList;
};

#endif
