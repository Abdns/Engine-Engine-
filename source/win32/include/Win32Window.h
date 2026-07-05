#ifndef WIN32WINDOW_H
#define WIN32WINDOW_H

#include "Types.h"
#include "PlatformAPI.h"   // game_controller_input
#include "Win32Replay.h"
#include <windows.h>

extern bool32 isRunning;

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);
HWND Win32CreateMainWindow(HINSTANCE Instance, LPCWSTR ClassName, LPCWSTR Title, int Width, int Height);
void Win32ProcessPendingMessages(win32_state* State, game_controller_input* KeyboardController);

// Day 025: fullscreen toggle (Raymond Chen technique).
void Win32ToggleFullscreen(HWND Window);

#endif
