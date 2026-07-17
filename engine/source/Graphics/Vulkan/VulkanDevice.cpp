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

#define REQUIRED_DEVICE_TYPE VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU

#define MAX_EXTENSIONS        256
#define MAX_DEVICES           4
#define MAX_FAMILY_COUNT      8
#define MAX_DEVICE_EXTENSIONS 256

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
    if (result != VK_SUCCESS)
    {
        return 0;
    }

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

        if (deviceProperties.deviceType == REQUIRED_DEVICE_TYPE && indices.graphicsSupported && indices.presentSupported && swapchainOk &&
            features.shaderSampledImageArrayDynamicIndexing)
        {
            context->physicalDevice = devices[i];
            context->graphicsFamilyIndex = indices.graphicsIndex;
            context->presentFamilyIndex = indices.presentIndex;
            context->uniformBufferAlignment = (uint32)deviceProperties.limits.minUniformBufferOffsetAlignment;
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

    VkPhysicalDeviceFeatures features{};
    features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

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
