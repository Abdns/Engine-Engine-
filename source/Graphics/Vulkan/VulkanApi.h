#ifndef VULKANAPI_H
#define VULKANAPI_H

#include "Types.h"

#include <vulkan/vulkan.h>   // VK_USE_PLATFORM_WIN32_KHR задаётся в build.bat

internal void InitVulkan(HINSTANCE hinstance, HWND hwnd);
internal void ShutdownVulkan();

#endif
