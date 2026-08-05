#ifndef UI_H
#define UI_H

#include "Types.h"
#include "EngineMath.h"
#include "PlatformAPI.h"
#include "RenderCommands.h"

struct rect2
{
    Vector2 Min;
    Vector2 Max;
};

struct ui_style
{
    Vector4 Base;
    Vector4 Hot;
    Vector4 Active;
};

struct ui_context
{
    Vector2 MousePosition;
    bool32  MouseDown;
    bool32  MousePressed;
    bool32  MouseReleased;

    uint32 Hot;
    uint32 Active;

    ui_style         Style;
    render_commands *Commands;
};

inline rect2 RectMinMax(real32 MinX, real32 MinY, real32 MaxX, real32 MaxY)
{
    rect2 Result;
    Result.Min = Vector2(MinX, MinY);
    Result.Max = Vector2(MaxX, MaxY);

    return Result;
}

inline rect2 RectMinDim(real32 MinX, real32 MinY, real32 Width, real32 Height)
{
    return RectMinMax(MinX, MinY, MinX + Width, MinY + Height);
}

inline bool32 PointInRect(Vector2 Point, rect2 Rect)
{
    return (Point.X >= Rect.Min.X && Point.X < Rect.Max.X &&
            Point.Y >= Rect.Min.Y && Point.Y < Rect.Max.Y);
}

internal ui_style DefaultUIStyle()
{
    ui_style Style;
    Style.Base   = Vector4(0.16f, 0.18f, 0.24f, 0.85f);
    Style.Hot    = Vector4(0.26f, 0.32f, 0.44f, 0.90f);
    Style.Active = Vector4(0.15f, 0.45f, 0.90f, 0.95f);

    return Style;
}

internal void BeginUI(ui_context *UI, game_input *Input, render_commands *Commands)
{
    game_button_state *Button = &Input->MouseButtons[0];

    UI->MousePosition = Vector2((real32)Input->MouseX, (real32)Input->MouseY);
    UI->MouseDown     = Button->EndedDown;
    UI->MousePressed  = (Button->EndedDown && Button->HalfTransitionCount);
    UI->MouseReleased = (!Button->EndedDown && Button->HalfTransitionCount);

    UI->Hot      = 0;
    UI->Commands = Commands;
}

internal void EndUI(ui_context *UI)
{
    if (!UI->MouseDown)
    {
        UI->Active = 0;
    }
}

internal Vector4 UIWidgetColor(ui_context *UI, uint32 ID)
{
    if (UI->Active == ID)
    {
        return UI->Style.Active;
    }

    if (UI->Hot == ID)
    {
        return UI->Style.Hot;
    }

    return UI->Style.Base;
}

internal bool32 UIWidgetInput(ui_context *UI, uint32 ID, rect2 Rect)
{
    bool32 Clicked = false;
    bool32 Inside  = PointInRect(UI->MousePosition, Rect);

    if (UI->Active == ID)
    {
        if (UI->MouseReleased)
        {
            Clicked    = Inside;
            UI->Active = 0;
        }
    }
    else if (!UI->Active && Inside)
    {
        UI->Hot = ID;

        if (UI->MousePressed)
        {
            UI->Active = ID;
        }
    }

    return Clicked;
}

internal bool32 UIButton(ui_context *UI, uint32 ID, rect2 Rect)
{
    bool32 Clicked = UIWidgetInput(UI, ID, Rect);

    PushRenderRect(UI->Commands, Rect.Min, Rect.Max, UIWidgetColor(UI, ID));

    return Clicked;
}

internal bool32 UISlider(ui_context *UI, uint32 ID, rect2 Rect, real32 *Value)
{
    bool32 Changed = false;

    UIWidgetInput(UI, ID, Rect);

    real32 Width = Rect.Max.X - Rect.Min.X;
    if (UI->Active == ID && Width > 0.0f)
    {
        real32 T = Clamp(0.0f, (UI->MousePosition.X - Rect.Min.X) / Width, 1.0f);
        if (T != *Value)
        {
            *Value  = T;
            Changed = true;
        }
    }

    PushRenderRect(UI->Commands, Rect.Min, Rect.Max, UI->Style.Base);

    rect2 Fill = Rect;
    Fill.Max.X = Rect.Min.X + Width * Clamp(0.0f, *Value, 1.0f);
    PushRenderRect(UI->Commands, Fill.Min, Fill.Max, UIWidgetColor(UI, ID));

    return Changed;
}

internal void UIPanel(ui_context *UI, rect2 Rect, Vector4 Color)
{
    PushRenderRect(UI->Commands, Rect.Min, Rect.Max, Color);
}

#endif
