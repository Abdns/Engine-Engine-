#ifndef WIN32FILEIO_H
#define WIN32FILEIO_H

#include <windows.h>
#include "Types.h"
#include "PlatformAPI.h"   // DEBUG_PLATFORM_* + debug_read_file_result

#if HANDMADE_INTERNAL

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile);
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory);
DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile);
#endif

#endif
