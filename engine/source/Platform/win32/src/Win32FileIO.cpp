internal void Win32FreeFileMemory(void* Memory)
{
    if (Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

internal platform_file Win32ReadEntireFile(const char* Filename)
{
    platform_file Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize))
        {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Data = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (Result.Data)
            {
                DWORD BytesRead;
                if (ReadFile(FileHandle, Result.Data, FileSize32, &BytesRead, 0) &&
                    (FileSize32 == BytesRead))
                {
                    Result.Size = FileSize32;
                }
                else
                {
                    Win32FreeFileMemory(Result.Data);
                    Result.Data = 0;
                }
            }
        }
        CloseHandle(FileHandle);
    }

    return Result;
}

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
