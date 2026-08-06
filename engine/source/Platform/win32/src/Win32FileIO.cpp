#if ENGINE_INTERNAL

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 Result = false;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0,
                                    CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            Result = (BytesWritten == MemorySize);
        }
        CloseHandle(FileHandle);
    }

    return Result;
}

#endif

void Win32SetupPlatformAPI(game_memory* GameMemory)
{
    GameMemory->PlatformReadEntireFile = Win32ReadEntireFile;
    GameMemory->PlatformFreeFileMemory = Win32FreeFileMemory;
#if ENGINE_INTERNAL
    GameMemory->DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
#endif
}
