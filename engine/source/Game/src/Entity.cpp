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
    uint32      MeshHandle;
    uint32      MaterialHandle;
    bool32      Static;
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
    Lake->Transforms.Velocity        = PushArray(Arena, MAX_ENTITIES, Vector3);
    Lake->Transforms.Rotation        = PushArray(Arena, MAX_ENTITIES, Matrix4);
    Lake->Transforms.AngularVelocity = PushArray(Arena, MAX_ENTITIES, Vector3);
    Lake->Transforms.Tint            = PushArray(Arena, MAX_ENTITIES, Vector4);

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

internal Matrix4 EntityLocalToWorld(data_lake *Lake, entity *Entity)
{
    Assert(Entity->Type == Entity_Mesh);

    Vector3 Position = Lake->Transforms.Position[Entity->Slot];

    return Mat4Multiply(Mat4Translation(Position.X, Position.Y, Position.Z), Lake->Transforms.Rotation[Entity->Slot]);
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
    Entity->ID             = ++Lake->EntityNextID;
    Entity->Type           = Type;
    Entity->Slot           = (Type == Entity_Mesh) ? Lake->Transforms.Count++ : Lake->Widgets.Count++;
    Entity->MeshHandle     = 0;
    Entity->MaterialHandle = 0;
    Entity->Static         = false;

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

    Lake->Transforms.Position[Entity->Slot]        = Position;
    Lake->Transforms.Velocity[Entity->Slot]        = Vector3(0.0f, 0.0f, 0.0f);
    Lake->Transforms.Rotation[Entity->Slot]        = Mat4Identity();
    Lake->Transforms.AngularVelocity[Entity->Slot] = Vector3(0.0f, 0.0f, 0.0f);
    Lake->Transforms.Tint[Entity->Slot]            = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    Entity->MeshHandle     = MeshHandle;
    Entity->MaterialHandle = MaterialHandle;

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
