#include "SoftwareRender.h"

// Заполненный прямоугольник в пиксельный буфер: float-координаты, цвет 0.0..1.0.
internal void DrawRectangle(game_offscreen_buffer *Buffer, real32 MinX, real32 MinY, real32 MaxX, real32 MaxY, real32 R, real32 G, real32 B)
{
    int32 MinXi = RoundReal32ToInt32(MinX);
    int32 MinYi = RoundReal32ToInt32(MinY);
    int32 MaxXi = RoundReal32ToInt32(MaxX);
    int32 MaxYi = RoundReal32ToInt32(MaxY);

    if (MinXi < 0)              MinXi = 0;
    if (MinYi < 0)              MinYi = 0;
    if (MaxXi > Buffer->Width)  MaxXi = Buffer->Width;
    if (MaxYi > Buffer->Height) MaxYi = Buffer->Height;

    uint32 Color = ((RoundReal32ToUInt32(R * 255.0f) << 16) | (RoundReal32ToUInt32(G * 255.0f) << 8) | RoundReal32ToUInt32(B * 255.0f));

    uint8* Row = (uint8*)Buffer->Memory + MinXi * Buffer->BytesPerPixel + MinYi * Buffer->Pitch;
    for (int Y = MinYi; Y < MaxYi; ++Y)
    {
        uint32* Pixel = (uint32*)Row;
        for (int X = MinXi; X < MaxXi; ++X)
        {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
    }
}

// Исполнить буфер команд в CPU-буфер: пройти команды по порядку и нарисовать.
internal void SoftwareRenderCommands(render_commands *Commands, game_offscreen_buffer *Buffer)
{
    for (uint64 Offset = 0; Offset < Commands->BufferUsed; )
    {
        render_command_header *Header = (render_command_header *)(Commands->BufferBase + Offset);
        switch (Header->Type)
        {
            case RenderCommand_Clear:
            {
                render_command_clear *C = (render_command_clear *)Header;
                DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, C->R, C->G, C->B);
                Offset += sizeof(render_command_clear);
            } break;

            case RenderCommand_Rect:
            {
                render_command_rect *Rect = (render_command_rect *)Header;
                DrawRectangle(Buffer, Rect->MinX, Rect->MinY, Rect->MaxX, Rect->MaxY, Rect->R, Rect->G, Rect->B);
                Offset += sizeof(render_command_rect);
            } break;

            default:
            {
                // неизвестная команда — прерываем, чтобы не уйти в мусор
                Offset = Commands->BufferUsed;
            } break;
        }
    }
}
