#ifndef ENTITY_H
#define ENTITY_H

#include "Types.h"
#include "EngineMath.h"
#include "DataLake.h"

struct entity
{
    uint32 ID;
    uint32 Index;

    uint32 MeshHandle;
    uint32 MaterialHandle;
};

internal uint32 EntityIndexFromID(data_lake *Lake, uint32 EntityID)
{
    for (uint32 Index = 0; Index < Lake->EntityCount; ++Index)
    {
        if (Lake->EntityID[Index] == EntityID)
        {
            return Index;
        }
    }

    Assert(!"Entity not found");
    return 0;
}

internal entity EntityFromIndex(data_lake *Lake, uint32 Index)
{
    entity Result;
    Result.ID             = Lake->EntityID[Index];
    Result.Index          = Index;
    Result.MeshHandle     = Lake->EntityMeshHandle[Index];
    Result.MaterialHandle = Lake->EntityMaterialHandle[Index];

    return Result;
}

internal entity GetEntity(data_lake *Lake, uint32 EntityID)
{
    return EntityFromIndex(Lake, EntityIndexFromID(Lake, EntityID));
}

internal uint32 EntityMeshSlot(data_lake *Lake, entity Entity)
{
    if (!Entity.MeshHandle || Entity.MeshHandle > Lake->MeshCount)
    {
        return LAKE_INVALID_SLOT;
    }

    return Entity.MeshHandle - 1;
}

internal Matrix4 EntityTransform(data_lake *Lake, uint32 Index)
{
    Vector3 P = Lake->EntityPosition[Index];
    Vector3 R = Lake->EntityRotation[Index];

    Matrix4 Rotate = Mat4Multiply(Mat4RotationY(R.Y), Mat4Multiply(Mat4RotationX(R.X), Mat4RotationZ(R.Z)));

    return Mat4Multiply(Mat4Translation(P.X, P.Y, P.Z), Rotate);
}

internal uint32 AddEntity(data_lake *Lake, Vector3 Position, uint32 MeshHandle, uint32 MaterialHandle)
{
    if (Lake->EntityCount >= Lake->EntityCapacity)
    {
        DebugLog("Lake is full (%u entities)\n", Lake->EntityCapacity);
        return 0;
    }

    uint32 Index    = Lake->EntityCount++;
    uint32 EntityID = ++Lake->EntityNextID;

    Lake->EntityID[Index]              = EntityID;
    Lake->EntityPosition[Index]        = Position;
    Lake->EntityRotation[Index]        = Vector3(0.0f, 0.0f, 0.0f);
    Lake->EntityVelocity[Index]        = Vector3(0.0f, 0.0f, 0.0f);
    Lake->EntityAngularVelocity[Index] = Vector3(0.0f, 0.0f, 0.0f);
    Lake->EntityTint[Index]            = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    Lake->EntityMeshHandle[Index]      = MeshHandle;
    Lake->EntityMaterialHandle[Index]  = MaterialHandle;

    return EntityID;
}

internal void SetEntityAngularVelocity(data_lake *Lake, uint32 EntityID, Vector3 AngularVelocity)
{
    if (!EntityID)
    {
        return;
    }

    Lake->EntityAngularVelocity[EntityIndexFromID(Lake, EntityID)] = AngularVelocity;
}

internal void SetEntityTint(data_lake *Lake, uint32 EntityID, Vector4 Tint)
{
    Lake->EntityTint[EntityIndexFromID(Lake, EntityID)] = Tint;
}

internal void UpdateEntities(data_lake *Lake, real32 dt)
{
    for (uint32 EntityIndex = 0; EntityIndex < Lake->EntityCount; ++EntityIndex)
    {
        Lake->EntityPosition[EntityIndex] += dt * Lake->EntityVelocity[EntityIndex];
        Lake->EntityRotation[EntityIndex] += dt * Lake->EntityAngularVelocity[EntityIndex];
    }
}

#endif
