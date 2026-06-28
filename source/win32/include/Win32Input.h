#ifndef WIN32INPUT_H
#define WIN32INPUT_H

#include <windows.h>
#include <Xinput.h>
#include "Types.h"
#include "EngineLayer.h"
#include "Win32Replay.h"  // нужен win32_state для Win32ProcessInput

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
extern x_input_get_state* XInputGetState_;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
extern x_input_set_state* XInputSetState_;
#define XInputSetState XInputSetState_

void Win32LoadXInput(void);
void Win32ProcessXInput(game_input* NewInput, game_input* OldInput);

// Снять EndedDown с прошлого кадра, занулить HalfTransitionCount.
void Win32PrepareKeyboardInput(game_input* NewInput, game_input* OldInput);

// Мышь: координаты в окне + 5 кнопок.
void Win32ProcessMouseInput(HWND Window, game_input* NewInput, game_input* OldInput);

// Один вызов, обрабатывающий весь ввод за кадр (keyboard + Win32-сообщения + мышь + XInput).
void Win32ProcessInput(game_input* NewInput, game_input* OldInput,
                       win32_state* State, HWND Window);

#endif
