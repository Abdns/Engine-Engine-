#ifndef VULKANAPI_H
#define VULKANAPI_H

#include "Types.h"
#include "RenderCommands.h"   // render_commands — их исполняет Vulkan-бэкенд

#include <vulkan/vulkan.h>   // VK_USE_PLATFORM_WIN32_KHR задаётся в build.bat

internal void InitVulkan(HINSTANCE hinstance, HWND hwnd);
internal void RenderVulkanFrame(render_commands *Commands);
internal void ShutdownVulkan();

#endif
