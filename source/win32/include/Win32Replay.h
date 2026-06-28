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

// Аллоцировать единый блок памяти для игры через VirtualAlloc + назначить
// platform-указатели и сохранить блок в win32_state для snapshot'ов.
void Win32AllocateGameMemory(game_memory* GameMemory, win32_state* State);

// Один вызов в цикле: если записываем — пишем ввод, если воспроизводим — читаем.
void Win32UpdateRecordAndPlayback(win32_state* State, game_input* NewInput);

#endif
