#ifndef GAMERENDER_H
#define GAMERENDER_H

#include "Types.h"

struct game_offscreen_buffer
{
    void* Memory;
    int   Width;
    int   Height;
    int   Pitch;
    int   BytesPerPixel;
};

#endif
