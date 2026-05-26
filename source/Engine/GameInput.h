#ifndef GAMEINPUT_H
#define GAMEINPUT_H

#include "Types.h"

struct game_button_state
{
    int    HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsConnected;
    bool32 IsAnalog;

    real32 StickAverageX;
    real32 StickAverageY;

    union
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_input
{
    // Сколько секунд длился предыдущий кадр (фиксированный TargetSecondsPerFrame).
    real32 dtForFrame;

    // Мышь: координаты в пикселях окна, колесо, 5 кнопок.
    game_button_state MouseButtons[5];
    int32 MouseX;
    int32 MouseY;
    int32 MouseZ;

    game_controller_input Controllers[5];
};

#endif
