#ifndef PLATFORMAPI_H
#define PLATFORMAPI_H

#include "Types.h"
#include "Strings.h"
#include "RenderCommands.h"

struct platform_file_raw
{
    uint32 Size;
    void  *Data;
};

#define PLATFORM_READ_ENTIRE_FILE(name)  platform_file_raw name(const char *Filename)
typedef PLATFORM_READ_ENTIRE_FILE(platform_read_entire_file);

#define PLATFORM_FREE_FILE_MEMORY(name)  void name(void *Memory)
typedef PLATFORM_FREE_FILE_MEMORY(platform_free_file_memory);

#if ENGINE_INTERNAL
#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char* Filename, uint32 MemorySize, void* Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif

struct gpu_limits
{
    uint32 MaxVertices;
    uint32 MaxIndices;
    uint32 MaxTextures;
};

#define PLATFORM_GET_GPU_LIMITS(name) gpu_limits name(void)
typedef PLATFORM_GET_GPU_LIMITS(platform_get_gpu_limits);

#define PLATFORM_WRITE_VERTICES(name) bool32 name(uint32 FirstVertex, real32 *Data, uint32 VertexCount)
typedef PLATFORM_WRITE_VERTICES(platform_write_vertices);

#define PLATFORM_WRITE_INDICES(name)  bool32 name(uint32 FirstIndex, uint32 *Data, uint32 IndexCount)
typedef PLATFORM_WRITE_INDICES(platform_write_indices);

#define PLATFORM_WRITE_TEXTURE(name)  bool32 name(uint32 Slot, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB)
typedef PLATFORM_WRITE_TEXTURE(platform_write_texture);

struct game_memory
{
    bool32 IsInitialized;

    uint64 PermanentStorageSize;
    void*  PermanentStorage;

    platform_read_entire_file* PlatformReadEntireFile;
    platform_free_file_memory* PlatformFreeFileMemory;

    platform_get_gpu_limits* PlatformGetGpuLimits;
    platform_write_vertices* PlatformWriteVertices;
    platform_write_indices*  PlatformWriteIndices;
    platform_write_texture*  PlatformWriteTexture;

#if ENGINE_INTERNAL
    debug_platform_write_entire_file* DEBUGPlatformWriteEntireFile;
#endif
};

struct game_button_state
{
    int    HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsConnected;
    bool32 IsAnalog;

    real32 StickAverageX;
    real32 StickAverageY;

    union
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_input
{
    real32 dtForFrame;

    game_button_state MouseButtons[5];
    int32 MouseX;
    int32 MouseY;
    int32 MouseZ;

    game_controller_input Controllers[5];
};

struct game_sound_output_buffer
{
    int    SamplesPerSecond;
    int    SampleCount;
    int16* Samples;
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory* Memory, game_input* Input, render_commands* RenderCommands)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory* Memory, game_sound_output_buffer* SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#endif
