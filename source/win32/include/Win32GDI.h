#ifndef WIN32GDI_H
#define WIN32GDI_H

#include <windows.h>
#include <Wingdi.h>
#include "Types.h"
#include "PlatformAPI.h"  // game_offscreen_buffer

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void* Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct win32_window_dimensions
{
	int Width;
	int Height;
};

extern win32_offscreen_buffer GlobalBackBuffer;

win32_window_dimensions GetWindowDimension(HWND Window);
void Win32DisplayBufferInWindow(win32_offscreen_buffer* Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight);
void Win32ResizeDIB(win32_offscreen_buffer* Buffer, int Width, int Height);

// Перенос полей платформенного backbuffer'а в нейтральный game_offscreen_buffer для игры.
void Win32FillGameOffscreenBuffer(game_offscreen_buffer* Out, win32_offscreen_buffer* Src);

// Показать текущий backbuffer в окне (GetWindowDimension + StretchDIBits).
void Win32DisplayFrame(HWND Window, HDC DeviceContext);

#endif