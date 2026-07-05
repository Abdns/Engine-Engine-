#ifndef GAMEMEMORY_H
#define GAMEMEMORY_H

#include "Types.h"

#if HANDMADE_INTERNAL
struct debug_read_file_result
{
    uint32 ContentsSize;
    void*  Contents;
};

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name)  debug_read_file_result name(char* Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name)  void name(void* Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char* Filename, uint32 MemorySize, void* Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif

struct game_memory
{
    bool32 IsInitialized;
    // Day 024: платформа поднимает этот флаг на одном кадре сразу после
    // hot-reload DLL. Игра может пересоздать указатели/кеши, если хранила их.
    bool32 ExecutableReloaded;

    uint64 PermanentStorageSize;
    void*  PermanentStorage;

    uint64 TransientStorageSize;
    void*  TransientStorage;

#if HANDMADE_INTERNAL
    debug_platform_read_entire_file*  DEBUGPlatformReadEntireFile;
    debug_platform_free_file_memory*  DEBUGPlatformFreeFileMemory;
    debug_platform_write_entire_file* DEBUGPlatformWriteEntireFile;
#endif
};

#endif
