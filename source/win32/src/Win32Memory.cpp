#include "Win32Memory.h"
#include "Win32FileIO.h"

void Win32AllocateGameMemory(game_memory* GameMemory, win32_state* State)
{
#if HANDMADE_INTERNAL
    LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
    LPVOID BaseAddress = 0;
#endif

    GameMemory->PermanentStorageSize = Megabytes(64);
    GameMemory->TransientStorageSize = Gigabytes(1);

    State->TotalSize = GameMemory->PermanentStorageSize + GameMemory->TransientStorageSize;
    State->GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)State->TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    GameMemory->PermanentStorage = State->GameMemoryBlock;
    GameMemory->TransientStorage = (uint8*)GameMemory->PermanentStorage + GameMemory->PermanentStorageSize;

#if HANDMADE_INTERNAL
    GameMemory->DEBUGPlatformReadEntireFile  = DEBUGPlatformReadEntireFile;
    GameMemory->DEBUGPlatformFreeFileMemory  = DEBUGPlatformFreeFileMemory;
    GameMemory->DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
#endif
}
