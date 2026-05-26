#ifndef WIN32REPLAY_H
#define WIN32REPLAY_H

#include <windows.h>
#include "Types.h"
#include "EngineLayer.h"

struct win32_state
{
    uint64 TotalSize;
    void*  GameMemoryBlock;

    HANDLE RecordingHandle;
    int    InputRecordingIndex;

    HANDLE PlaybackHandle;
    int    InputPlayingIndex;

    char EXEFileName[MAX_PATH];
    char* OnePastLastSlash;
};

void Win32GetInputFileLocation(win32_state* State, bool32 InputStream, int SlotIndex,
                               int DestCount, char* Dest);

void Win32BeginRecordingInput (win32_state* State, int InputRecordingIndex);
void Win32EndRecordingInput   (win32_state* State);
void Win32BeginInputPlayBack  (win32_state* State, int InputPlayingIndex);
void Win32EndInputPlayBack    (win32_state* State);

void Win32RecordInput   (win32_state* State, game_input* NewInput);
void Win32PlayBackInput (win32_state* State, game_input* NewInput);

#endif
