#ifndef ENTITY_H
#define ENTITY_H

#include "Types.h"
#include "EngineMath.h"
#include "RenderCommands.h"

struct entities
{
    uint32 Count;
    uint32 MaxCount;
    uint32 NextID;

    uint32  *ID;
    Vector3 *Position;
    Vector3 *Velocity;
    Vector3 *Rotation;
    Vector3 *AngularVelocity;
    Vector4 *Tint;

    mesh_handle    *Mesh;
    texture_handle *Texture;
};

#endif
