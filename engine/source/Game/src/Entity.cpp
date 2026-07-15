#include "Entity.h"
#include "Memory.h"
#include "RenderCommands.h"

internal void InitEntities(entities* Entities, memory_arena* WorldArena)
{
    Entities->Count = 0;
    Entities->MaxCount = 8;
    Entities->NextID = 0;
    Entities->ID = PushArray(WorldArena, Entities->MaxCount, uint32);
    Entities->Position = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->Velocity = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->Rotation = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->AngularVelocity = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->Tint = PushArray(WorldArena, Entities->MaxCount, Vector4);
    Entities->MeshID = PushArray(WorldArena, Entities->MaxCount, uint32);
    Entities->TextureID = PushArray(WorldArena, Entities->MaxCount, uint32);
}

internal Matrix4 EntityTransform(entities* Entities, uint32 Index)
{
    Vector3 P = Entities->Position[Index];
    Vector3 R = Entities->Rotation[Index];

    Matrix4 Rotate = Mat4Multiply(Mat4RotationY(R.Y), Mat4Multiply(Mat4RotationX(R.X), Mat4RotationZ(R.Z)));

    return Mat4Multiply(Mat4Translation(P.X, P.Y, P.Z), Rotate);
}

internal uint32 FindEntityIndex(entities* Entities, uint32 EntityID)
{
    for (uint32 Index = 0; Index < Entities->Count; ++Index)
    {
        if (Entities->ID[Index] == EntityID)
        {
            return Index;
        }
    }

    Assert(!"Entity not found");
    return 0;
}

internal uint32 AddEntity(entities* Entities, Vector3 Position, uint32 MeshID, uint32 TextureID)
{
    Assert(Entities->Count < Entities->MaxCount);
    uint32 Index = Entities->Count++;
    uint32 EntityID = ++Entities->NextID;

    Entities->ID[Index]              = EntityID;
    Entities->Position[Index]        = Position;
    Entities->Rotation[Index]        = V3(0.0f, 0.0f, 0.0f);
    Entities->Velocity[Index]        = V3(0.0f, 0.0f, 0.0f);
    Entities->AngularVelocity[Index] = V3(0.0f, 0.0f, 0.0f);
    Entities->Tint[Index]            = V4(1.0f, 1.0f, 1.0f, 1.0f);
    Entities->MeshID[Index]          = MeshID;
    Entities->TextureID[Index]       = TextureID;

    return EntityID;
}

internal void SetEntityAngularVelocity(entities* Entities, uint32 EntityID, Vector3 AngularVelocity)
{
    Entities->AngularVelocity[FindEntityIndex(Entities, EntityID)] = AngularVelocity;
}

internal void SetEntityTint(entities* Entities, uint32 EntityID, Vector4 Tint)
{
    Entities->Tint[FindEntityIndex(Entities, EntityID)] = Tint;
}

internal void UpdateEntities(entities* Entities, real32 dt)
{
    for (uint32 EntityIndex = 0; EntityIndex < Entities->Count; ++EntityIndex)
    {
        Entities->Position[EntityIndex] += dt * Entities->Velocity[EntityIndex];
        Entities->Rotation[EntityIndex] += dt * Entities->AngularVelocity[EntityIndex];
    }
}

internal void PushEntitiesToRender(entities* Entities, render_commands* Commands)
{
    for (uint32 EntityIndex = 0; EntityIndex < Entities->Count; ++EntityIndex)
    {
        PushRenderMesh(Commands, EntityTransform(Entities, EntityIndex), Entities->Tint[EntityIndex], Entities->MeshID[EntityIndex], Entities->TextureID[EntityIndex]);
    }
}
