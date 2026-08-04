#include "Entity.h"
#include "Memory.h"
#include "DataLake.h"
#include "Physics.h"
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
    Assert(Entities->Count < Entities->MaxCount);
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

internal uint32 BuildEntityColliders(entities* Entities, data_lake* Lake, collider* Colliders, uint32 MaxColliders)
{
    uint32 Count = 0;

    for (uint32 EntityIndex = 0; EntityIndex < Entities->Count && Count < MaxColliders; ++EntityIndex)
    {
        uint32 MeshHandle = Entities->MeshHandle[EntityIndex];
        if (!MeshHandle || MeshHandle > Lake->MeshCount)
        {
            continue;
        }

        uint32 Slot = MeshHandle - 1;

        collider* Current = Colliders + Count++;
        Current->Handle    = Entities->ID[EntityIndex];
        Current->Transform = EntityTransform(Entities, EntityIndex);

        Current->Mesh.Vertices     = LakeMeshVertices(Lake, Slot);
        Current->Mesh.VertexStride = sizeof(enga_vertex);
        Current->Mesh.Indices      = LakeMeshIndices(Lake, Slot);
        Current->Mesh.IndexCount   = Lake->MeshIndexCount[Slot];
        Current->Mesh.BoundsMin    = Lake->MeshBoundsMin[Slot];
        Current->Mesh.BoundsMax    = Lake->MeshBoundsMax[Slot];
    }

    return Count;
}

internal void PushEntitiesToRender(entities* Entities, render_commands* Commands, uint32 SelectedID)
{
    for (uint32 EntityIndex = 0; EntityIndex < Entities->Count; ++EntityIndex)
    {
        Vector4 Tint = Entities->Tint[EntityIndex];
        if (SelectedID && Entities->ID[EntityIndex] == SelectedID)
        {
            Tint = Vector4(1.0f, 0.85f, 0.2f, Tint.W);
        }

        PushRenderMesh(Commands, EntityTransform(Entities, EntityIndex), Tint, Entities->MeshHandle[EntityIndex], Entities->MaterialHandle[EntityIndex]);
    }
}
