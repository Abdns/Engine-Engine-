#include "VulkanApi.h"
#include <stdio.h>

#define MAX_EXTENSIONS 256
#define MAX_DEVICES 4
#define MAX_FAMILY_COUNT 8

struct vulkan_context
{
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    uint32 graphicsFamilyIndex;
};

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

internal VkInstanceCreateInfo GetInstanceInfo(VkApplicationInfo *appInfo)
{
    VkInstanceCreateInfo createInfo{};
    
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = appInfo;

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
        printf("No vulkan devices support\n");
    }
    else
    {
	printf("vulkan devices support %d\n", devicesCount );
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

internal uint32 GetDeviceQueueFamily(const VkPhysicalDevice *device, VkQueueFlagBits queueFlag)
{
    uint32 familyCount = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(*device, &familyCount, nullptr);
    
    if (familyCount > MAX_FAMILY_COUNT)
    {
        familyCount = MAX_FAMILY_COUNT;
    }

    VkQueueFamilyProperties families[MAX_FAMILY_COUNT];
    vkGetPhysicalDeviceQueueFamilyProperties(*device, &familyCount, families);

    for (uint32 i = 0; i < familyCount; ++i)
    {
        if (families[i].queueFlags & queueFlag)
	{
          return i;   
	}
    }

    return UINT32_MAX;
	
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
        uint32 familyIndex = GetDeviceQueueFamily(&devices[i], VK_QUEUE_GRAPHICS_BIT);

        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && familyIndex != UINT32_MAX)
	{
            context->physicalDevice = devices[i];
            context->graphicsFamilyIndex = familyIndex;
            return true;
	}
    }

    return false;
}

internal bool32 CreateLogicalDevice(vulkan_context *context)
{
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = context->graphicsFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.pEnabledFeatures = &features;

    VkResult result = vkCreateDevice(context->physicalDevice, &deviceInfo, nullptr, &context->device);
    if (result != VK_SUCCESS)
    {
        printf("Fail to create logical device\n");
        return false;
    }

    vkGetDeviceQueue(context->device, context->graphicsFamilyIndex, 0, &context->graphicsQueue);

    printf("Logical device created\n");
    return true;
}

internal void InitVulkan()
{
    vulkan_context context = {};

    VkExtensionProperties props[MAX_EXTENSIONS];
    uint32 extensionCount = GetExtensions(props, ArrayCount(props));

    VkApplicationInfo appInfo = VkGetInfo();
    VkInstanceCreateInfo InstanceInfo = GetInstanceInfo(&appInfo);

    VkResult result = vkCreateInstance(&InstanceInfo, nullptr, &context.instance);

    if (result != VK_SUCCESS)
    {
        printf("Fail to create vulkan instance\n");
        return;
    }

    printf("Vulkan instance created\n");

    if (!SelectDevice(&context))
    {
	printf("No suitable GPU found\n");
        vkDestroyInstance(context.instance, nullptr);
        return;
    }

    if (!CreateLogicalDevice(&context))
    {
        vkDestroyInstance(context.instance, nullptr);
        return;
    }

    vkDestroyDevice(context.device, nullptr);
    vkDestroyInstance(context.instance, nullptr);
}



 
