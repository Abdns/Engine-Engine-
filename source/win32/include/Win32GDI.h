#ifndef WIN32GDI_H
#define WIN32GDI_H

#include <windows.h>
#include <Wingdi.h>
#include "Types.h"

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


#endif