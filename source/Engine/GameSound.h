#ifndef GAMESOUND_H
#define GAMESOUND_H

#include "Types.h"

struct game_sound_output_buffer
{
    int    SamplesPerSecond;
    int    SampleCount;
    int16* Samples;
};

#endif
