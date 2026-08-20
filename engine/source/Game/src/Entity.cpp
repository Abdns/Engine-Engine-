#include "Types.h"
#include "EngineMath.h"
#include "Memory.h"
#include "Strings.h"

#define MAX_ENTITIES    64
#define MAX_ENTITY_NAME 32

enum entity_type
{
    Entity_None = 0,
    Entity_Mesh,
    Entity_UIButton,
    Entity_UISlider,
    Entity_UIList,
};

struct entity
{
    uint32      ID;
    entity_type Type;
    uint32      Slot;
};

internal void InitEntities(data_lake *Lake, memory_arena *Arena)
{
    Lake->EntityCount    = 0;
    Lake->EntityCapacity = MAX_ENTITIES;
    Lake->EntityNextID   = 0;
    Lake->Entities       = PushArray(Arena, Lake->EntityCapacity, entity);
    Lake->EntityNames    = PushArray(Arena, (memory_size)Lake->EntityCapacity * MAX_ENTITY_NAME, char);

    Lake->Transforms.Count           = 0;
    Lake->Transforms.Position        = PushArray(Arena, MAX_ENTITIES, Vector3);
    Lake->Transforms.PrevPosition    = PushArray(Arena, MAX_ENTITIES, Vector3);
    Lake->Transforms.Rotation        = PushArray(Arena, MAX_ENTITIES, Matrix4);
    Lake->Transforms.PrevRotation    = PushArray(Arena, MAX_ENTITIES, Matrix4);
    Lake->Transforms.Tint            = PushArray(Arena, MAX_ENTITIES, Vector4);
    Lake->Transforms.EntityID        = PushArray(Arena, MAX_ENTITIES, uint32);
    Lake->Transforms.MeshHandle      = PushArray(Arena, MAX_ENTITIES, uint32);
    Lake->Transforms.MaterialHandle  = PushArray(Arena, MAX_ENTITIES, uint32);
    Lake->Transforms.Static          = PushArray(Arena, MAX_ENTITIES, bool32);

    Lake->Widgets.Count   = 0;
    Lake->Widgets.RectMin = PushArray(Arena, MAX_ENTITIES, Vector2);
    Lake->Widgets.RectMax = PushArray(Arena, MAX_ENTITIES, Vector2);
    Lake->Widgets.Value   = PushArray(Arena, MAX_ENTITIES, real32);
}

internal char *EntityName(data_lake *Lake, entity *Entity)
{
    uint32 Index = (uint32)(Entity - Lake->Entities);

    return Lake->EntityNames + (memory_size)Index * MAX_ENTITY_NAME;
}

internal entity *GetEntity(data_lake *Lake, uint32 EntityID)
{
    for (uint32 Index = 0; Index < Lake->EntityCount; ++Index)
    {
        entity *Entity = Lake->Entities + Index;
        if (Entity->ID == EntityID)
        {
            return Entity;
        }
    }

    Assert(!"Entity not found");
    return Lake->Entities;
}

internal entity *GetEntityByName(data_lake *Lake, const char *Name)
{
    Assert(Name && Name[0]);

    for (uint32 Index = 0; Index < Lake->EntityCount; ++Index)
    {
        if (StringsAreEqual(Lake->EntityNames + (memory_size)Index * MAX_ENTITY_NAME, Name))
        {
            return Lake->Entities + Index;
        }
    }

    Assert(!"Entity not found");

    return Lake->Entities;
}

internal Matrix4 EntityLocalToWorld(data_lake *Lake, uint32 Slot)
{
    Assert(Slot < Lake->Transforms.Count);

    Vector3 Position = Lake->Transforms.Position[Slot];

    return Mat4Multiply(Mat4Translation(Position.X, Position.Y, Position.Z), Lake->Transforms.Rotation[Slot]);
}

internal Matrix4 EntityRenderTransform(data_lake *Lake, uint32 Slot, real32 Alpha)
{
    Assert(Slot < Lake->Transforms.Count);

    Vector3 Prev     = Lake->Transforms.PrevPosition[Slot];
    Vector3 Position = Prev + Alpha * (Lake->Transforms.Position[Slot] - Prev);
    Matrix4 Rotation = Mat4LerpRotation(Lake->Transforms.PrevRotation[Slot], Lake->Transforms.Rotation[Slot], Alpha);

    return Mat4Multiply(Mat4Translation(Position.X, Position.Y, Position.Z), Rotation);
}

internal void EntityUpdatePreviousTrasform(data_lake *Lake)
{
    for (uint32 Slot = 0; Slot < Lake->Transforms.Count; ++Slot)
    {
        Lake->Transforms.PrevPosition[Slot] = Lake->Transforms.Position[Slot];
        Lake->Transforms.PrevRotation[Slot] = Lake->Transforms.Rotation[Slot];
    }
}

internal entity *AddEntity(data_lake *Lake, const char *Name, entity_type Type)
{
    if (Lake->EntityCount >= Lake->EntityCapacity)
    {
        DebugLog("Lake is full (%u entities)\n", Lake->EntityCapacity);
        return 0;
    }

    if (Name && Name[0])
    {
        for (uint32 Index = 0; Index < Lake->EntityCount; ++Index)
        {
            if (StringsAreEqual(Lake->EntityNames + (memory_size)Index * MAX_ENTITY_NAME, Name))
            {
                DebugLog("Entity name '%s' is taken\n", Name);
                Assert(!"Entity name is taken");
                return 0;
            }
        }
    }

    entity *Entity = Lake->Entities + Lake->EntityCount++;
    Entity->ID   = ++Lake->EntityNextID;
    Entity->Type = Type;
    Entity->Slot = (Type == Entity_Mesh) ? Lake->Transforms.Count++ : Lake->Widgets.Count++;

    char *Dest = EntityName(Lake, Entity);
    Dest[0] = 0;
    if (Name)
    {
        AppendString(Dest, MAX_ENTITY_NAME, 0, Name);
    }

    return Entity;
}

internal entity *AddMeshEntity(data_lake *Lake, const char *Name, Vector3 Position, uint32 MeshHandle, uint32 MaterialHandle)
{
    entity *Entity = AddEntity(Lake, Name, Entity_Mesh);
    if (!Entity)
    {
        return 0;
    }

    Lake->Transforms.Position[Entity->Slot]       = Position;
    Lake->Transforms.PrevPosition[Entity->Slot]   = Position;
    Lake->Transforms.Rotation[Entity->Slot]       = Mat4Identity();
    Lake->Transforms.PrevRotation[Entity->Slot]   = Mat4Identity();
    Lake->Transforms.Tint[Entity->Slot]           = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    Lake->Transforms.EntityID[Entity->Slot]       = Entity->ID;
    Lake->Transforms.MeshHandle[Entity->Slot]     = MeshHandle;
    Lake->Transforms.MaterialHandle[Entity->Slot] = MaterialHandle;
    Lake->Transforms.Static[Entity->Slot]         = false;

    return Entity;
}

internal entity *AddWidgetEntity(data_lake *Lake, const char *Name, entity_type Type, Vector2 RectMin, Vector2 RectMax, real32 Value)
{
    entity *Entity = AddEntity(Lake, Name, Type);
    if (!Entity)
    {
        return 0;
    }

    Lake->Widgets.RectMin[Entity->Slot] = RectMin;
    Lake->Widgets.RectMax[Entity->Slot] = RectMax;
    Lake->Widgets.Value[Entity->Slot]   = Value;

    return Entity;
}

internal real32 EntityValue(data_lake *Lake, uint32 EntityID)
{
    entity *Entity = GetEntity(Lake, EntityID);
    Assert(Entity->Type == Entity_UIButton || Entity->Type == Entity_UISlider);

    return Lake->Widgets.Value[Entity->Slot];
}
