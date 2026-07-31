#ifndef CAMERA_H
#define CAMERA_H

#include "Types.h"
#include "EngineMath.h"

struct camera
{
    Vector3 Position;
    real32  Yaw;
    real32  Pitch;
    real32  FovY;
};

struct camera_basis
{
    Vector3 Right;
    Vector3 Up;
    Vector3 Forward;
};

#endif
