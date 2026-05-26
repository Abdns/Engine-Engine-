#include "Win32Input.h"

X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;

X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;

void Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibrary(L"xinput1_4.dll");
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibrary(L"xinput9_1_0.dll");
    }
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibrary(L"xinput1_3.dll");
    }
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                              game_button_state* OldState, DWORD ButtonBit,
                                              game_button_state* NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
    real32 Result = 0.0f;
    if (Value < -DeadZoneThreshold)
    {
        Result = (real32)(Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);
    }
    else if (Value > DeadZoneThreshold)
    {
        Result = (real32)(Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold);
    }
    return Result;
}

void Win32ProcessXInput(game_input* NewInput, game_input* OldInput)
{
    DWORD MaxControllerCount = XUSER_MAX_COUNT;
    if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
    {
        MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
    }

    for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex)
    {
        DWORD OurControllerIndex = ControllerIndex + 1;
        game_controller_input* OldController = &OldInput->Controllers[OurControllerIndex];
        game_controller_input* NewController = &NewInput->Controllers[OurControllerIndex];

        XINPUT_STATE ControllerState;
        if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
        {
            XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

            NewController->IsConnected = true;
            NewController->IsAnalog = true;

            NewController->StickAverageX =
                Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            NewController->StickAverageY =
                Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Up,
                                            XINPUT_GAMEPAD_DPAD_UP, &NewController->Up);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Down,
                                            XINPUT_GAMEPAD_DPAD_DOWN, &NewController->Down);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Left,
                                            XINPUT_GAMEPAD_DPAD_LEFT, &NewController->Left);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Right,
                                            XINPUT_GAMEPAD_DPAD_RIGHT, &NewController->Right);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder,
                                            XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder,
                                            XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
        }
        else
        {
            NewController->IsConnected = false;
        }
    }
}
