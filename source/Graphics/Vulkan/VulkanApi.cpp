#include "VulkanApi.h"
#include "Strings.h"
#include "VulkanShaders.h"
#include "Debug.h"

#define MAX_EXTENSIONS 256
#define MAX_DEVICES 4
#define MAX_FAMILY_COUNT 8
#define MAX_DEVICE_EXTENSIONS 256
#define MAX_SURFACE_FORMATS 64
#define MAX_PRESENT_MODES 8
#define MAX_SWAPCHAIN_IMAGES 8

// Что движку нужно от Vulkan: правишь ЗДЕСЬ, код сам проверяет/включает по списку
global_variable const char *RequiredInstanceExtensions[] =
{
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
};

global_variable const char *RequiredDeviceExtensions[] =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

// Предпочтения (политика выбора). Структурные константы API тут НЕ держим.
#define REQUIRED_DEVICE_TYPE       VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
#define PREFERRED_SURFACE_FORMAT   VK_FORMAT_B8G8R8A8_UNORM           // линейный, без авто-гаммы
#define FALLBACK_SURFACE_FORMAT    VK_FORMAT_R8G8B8A8_UNORM
#define REQUIRED_COLOR_SPACE       VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define PREFERRED_PRESENT_MODE     VK_PRESENT_MODE_MAILBOX_KHR        // тройная буферизация
#define FALLBACK_PRESENT_MODE      VK_PRESENT_MODE_FIFO_KHR           // v-sync, гарантирован

struct vulkan_context
{
    VkInstance instance;
    VkSurfaceKHR surface;
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

global_variable vulkan_context GlobalVulkan;

internal uint32 GetExtensions(VkExtensionProperties *props, uint32 maxCount)
{
    uint32 extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    if (extensionCount > maxCount)
    {
        extensionCount = maxCount;
    }

    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, props);

    return extensionCount;
}

internal bool32 CheckInstanceExtensionSupport(const char **required, uint32 requiredCount)
{
    VkExtensionProperties available[MAX_EXTENSIONS];
    uint32 availableCount = GetExtensions(available, ArrayCount(available));

    for (uint32 i = 0; i < requiredCount; ++i)
    {
        bool32 found = false;
        for (uint32 j = 0; j < availableCount; ++j)
        {
            if (StringsAreEqual(required[i], available[j].extensionName))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            DebugLog("Instance extension %s not supported\n", required[i]);
            return false;
        }
    }

    return true;
}

internal VkApplicationInfo VkGetInfo()
{
    VkApplicationInfo appInfo = {};
    
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName =  "Vulkan App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    return appInfo;
}

internal VkInstanceCreateInfo GetInstanceInfo(VkApplicationInfo *appInfo, const char **extensions, uint32 extensionCount)
{
    VkInstanceCreateInfo createInfo{};

    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;

    return createInfo;
}

internal uint32 GetDiveses(const VkInstance *instance, VkPhysicalDevice *devices, uint32 maxCount)
{
    uint32_t devicesCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(*instance, &devicesCount, nullptr);  
    if (result != VK_SUCCESS)
    {
	return 0;
    }
    
    if (devicesCount > maxCount)
    {
        devicesCount = maxCount;
    }

    if (!devicesCount)
    {
        DebugLog("No vulkan devices support\n");
    }
    else
    {
	    DebugLog("vulkan devices support %d\n", devicesCount );
    }

    vkEnumeratePhysicalDevices(*instance, &devicesCount, devices);
    if (result != VK_SUCCESS)
    {
	return 0;
    }

    return devicesCount;
}

internal void GetDivesesPropsAndFeatures(const VkPhysicalDevice *devise, VkPhysicalDeviceProperties *deviceProperties, VkPhysicalDeviceFeatures *features)
{
    vkGetPhysicalDeviceProperties(*devise, deviceProperties);
    vkGetPhysicalDeviceFeatures(*devise, features);
}

internal queue_family_indices SelectQueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    queue_family_indices result = {};

    uint32 familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    if (!familyCount)
    {
        return result;
    }

    if (familyCount > MAX_FAMILY_COUNT)
    {
        familyCount = MAX_FAMILY_COUNT;
    }

    VkQueueFamilyProperties families[MAX_FAMILY_COUNT];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families);

    for (uint32 i = 0; i < familyCount; ++i)
    {
        // Графика: умеет ли семейство команды рисования
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            result.graphicsSupported = true;
            result.graphicsIndex = i;
        }

        // Показ: умеет ли семейство выводить в нашу поверхность
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport)
        {
            result.presentSupported = true;
            result.presentIndex = i;
        }

        if (result.graphicsSupported && result.presentSupported)
        {
            break;
        }
    }

    return result;
}

internal bool32 CheckDeviceExtensionSupport(VkPhysicalDevice device, const char **required, uint32 requiredCount)
{
    uint32 availableCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &availableCount, nullptr);

    if (availableCount > MAX_DEVICE_EXTENSIONS)
    {
        availableCount = MAX_DEVICE_EXTENSIONS;
    }

    VkExtensionProperties available[MAX_DEVICE_EXTENSIONS];
    vkEnumerateDeviceExtensionProperties(device, nullptr, &availableCount, available);

    // каждое нужное должно найтись среди доступных (как CheckInstanceExtensionSupport)
    for (uint32 i = 0; i < requiredCount; ++i)
    {
        bool32 found = false;
        for (uint32 j = 0; j < availableCount; ++j)
        {
            if (StringsAreEqual(required[i], available[j].extensionName))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            DebugLog("Device extension %s not supported\n", required[i]);
            return false;
        }
    }

    return true;
}

internal swapchain_support_details QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    swapchain_support_details details = {};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount > MAX_SURFACE_FORMATS)
    {
        formatCount = MAX_SURFACE_FORMATS;
    }
    details.formatCount = formatCount;
    if (formatCount)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats);
    }

    uint32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount > MAX_PRESENT_MODES)
    {
        presentModeCount = MAX_PRESENT_MODES;
    }
    details.presentModeCount = presentModeCount;
    if (presentModeCount)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes);
    }

    return details;
}

internal bool32 SelectDevice(vulkan_context *context)
{
    VkPhysicalDevice devices[MAX_DEVICES];
    uint32 devicesCount = GetDiveses(&context->instance, devices, ArrayCount(devices));

    for (uint32 i = 0; i < devicesCount; i++)
    {
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures features;

        GetDivesesPropsAndFeatures(&devices[i], &deviceProperties, &features);
        queue_family_indices indices = SelectQueueFamilyIndices(devices[i], context->surface);

        if (!CheckDeviceExtensionSupport(devices[i], RequiredDeviceExtensions, ArrayCount(RequiredDeviceExtensions)))
        {
            continue;   // функция уже залогировала, чего не хватает
        }

        swapchain_support_details swapchain = QuerySwapchainSupport(devices[i], context->surface);
        bool32 swapchainOk = (swapchain.formatCount > 0) && (swapchain.presentModeCount > 0);

        if (deviceProperties.deviceType == REQUIRED_DEVICE_TYPE
            && indices.graphicsSupported && indices.presentSupported
            && swapchainOk)
	{
            context->physicalDevice = devices[i];
            context->graphicsFamilyIndex = indices.graphicsIndex;
            context->presentFamilyIndex = indices.presentIndex;
            return true;
	}
    }

    return false;
}

internal VkQueue CreateQueue(VkDevice device, uint32 queueFamilyIndex)
{
    if (device == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }

    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
    return queue;
}

internal bool32 CreateLogicalDevice(vulkan_context *context)
{
    float queuePriority = 1.0f;

    // graphics и present могут оказаться одним семейством — берём уникальные
    uint32 uniqueFamilies[2];
    uint32 uniqueCount = 0;
    uniqueFamilies[uniqueCount++] = context->graphicsFamilyIndex;
    if (context->presentFamilyIndex != context->graphicsFamilyIndex)
    {
        uniqueFamilies[uniqueCount++] = context->presentFamilyIndex;
    }

    VkDeviceQueueCreateInfo queueInfos[2] = {};
    for (uint32 i = 0; i < uniqueCount; ++i)
    {
        queueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[i].queueFamilyIndex = uniqueFamilies[i];
        queueInfos[i].queueCount = 1;
        queueInfos[i].pQueuePriorities = &queuePriority;
    }

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = uniqueCount;
    deviceInfo.pQueueCreateInfos = queueInfos;
    deviceInfo.pEnabledFeatures = &features;
    deviceInfo.enabledExtensionCount = ArrayCount(RequiredDeviceExtensions);
    deviceInfo.ppEnabledExtensionNames = RequiredDeviceExtensions;

    VkResult result = vkCreateDevice(context->physicalDevice, &deviceInfo, nullptr, &context->device);
    if (result != VK_SUCCESS)
    {
        DebugLog("Fail to create logical device\n");
        return false;
    }

    context->graphicsQueue = CreateQueue(context->device, context->graphicsFamilyIndex);
    context->presentQueue  = CreateQueue(context->device, context->presentFamilyIndex);

    DebugLog("Logical device created\n");
    return true;
}

internal bool32 CreateSurface(vulkan_context *context, HINSTANCE hinstance, HWND hwnd)
{
    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = hinstance;
    surfaceInfo.hwnd = hwnd;

    VkResult result = vkCreateWin32SurfaceKHR(context->instance, &surfaceInfo, nullptr, &context->surface);
    if (result != VK_SUCCESS)
    {
        DebugLog("Fail to create window surface\n");
        return false;
    }

    DebugLog("Window surface created\n");
    return true;
}

internal VkSurfaceFormatKHR ChooseSwapSurfaceFormat(swapchain_support_details *support)
{
    // Нужен только ЛИНЕЙНЫЙ формат (UNORM, без авто-гаммы) — sRGB-форматы не берём.
    for (uint32 i = 0; i < support->formatCount; ++i)
    {
        if (support->formats[i].format == PREFERRED_SURFACE_FORMAT
            && support->formats[i].colorSpace == REQUIRED_COLOR_SPACE)
        {
            return support->formats[i];
        }
    }

    // запасной линейный вариант, если BGRA_UNORM нет
    for (uint32 i = 0; i < support->formatCount; ++i)
    {
        if (support->formats[i].format == FALLBACK_SURFACE_FORMAT)
        {
            return support->formats[i];
        }
    }

    return support->formats[0];   // крайний случай (на десктопе не достигается)
}

internal VkPresentModeKHR ChooseSwapPresentMode(swapchain_support_details *support)
{
    // MAILBOX (тройная буферизация) если есть
    for (uint32 i = 0; i < support->presentModeCount; ++i)
    {
        if (support->presentModes[i] == PREFERRED_PRESENT_MODE)
        {
            return PREFERRED_PRESENT_MODE;
        }
    }

    return FALLBACK_PRESENT_MODE;   // гарантированно поддерживается (v-sync)
}

internal VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR *capabilities, HWND hwnd)
{
    // Windows обычно сам задаёт точный размер в currentExtent
    if (capabilities->currentExtent.width != UINT32_MAX)
    {
        return capabilities->currentExtent;
    }

    // иначе берём клиентскую область окна и зажимаем в допустимые пределы
    RECT rect;
    GetClientRect(hwnd, &rect);
    uint32 width  = (uint32)(rect.right - rect.left);
    uint32 height = (uint32)(rect.bottom - rect.top);

    if (width  < capabilities->minImageExtent.width)  width  = capabilities->minImageExtent.width;
    if (width  > capabilities->maxImageExtent.width)  width  = capabilities->maxImageExtent.width;
    if (height < capabilities->minImageExtent.height) height = capabilities->minImageExtent.height;
    if (height > capabilities->maxImageExtent.height) height = capabilities->maxImageExtent.height;

    VkExtent2D extent;
    extent.width  = width;
    extent.height = height;
    return extent;
}

internal bool32 CreateSwapchain(vulkan_context *context, HWND hwnd)
{
    swapchain_support_details support = QuerySwapchainSupport(context->physicalDevice, context->surface);

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(&support);
    VkPresentModeKHR   presentMode   = ChooseSwapPresentMode(&support);
    VkExtent2D         extent        = ChooseSwapExtent(&support.capabilities, hwnd);

    // на одну картинку больше минимума — чтобы реже ждать драйвер
    uint32 imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
    {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32 familyIndices[] = { context->graphicsFamilyIndex, context->presentFamilyIndex };
    if (context->graphicsFamilyIndex != context->presentFamilyIndex)
    {
        // разные семейства — картинки делятся между ними
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = familyIndices;
    }
    else
    {
        // одно семейство — эксклюзивный доступ (быстрее)
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(context->device, &createInfo, nullptr, &context->swapchain);
    if (result != VK_SUCCESS)
    {
        DebugLog("Fail to create swapchain\n");
        return false;
    }

    // получаем сами изображения цепочки
    uint32 count = 0;
    vkGetSwapchainImagesKHR(context->device, context->swapchain, &count, nullptr);
    if (count > MAX_SWAPCHAIN_IMAGES)
    {
        count = MAX_SWAPCHAIN_IMAGES;
    }
    context->swapchainImageCount = count;
    vkGetSwapchainImagesKHR(context->device, context->swapchain, &count, context->swapchainImages);

    context->swapchainImageFormat = surfaceFormat.format;
    context->swapchainExtent = extent;

    DebugLog("Swapchain created (%u images, %ux%u)\n", count, extent.width, extent.height);
    return true;
}

internal bool32 CreateImageViews(vulkan_context *context)
{
    for (uint32 i = 0; i < context->swapchainImageCount; ++i)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = context->swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = context->swapchainImageFormat;

        // без перестановки каналов — как есть
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // цветовое изображение, один mip-уровень, один слой
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(context->device, &createInfo, nullptr, &context->swapchainImageViews[i]) != VK_SUCCESS)
        {
            DebugLog("Fail to create image view\n");
            return false;
        }
    }

    DebugLog("Image views created (%u)\n", context->swapchainImageCount);
    return true;
}

internal void InitVulkan(HINSTANCE hinstance, HWND hwnd)
{
    vulkan_context *context = &GlobalVulkan;

    if (!CheckInstanceExtensionSupport(RequiredInstanceExtensions, ArrayCount(RequiredInstanceExtensions)))
    {
        DebugLog("Required instance extensions missing\n");
        return;
    }

    VkApplicationInfo appInfo = VkGetInfo();
    VkInstanceCreateInfo InstanceInfo = GetInstanceInfo(&appInfo, RequiredInstanceExtensions, ArrayCount(RequiredInstanceExtensions));

    if (vkCreateInstance(&InstanceInfo, nullptr, &context->instance) != VK_SUCCESS)
    {
        DebugLog("Fail to create vulkan instance\n");
        return;
    }
    DebugLog("Vulkan instance created\n");

    // На любом провале просто выходим: частично созданное снесёт ShutdownVulkan
    // (он null-безопасен). Контекст глобальный, живёт весь сеанс.
    if (!CreateSurface(context, hinstance, hwnd))      return;
    if (!SelectDevice(context))                        { DebugLog("No suitable GPU found\n"); return; }
    if (!CreateLogicalDevice(context))                 return;
    if (!CreateSwapchain(context, hwnd))               return;
    if (!CreateImageViews(context))                    return;

    // Загрузка по имени; в 2.8 байты пойдут в VkShaderModule. Пока — проверка.
    vulkan_shader shader = LoadShader("default");
    FreeShader(&shader);
}

internal void ShutdownVulkan()
{
    vulkan_context *context = &GlobalVulkan;

    // дождаться завершения GPU, затем сносить в обратном порядке создания
    if (context->device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(context->device);

        for (uint32 i = 0; i < context->swapchainImageCount; ++i)
        {
            vkDestroyImageView(context->device, context->swapchainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(context->device, context->swapchain, nullptr);
        vkDestroyDevice(context->device, nullptr);
    }

    if (context->instance != VK_NULL_HANDLE)
    {
        if (context->surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(context->instance, context->surface, nullptr);
        }
        vkDestroyInstance(context->instance, nullptr);
    }
}



 
