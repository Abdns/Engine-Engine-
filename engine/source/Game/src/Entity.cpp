#include "Entity.h"
#include "Memory.h"

internal void InitEntities(entities* Entities, memory_arena* WorldArena)
{
    Entities->Count = 0;
    Entities->MaxCount = MAX_ENTITIES;
    Entities->NextID = 0;
    Entities->ID = PushArray(WorldArena, Entities->MaxCount, uint32);
    Entities->Position = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->Velocity = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->Rotation = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->AngularVelocity = PushArray(WorldArena, Entities->MaxCount, Vector3);
    Entities->Tint = PushArray(WorldArena, Entities->MaxCount, Vector4);
    Entities->MeshHandle = PushArray(WorldArena, Entities->MaxCount, uint32);
    Entities->MaterialHandle = PushArray(WorldArena, Entities->MaxCount, uint32);
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

internal uint32 AddEntity(entities* Entities, Vector3 Position, uint32 MeshHandle, uint32 MaterialHandle)
{
    if (Entities->Count >= Entities->MaxCount)
    {
        DebugLog("Entity storage is full (%u entities)\n", Entities->MaxCount);
        return 0;
    }

    uint32 Index = Entities->Count++;
    uint32 EntityID = ++Entities->NextID;

    Entities->ID[Index]              = EntityID;
    Entities->Position[Index]        = Position;
    Entities->Rotation[Index]        = Vector3(0.0f, 0.0f, 0.0f);
    Entities->Velocity[Index]        = Vector3(0.0f, 0.0f, 0.0f);
    Entities->AngularVelocity[Index] = Vector3(0.0f, 0.0f, 0.0f);
    Entities->Tint[Index]            = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    Entities->MeshHandle[Index]      = MeshHandle;
    Entities->MaterialHandle[Index]  = MaterialHandle;

    return EntityID;
}

internal void SetEntityAngularVelocity(entities* Entities, uint32 EntityID, Vector3 AngularVelocity)
{
    if (!EntityID)
    {
        return;
    }

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
