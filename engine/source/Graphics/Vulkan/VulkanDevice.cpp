#include "Vulkan.h"
#include "Strings.h"

global_variable const char *RequiredInstanceExtensions[] =
{
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
};

global_variable const char *RequiredDeviceExtensions[] =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

global_variable const char *OptionalDeviceExtensions[] =
{
    VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
};

#define REQUIRED_API_VERSION VK_API_VERSION_1_3

#define REQUIRED_DEVICE_TYPE VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU

#define MAX_EXTENSIONS        256
#define MAX_DEVICES           4
#define MAX_FAMILY_COUNT      8
#define MAX_DEVICE_EXTENSIONS 256

#define PREFERRED_SURFACE_FORMAT VK_FORMAT_B8G8R8A8_SRGB
#define FALLBACK_SURFACE_FORMAT  VK_FORMAT_R8G8B8A8_SRGB
#define REQUIRED_COLOR_SPACE     VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define PREFERRED_PRESENT_MODE   VK_PRESENT_MODE_MAILBOX_KHR
#define FALLBACK_PRESENT_MODE    VK_PRESENT_MODE_FIFO_KHR

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

internal bool32 CheckInstanceVersion()
{
    PFN_vkEnumerateInstanceVersion enumerateVersion =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");

    uint32 version = VK_API_VERSION_1_0;
    if (enumerateVersion && enumerateVersion(&version) != VK_SUCCESS)
    {
        version = VK_API_VERSION_1_0;
    }

    if (version < REQUIRED_API_VERSION)
    {
        DebugLog("Vulkan loader is %u.%u, need 1.3 for core dynamic state\n", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version));
        return false;
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
    appInfo.apiVersion = REQUIRED_API_VERSION;

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

internal uint32 GetDevices(const VkInstance *instance, VkPhysicalDevice *devices, uint32 maxCount)
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
        DebugLog("vulkan devices support %d\n", devicesCount);
    }

    vkEnumeratePhysicalDevices(*instance, &devicesCount, devices);

    return devicesCount;
}

internal void GetDevicePropsAndFeatures(const VkPhysicalDevice *devise, VkPhysicalDeviceProperties *deviceProperties, VkPhysicalDeviceFeatures *features)
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

        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            result.graphicsSupported = true;
            result.graphicsIndex = i;
        }

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

internal bool32 DeviceSupportsExtension(VkPhysicalDevice device, const char *name)
{
    return CheckDeviceExtensionSupport(device, &name, 1);
}

internal swapchain_support_details GetQuerySwapchainSupportDetails(VkPhysicalDevice device, VkSurfaceKHR surface)
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
    uint32 devicesCount = GetDevices(&context->instance, devices, ArrayCount(devices));

    for (uint32 i = 0; i < devicesCount; i++)
    {
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures features;

        GetDevicePropsAndFeatures(&devices[i], &deviceProperties, &features);
        queue_family_indices indices = SelectQueueFamilyIndices(devices[i], context->surface);

        if (!CheckDeviceExtensionSupport(devices[i], RequiredDeviceExtensions, ArrayCount(RequiredDeviceExtensions)))
        {
            continue;
        }

        swapchain_support_details swapchain = GetQuerySwapchainSupportDetails(devices[i], context->surface);
        bool32 swapchainOk = (swapchain.formatCount > 0) && (swapchain.presentModeCount > 0);

        if (deviceProperties.apiVersion < REQUIRED_API_VERSION)
        {
            DebugLog("Device '%s' is Vulkan %u.%u, need 1.3\n", deviceProperties.deviceName, VK_API_VERSION_MAJOR(deviceProperties.apiVersion), VK_API_VERSION_MINOR(deviceProperties.apiVersion));
            continue;
        }

        if (deviceProperties.deviceType == REQUIRED_DEVICE_TYPE && indices.graphicsSupported && indices.presentSupported && swapchainOk &&
            features.shaderSampledImageArrayDynamicIndexing)
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

    const char *enabledExtensions[ArrayCount(RequiredDeviceExtensions) + ArrayCount(OptionalDeviceExtensions)];
    uint32      enabledExtensionCount = 0;

    for (uint32 i = 0; i < ArrayCount(RequiredDeviceExtensions); ++i)
    {
        enabledExtensions[enabledExtensionCount++] = RequiredDeviceExtensions[i];
    }

    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamicState3{};
    dynamicState3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;

    VkPhysicalDeviceVulkan12Features vulkan12{};
    vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    {
        dynamicState3.pNext = &vulkan12;

        VkPhysicalDeviceFeatures2 available{};
        available.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        available.pNext = &dynamicState3;
        vkGetPhysicalDeviceFeatures2(context->physicalDevice, &available);

        if (dynamicState3.extendedDynamicState3ColorBlendEnable &&
            DeviceSupportsExtension(context->physicalDevice, VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME))
        {
            enabledExtensions[enabledExtensionCount++] = VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME;
            context->DynamicBlend = true;
        }

        if (!vulkan12.descriptorBindingPartiallyBound || !vulkan12.descriptorBindingSampledImageUpdateAfterBind)
        {
            DebugLog("Device lacks descriptor indexing (partiallyBound %d, updateAfterBind %d)\n",
                     (int)vulkan12.descriptorBindingPartiallyBound, (int)vulkan12.descriptorBindingSampledImageUpdateAfterBind);
            return false;
        }
    }

    dynamicState3 = {};
    dynamicState3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
    dynamicState3.extendedDynamicState3ColorBlendEnable = VK_TRUE;

    vulkan12 = {};
    vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12.descriptorBindingPartiallyBound              = VK_TRUE;
    vulkan12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vulkan12.pNext = context->DynamicBlend ? &dynamicState3 : nullptr;

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    features.pNext = &vulkan12;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &features;
    deviceInfo.queueCreateInfoCount = uniqueCount;
    deviceInfo.pQueueCreateInfos = queueInfos;
    deviceInfo.pEnabledFeatures = nullptr;
    deviceInfo.enabledExtensionCount = enabledExtensionCount;
    deviceInfo.ppEnabledExtensionNames = enabledExtensions;

    VkResult result = vkCreateDevice(context->physicalDevice, &deviceInfo, nullptr, &context->device);
    if (result != VK_SUCCESS)
    {
        DebugLog("Fail to create logical device\n");
        return false;
    }

    if (context->DynamicBlend)
    {
        context->CmdSetColorBlendEnableEXT = (PFN_vkCmdSetColorBlendEnableEXT)vkGetDeviceProcAddr(context->device, "vkCmdSetColorBlendEnableEXT");
        if (!context->CmdSetColorBlendEnableEXT)
        {
            context->DynamicBlend = false;
        }
    }

    context->graphicsQueue = CreateQueue(context->device, context->graphicsFamilyIndex);
    context->presentQueue  = CreateQueue(context->device, context->presentFamilyIndex);

    DebugLog("Logical device created (dynamic blend %s)\n", context->DynamicBlend ? "on" : "off");
    return true;
}

internal VkSurfaceFormatKHR ChooseSwapSurfaceFormat(swapchain_support_details *support)
{

    for (uint32 i = 0; i < support->formatCount; ++i)
    {
        if (support->formats[i].format == PREFERRED_SURFACE_FORMAT
            && support->formats[i].colorSpace == REQUIRED_COLOR_SPACE)
        {
            return support->formats[i];
        }
    }

    for (uint32 i = 0; i < support->formatCount; ++i)
    {
        if (support->formats[i].format == FALLBACK_SURFACE_FORMAT
            && support->formats[i].colorSpace == REQUIRED_COLOR_SPACE)
        {
            return support->formats[i];
        }
    }

    DebugLog("No sRGB surface format available, colours will be too dark\n");

    return support->formats[0];
}

internal VkPresentModeKHR ChooseSwapPresentMode(swapchain_support_details *support)
{

    for (uint32 i = 0; i < support->presentModeCount; ++i)
    {
        if (support->presentModes[i] == PREFERRED_PRESENT_MODE)
        {
            return PREFERRED_PRESENT_MODE;
        }
    }

    return FALLBACK_PRESENT_MODE;
}

internal VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR *capabilities, HWND hwnd)
{

    if (capabilities->currentExtent.width != UINT32_MAX)
    {
        return capabilities->currentExtent;
    }

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
    swapchain_support_details support = GetQuerySwapchainSupportDetails(context->physicalDevice, context->surface);

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(&support);
    VkPresentModeKHR   presentMode   = ChooseSwapPresentMode(&support);
    VkExtent2D         extent        = ChooseSwapExtent(&support.capabilities, hwnd);

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

        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = familyIndices;
    }
    else
    {

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

    DebugLog("Swapchain created (%u images, %ux%u, format %d)\n", count, extent.width, extent.height, surfaceFormat.format);
    return true;
}

internal bool32 CreateImageViews(vulkan_context *context)
{
    for (uint32 i = 0; i < context->swapchainImageCount; ++i)
    {
        context->swapchainImageViews[i] = CreateColorImageView(context->device, context->swapchainImages[i], context->swapchainImageFormat);
        if (context->swapchainImageViews[i] == VK_NULL_HANDLE)
        {
            return false;
        }
    }

    DebugLog("Image views created (%u)\n", context->swapchainImageCount);
    return true;
}

internal bool32 CreateDepthResources(vulkan_context *context)
{
    context->depthFormat = VK_FORMAT_D32_SFLOAT;

    if (!CreateStandaloneImage(context, context->swapchainExtent.width, context->swapchainExtent.height,
                     context->depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &context->depthImage, &context->depthImageMemory))
    {
        DebugLog("Fail to create depth image\n");
        return false;
    }

    context->depthImageView = CreateDepthImageView(context->device, context->depthImage, context->depthFormat);
    if (context->depthImageView == VK_NULL_HANDLE)
    {
        return false;
    }

    DebugLog("Depth resources created\n");
    return true;
}

internal bool32 RecreateSwapchain(vulkan_context *context)
{

    RECT rect;
    GetClientRect(context->windowHandle, &rect);
    if ((rect.right - rect.left) <= 0 || (rect.bottom - rect.top) <= 0)
    {
        return false;
    }

    vkDeviceWaitIdle(context->device);

    vkDestroyImageView(context->device, context->depthImageView, nullptr);
    vkDestroyImage(context->device, context->depthImage, nullptr);
    vkFreeMemory(context->device, context->depthImageMemory, nullptr);

    for (uint32 i = 0; i < context->swapchainImageCount; ++i)
    {
        vkDestroyImageView(context->device, context->swapchainImageViews[i], nullptr);
    }
    vkDestroySwapchainKHR(context->device, context->swapchain, nullptr);

    if (!CreateSwapchain(context, context->windowHandle)) return false;
    if (!CreateImageViews(context))                       return false;
    if (!CreateDepthResources(context))                   return false;

    return true;
}

internal bool32 CreateCommandPool(vulkan_context *context)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context->graphicsFamilyIndex;

    if (vkCreateCommandPool(context->device, &poolInfo, nullptr, &context->commandPool) != VK_SUCCESS)
    {
        DebugLog("Fail to create command pool\n");
        return false;
    }

    DebugLog("Command pool created\n");
    return true;
}

internal bool32 CreateCommandBuffer(vulkan_context *context)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = context->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(context->device, &allocInfo, &context->commandBuffer) != VK_SUCCESS)
    {
        DebugLog("Fail to allocate command buffer\n");
        return false;
    }

    DebugLog("Command buffer allocated\n");
    return true;
}

internal bool32 CreateSyncObjects(vulkan_context *context)
{
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(context->device, &semInfo, nullptr, &context->imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateFence(context->device, &fenceInfo, nullptr, &context->inFlightFence) != VK_SUCCESS)
    {
        DebugLog("Fail to create sync objects\n");
        return false;
    }

    for (uint32 i = 0; i < context->swapchainImageCount; ++i)
    {
        if (vkCreateSemaphore(context->device, &semInfo, nullptr, &context->renderFinishedSemaphores[i]) != VK_SUCCESS)
        {
            DebugLog("Fail to create render-finished semaphore\n");
            return false;
        }
    }

    DebugLog("Sync objects created\n");
    return true;
}
