#include "VulkanShaders.h"  // vulkan_shader + интерфейс модуля
#include "Strings.h"        // AppendString
#include "Debug.h"          // DebugLog (вместо stdio.h)

internal vulkan_shader LoadShader(const char *name)
{
    vulkan_shader shader = {};

    char vertPath[256];
    char fragPath[256];

    uint32 n = AppendString(vertPath, ArrayCount(vertPath), 0, "CompiledShaders/");
    n = AppendString(vertPath, ArrayCount(vertPath), n, name);
    AppendString(vertPath, ArrayCount(vertPath), n, ".vert.spv");

    n = AppendString(fragPath, ArrayCount(fragPath), 0, "CompiledShaders/");
    n = AppendString(fragPath, ArrayCount(fragPath), n, name);
    AppendString(fragPath, ArrayCount(fragPath), n, ".frag.spv");

    shader.vert = DEBUGPlatformReadEntireFile(vertPath);
    shader.frag = DEBUGPlatformReadEntireFile(fragPath);
    shader.valid = (shader.vert.Contents != 0) && (shader.frag.Contents != 0);

    if (shader.valid)
    {
        DebugLog("Shader '%s' loaded (vert %u, frag %u bytes)\n",
               name, shader.vert.ContentsSize, shader.frag.ContentsSize);
    }
    else
    {
        DebugLog("Shader '%s' not found in CompiledShaders\n", name);
    }

    return shader;
}

internal void FreeShader(vulkan_shader *shader)
{
    DEBUGPlatformFreeFileMemory(shader->vert.Contents);
    DEBUGPlatformFreeFileMemory(shader->frag.Contents);
    *shader = {};
}
