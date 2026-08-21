#include "Types.h"
#include "EngineMath.h"
#include "Memory.h"
#include "Strings.h"

#define ENTITY_MAX_NAME       32
#define ENTITY_STORAGE_NONE   0

enum entity_type
{
    Entity_Null = 0,
    Entity_Floor,
    Entity_Prop,
    Entity_Ball,
};

enum entity_flag
{
    EntityFlag_Visible   = 0x1,
    EntityFlag_Simulates = 0x2,
    EntityFlag_Static    = 0x4,
};

struct sim_entity;

union sim_entity_reference
{
    sim_entity *Ptr;
    uint32      Index;
};

struct sim_entity
{
    uint32      StorageIndex;
    bool32      Updatable;

    entity_type Type;
    uint32      Flags;

    Vector3     P;
    Quaternion  Orientation;
    Vector3     dP;
    Vector3     dOrientation;

    Vector3     PrevP;
    Quaternion  PrevOrientation;

    real32      InvMass;
    real32      InvInertia;

    uint32      MeshHandle;
    uint32      MaterialHandle;
    Vector4     Tint;

    Vector3     BoundsMin;
    Vector3     BoundsMax;
};

struct low_entity
{
    world_position P;
    world_position PrevP;
    sim_entity     Sim;
    char           Name[ENTITY_MAX_NAME];
};

struct entity_storage
{
    low_entity *LowEntities;
    uint32      Count;
    uint32      Capacity;

    uint32     *FreeIndex;
    uint32      FreeCount;
};

internal void EntityStorageInit(entity_storage *Storage, memory_arena *Arena, uint32 Capacity)
{
    Storage->Capacity  = Capacity;
    Storage->Count     = 1;
    Storage->FreeCount = 0;

    Storage->LowEntities = PushArray(Arena, Capacity, low_entity);
    Storage->FreeIndex   = PushArray(Arena, Capacity, uint32);

    ZeroStruct(Storage->LowEntities[ENTITY_STORAGE_NONE]);
    Storage->LowEntities[ENTITY_STORAGE_NONE].P = NullWorldPosition();
}

internal low_entity *GetLowEntity(entity_storage *Storage, uint32 StorageIndex)
{
    if (StorageIndex == ENTITY_STORAGE_NONE || StorageIndex >= Storage->Count)
    {
        return 0;
    }

    return Storage->LowEntities + StorageIndex;
}

internal char *LowEntityName(entity_storage *Storage, uint32 StorageIndex)
{
    Assert(StorageIndex < Storage->Capacity);

    return Storage->LowEntities[StorageIndex].Name;
}

internal void ChangeEntityLocation(memory_arena *Arena, world *World, entity_storage *Storage, uint32 StorageIndex, world_position NewP)
{
    low_entity *Entity = Storage->LowEntities + StorageIndex;

    world_position *OldP = IsWorldPositionValid(Entity->P) ? &Entity->P : 0;
    world_position *NextP = IsWorldPositionValid(NewP) ? &NewP : 0;

    ChangeEntityLocationRaw(Arena, World, StorageIndex, OldP, NextP);

    Entity->P = NewP;
}

internal uint32 AddLowEntity(memory_arena *Arena, world *World, entity_storage *Storage, entity_type Type, world_position P, const char *Name)
{
    uint32 StorageIndex;

    if (Storage->FreeCount)
    {
        StorageIndex = Storage->FreeIndex[--Storage->FreeCount];
    }
    else
    {
        if (Storage->Count >= Storage->Capacity)
        {
            DebugLog("Entity storage is full (%u entities)\n", Storage->Capacity);
            return ENTITY_STORAGE_NONE;
        }

        StorageIndex = Storage->Count++;
    }

    low_entity *Entity = Storage->LowEntities + StorageIndex;

    ZeroStruct(*Entity);

    Entity->P                 = NullWorldPosition();
    Entity->PrevP             = P;
    Entity->Sim.StorageIndex  = StorageIndex;
    Entity->Sim.Type          = Type;
    Entity->Sim.Orientation   = QuatIdentity();
    Entity->Sim.PrevOrientation = QuatIdentity();
    Entity->Sim.Tint          = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    Entity->Name[0] = 0;
    if (Name)
    {
        AppendString(Entity->Name, ENTITY_MAX_NAME, 0, Name);
    }

    ChangeEntityLocation(Arena, World, Storage, StorageIndex, P);

    return StorageIndex;
}

internal void RemoveLowEntity(memory_arena *Arena, world *World, entity_storage *Storage, uint32 StorageIndex)
{
    low_entity *Entity = GetLowEntity(Storage, StorageIndex);
    if (!Entity || Entity->Sim.Type == Entity_Null)
    {
        return;
    }

    ChangeEntityLocation(Arena, World, Storage, StorageIndex, NullWorldPosition());

    Entity->Sim.Type  = Entity_Null;
    Entity->Sim.Flags = 0;
    Entity->Name[0]   = 0;

    Assert(Storage->FreeCount < Storage->Capacity);
    Storage->FreeIndex[Storage->FreeCount++] = StorageIndex;
}

internal uint32 FindLowEntityByName(entity_storage *Storage, const char *Name)
{
    Assert(Name && Name[0]);

    for (uint32 Index = 1; Index < Storage->Count; ++Index)
    {
        if (Storage->LowEntities[Index].Sim.Type != Entity_Null && StringsAreEqual(Storage->LowEntities[Index].Name, Name))
        {
            return Index;
        }
    }

    return ENTITY_STORAGE_NONE;
}
