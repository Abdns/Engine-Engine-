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
    int32   MouseWheel;

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
    UI->MouseWheel    = Input->MouseZ;

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
    Assert(Width > 0.0f);

    if (UI->Active == ID)
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

internal int32 UIList(ui_context *UI, uint32 ID, rect2 Rect, const char **Names, uint32 ItemCount, uint32 VisibleCount, int32 Selected, real32 *Scroll)
{
    int32 Clicked = -1;

    if (PointInRect(UI->MousePosition, Rect) && UI->MouseWheel)
    {
        *Scroll -= (real32)UI->MouseWheel;
    }

    real32 MaxScroll = (ItemCount > VisibleCount) ? (real32)(ItemCount - VisibleCount) : 0.0f;
    *Scroll = Clamp(0.0f, *Scroll, MaxScroll);

    Vector4 Back = UI->Style.Base;
    Back.X *= 0.6f;
    Back.Y *= 0.6f;
    Back.Z *= 0.6f;
    PushRenderRect(UI->Commands, Rect.Min, Rect.Max, Back);

    bool32 Scrollable = (ItemCount > VisibleCount);
    real32 RowRight   = Scrollable ? Rect.Max.X - 6.0f : Rect.Max.X - 2.0f;
    real32 RowHeight  = (Rect.Max.Y - Rect.Min.Y) / (real32)VisibleCount;
    uint32 First      = (uint32)*Scroll;

    for (uint32 RowIndex = 0; RowIndex < VisibleCount; ++RowIndex)
    {
        uint32 ItemIndex = First + RowIndex;
        if (ItemIndex >= ItemCount)
        {
            break;
        }

        real32 RowTop = Rect.Min.Y + (real32)RowIndex * RowHeight;
        rect2  Row    = RectMinMax(Rect.Min.X + 2.0f, RowTop + 2.0f, RowRight, RowTop + RowHeight - 2.0f);

        uint32 RowID = (1u << 31) | (ID << 4) | RowIndex;
        if (UIWidgetInput(UI, RowID, Row))
        {
            Clicked = (int32)ItemIndex;
        }

        Vector4 Color = ((int32)ItemIndex == Selected) ? UI->Style.Active : UIWidgetColor(UI, RowID);
        PushRenderRect(UI->Commands, Row.Min, Row.Max, Color);
    }

    if (Scrollable)
    {
        real32 Height      = Rect.Max.Y - Rect.Min.Y;
        real32 ThumbHeight = Height * (real32)VisibleCount / (real32)ItemCount;
        real32 ThumbY      = Rect.Min.Y + Height * (*Scroll / (real32)ItemCount);

        PushRenderRect(UI->Commands, Vector2(Rect.Max.X - 4.0f, ThumbY), Vector2(Rect.Max.X - 2.0f, ThumbY + ThumbHeight), UI->Style.Hot);
    }

    if (!UI->Active && UI->MousePressed && PointInRect(UI->MousePosition, Rect))
    {
        UI->Active = ID;
    }

    return Clicked;
}

internal void UIPanel(ui_context *UI, rect2 Rect, Vector4 Color)
{
    PushRenderRect(UI->Commands, Rect.Min, Rect.Max, Color);
}

internal uint32 UIIDFromString(const char *Name)
{
    uint32 Hash = 2166136261u;

    for (const char *At = Name; *At; ++At)
    {
        Hash ^= (uint32)(uint8)*At;
        Hash *= 16777619u;
    }

    return Hash ? Hash : 1;
}

internal bool32 UILabeledButton(ui_context *UI, asset_store *Assets, uint32 FontHandle, const char *Label, rect2 Rect)
{
    bool32 Clicked = UIButton(UI, UIIDFromString(Label), Rect);

    Vector2 LabelP = Vector2(Rect.Min.X + 0.5f * ((Rect.Max.X - Rect.Min.X) - TextWidth(Assets, FontHandle, Label)),
                             Rect.Min.Y + 0.5f * ((Rect.Max.Y - Rect.Min.Y) - TextLineAdvance(Assets, FontHandle)));

    DrawText(UI->Commands, Assets, FontHandle, LabelP, Vector4(1.0f, 1.0f, 1.0f, 1.0f), Label);

    return Clicked;
}
