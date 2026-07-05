#ifndef WIN32MEMORY_H
#define WIN32MEMORY_H

#include <windows.h>
#include "Types.h"
#include "PlatformAPI.h"   // game_memory
struct win32_state
{
	uint64 TotalSize;
	void* GameMemoryBlock;

	HANDLE RecordingHandle;
	int    InputRecordingIndex;

	HANDLE PlaybackHandle;
	int    InputPlayingIndex;

	char EXEFileName[MAX_PATH];
	char* OnePastLastSlash;
};

void Win32AllocateGameMemory(game_memory* GameMemory, win32_state* State);

#endif
