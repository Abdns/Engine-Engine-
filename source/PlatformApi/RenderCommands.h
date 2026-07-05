#ifndef RENDERCOMMANDS_H
#define RENDERCOMMANDS_H

#include "Types.h"


enum render_command_type
{
    RenderCommand_Clear,
    RenderCommand_Rect,
};

struct render_command_header
{
    render_command_type Type;
};

struct render_command_clear
{
    render_command_header Header;
    real32 R, G, B;
};

// Прямоугольник в ПИКСЕЛЯХ (origin — верхний левый угол, y вниз).
struct render_command_rect
{
    render_command_header Header;
    real32 MinX, MinY, MaxX, MaxY;
    real32 R, G, B;
};

struct render_commands
{
    uint8 *BufferBase;
    uint32 BufferSize;
    uint32 BufferUsed;

    uint32 Width;
    uint32 Height;
};

inline render_commands InitRenderCommands(void *Memory, uint32 Size, uint32 Width, uint32 Height)
{
    render_commands Result;
    Result.BufferBase = (uint8 *)Memory;
    Result.BufferSize = Size;
    Result.BufferUsed = 0;
    Result.Width  = Width;
    Result.Height = Height;
    return Result;
}

// Зарезервировать место под одну команду (bump внутри буфера).
inline void *PushRenderCommand_(render_commands *Commands, uint32 Size)
{
    void *Result = 0;
    if (Commands->BufferUsed + Size <= Commands->BufferSize)
    {
        Result = Commands->BufferBase + Commands->BufferUsed;
        Commands->BufferUsed += Size;
    }
    return Result;
}

inline void PushRenderClear(render_commands *Commands, real32 R, real32 G, real32 B)
{
    render_command_clear *Cmd = (render_command_clear *)PushRenderCommand_(Commands, (uint32)sizeof(render_command_clear));
    if (Cmd)
    {
        Cmd->Header.Type = RenderCommand_Clear;
        Cmd->R = R; Cmd->G = G; Cmd->B = B;
    }
}

inline void PushRenderRect(render_commands *Commands, real32 MinX, real32 MinY, real32 MaxX, real32 MaxY, real32 R, real32 G, real32 B)
{
    render_command_rect *Cmd = (render_command_rect *)PushRenderCommand_(Commands, (uint32)sizeof(render_command_rect));
    if (Cmd)
    {
        Cmd->Header.Type = RenderCommand_Rect;
        Cmd->MinX = MinX; Cmd->MinY = MinY; Cmd->MaxX = MaxX; Cmd->MaxY = MaxY;
        Cmd->R = R; Cmd->G = G; Cmd->B = B;
    }
}

#endif
