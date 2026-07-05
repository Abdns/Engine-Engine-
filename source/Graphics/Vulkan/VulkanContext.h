#ifndef VULKANCONTEXT_H
#define VULKANCONTEXT_H

#include "VulkanApi.h"   // <vulkan/vulkan.h> (+Win32) и Types.h (uint32, bool32, internal)

// Общие ДАННЫЕ подсистемы Vulkan: лимиты буферов + структуры состояния. ТОЛЬКО
// данные — никаких объявлений функций. Это легальный «заголовок-данные» (как
// VulkanApi.h — интерфейс), а НЕ свалка forward-decl static-функций, которую мы
// в своё время убрали. Нужен, потому что vulkan_context делят два .cpp:
// VulkanApi.cpp (создаёт объекты) и VulkanRender.cpp (рисует ими каждый кадр).

// --- Лимиты буферов ---
#define MAX_EXTENSIONS        256
#define MAX_DEVICES           4
#define MAX_FAMILY_COUNT      8
#define MAX_DEVICE_EXTENSIONS 256
#define MAX_SURFACE_FORMATS   64
#define MAX_PRESENT_MODES     8
#define MAX_SWAPCHAIN_IMAGES  8

// --- Состояние Vulkan: один контекст на весь движок ---
struct vulkan_context
{
    VkInstance instance;
    VkSurfaceKHR surface;
    HWND windowHandle;            // нужен для пересоздания swapchain при ресайзе
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32 graphicsFamilyIndex;
    uint32 presentFamilyIndex;

    VkSwapchainKHR swapchain;
    VkImage swapchainImages[MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchainImageViews[MAX_SWAPCHAIN_IMAGES];
    uint32 swapchainImageCount;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;

    VkRenderPass renderPass;

    // --- конвейер и отрисовка (уроки 2.9–2.15) ---
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkFramebuffer swapchainFramebuffers[MAX_SWAPCHAIN_IMAGES];

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;                        // acquire -> submit
    VkSemaphore renderFinishedSemaphores[MAX_SWAPCHAIN_IMAGES]; // submit -> present, по одному на картинку
    VkFence inFlightFence;                                      // CPU ждёт GPU
};

struct queue_family_indices
{
    uint32 graphicsIndex;
    uint32 presentIndex;
    bool32 graphicsSupported;
    bool32 presentSupported;
};

struct swapchain_support_details
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR formats[MAX_SURFACE_FORMATS];
    uint32 formatCount;
    VkPresentModeKHR presentModes[MAX_PRESENT_MODES];
    uint32 presentModeCount;
};

// Пуш-константы для рисования прямоугольника. Раскладка ДОЛЖНА совпадать с
// блоком push_constant в rect.vert (std430): vec2 Min; vec2 Max; vec4 Color; vec2 Screen.
struct rect_push_constants
{
    real32 MinX, MinY;       // vec2 Min   (пиксели)
    real32 MaxX, MaxY;       // vec2 Max   (пиксели)
    real32 R, G, B, A;       // vec4 Color
    real32 ScreenW, ScreenH; // vec2 Screen (размер кадра в пикселях)
};

#endif
