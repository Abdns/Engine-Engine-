#include "Entity.h"

internal void InitEntities(entities* Entities, memory_arena* WorldArena)
{
    Entities->Count = 0;
    Entities->MaxCount = 8;
    Entities->Position = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->Velocity = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->Rotation = PushArray(WorldArena, Entities->MaxCount, real32);
    Entities->MeshID = PushArray(WorldArena, Entities->MaxCount, uint32);
    Entities->TextureID = PushArray(WorldArena, Entities->MaxCount, uint32);
}

internal uint32 AddEntity(entities* Entities, Vector3 Position, uint32 MeshID, uint32 TextureID)
{
    Assert(Entities->Count < Entities->MaxCount);
    uint32 Index = Entities->Count++;

    Entities->Position[Index]  = Position;
    Entities->Velocity[Index]  = V3(0.0f, 0.0f, 0.0f);
    Entities->Rotation[Index]  = 0.0f;
    Entities->MeshID[Index]    = MeshID;
    Entities->TextureID[Index] = TextureID;

    return Index;
}

internal void UpdateEntities(entities* Entities, real32 dt)
{
    for (uint32 EntityIndex = 0; EntityIndex < Entities->Count; ++EntityIndex)
    {
        Entities->Position[EntityIndex] += dt * Entities->Velocity[EntityIndex];
        Entities->Rotation[EntityIndex] += dt;
    }
}
