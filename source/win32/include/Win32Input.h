#ifndef WIN32INPUT_H
#define WIN32INPUT_H

#include <windows.h>
#include <Xinput.h>
#include "Types.h"
#include "EngineLayer.h"

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

#endif
