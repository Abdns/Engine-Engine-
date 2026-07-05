#ifndef WIN32GAMECODE_H
#define WIN32GAMECODE_H

#include <windows.h>
#include "Types.h"
#include "PlatformAPI.h"   // game_update_and_render / game_get_sound_samples

struct win32_game_code
{
    HMODULE  GameCodeDLL;
    FILETIME DLLLastWriteTime;
    game_update_and_render* UpdateAndRender;
    game_get_sound_samples* GetSoundSamples;
    bool32 IsValid;
};

struct win32_exe_paths
{
    char EXEFileName[MAX_PATH];
    char* OnePastLastSlash;
    char SourceGameCodeDLLFullPath[MAX_PATH];
    char TempGameCodeDLLFullPath[MAX_PATH];
    char GameCodeLockFullPath[MAX_PATH];
};

void Win32GetEXEPaths(win32_exe_paths* Paths);

FILETIME Win32GetLastWriteTime(char* Filename);
win32_game_code Win32LoadGameCode(char* SourceDLLName, char* TempDLLName);
void Win32UnloadGameCode(win32_game_code* GameCode);
void Win32ReloadGameCodeIfChanged(win32_game_code* Game, char* SourceDLLPath, char* TempDLLPath, char* LockPath);

#endif
