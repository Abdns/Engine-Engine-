#include "VulkanContext.h"
#include "VulkanShaders.h"
#include "VulkanRender.h"
#include "Strings.h"
#include "Debug.h"

global_variable const char *RequiredInstanceExtensions[] =
{
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
};

global_variable const char *RequiredDeviceExtensions[] =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#define REQUIRED_DEVICE_TYPE       VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
#define PREFERRED_SURFACE_FORMAT   VK_FORMAT_B8G8R8A8_UNORM          
#define FALLBACK_SURFACE_FORMAT    VK_FORMAT_R8G8B8A8_UNORM
#define REQUIRED_COLOR_SPACE       VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define PREFERRED_PRESENT_MODE     VK_PRESENT_MODE_MAILBOX_KHR       
#define FALLBACK_PRESENT_MODE      VK_PRESENT_MODE_FIFO_KHR          

global_variable vulkan_context GlobalVulkan;

// ============================================================================
//  Экземпляр и расширения
// ============================================================================

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

// ============================================================================
//  Поверхность
// ============================================================================

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

// ============================================================================
//  Выбор физического устройства
// ============================================================================

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
        DebugLog("vulkan devices support %d\n", devicesCount);
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

internal swapchain_support_details QuerySwapchainSupportDetails(VkPhysicalDevice device, VkSurfaceKHR surface)
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

        swapchain_support_details swapchain = QuerySwapchainSupportDetails(devices[i], context->surface);
        bool32 swapchainOk = (swapchain.formatCount > 0) && (swapchain.presentModeCount > 0);

        if (deviceProperties.deviceType == REQUIRED_DEVICE_TYPE && indices.graphicsSupported && indices.presentSupported && swapchainOk)
        {
            context->physicalDevice = devices[i];
            context->graphicsFamilyIndex = indices.graphicsIndex;
            context->presentFamilyIndex = indices.presentIndex;
            return true;
        }
    }

    return false;
}

// ============================================================================
//  Логическое устройство и очереди
// ============================================================================

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

// ============================================================================
//  Цепочка буферов (swapchain) и представления изображений
// ============================================================================

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
    swapchain_support_details support = QuerySwapchainSupportDetails(context->physicalDevice, context->surface);

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

// ============================================================================
//  Проход рендеринга
// ============================================================================

internal bool32 CreateRenderPass(vulkan_context *context)
{
    // Одно цветовое вложение = картинка swapchain, в которую рисуем
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = context->swapchainImageFormat;          // формат как у swapchain
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                 // без мультисэмплинга
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;           // в начале кадра — очистить
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;         // в конце — сохранить (увидим на экране)
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // стенсил не используем
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;       // что было раньше — не важно
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;   // в конце — готово к показу

    // Ссылка на вложение из подпрохода: индекс 0 + удобный для рисования layout
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;                              // attachment №0 (наш colorAttachment)
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Подпроход: один, графический, пишет в наше цветовое вложение
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VkResult result = vkCreateRenderPass(context->device, &renderPassInfo, nullptr, &context->renderPass);
    if (result != VK_SUCCESS)
    {
        DebugLog("Fail to create render pass\n");
        return false;
    }

    DebugLog("Render pass created\n");
    return true;
}

// ============================================================================
//  Графический конвейер (2.9 модули шейдеров, 2.10 фикс-функции, 2.11 конвейер)
// ============================================================================

// 2.9: обернуть SPIR-V байткод в VkShaderModule. codeSize в БАЙТАХ, pCode —
// поток 32-битных слов (отсюда каст к const uint32_t*; память из VirtualAlloc
// выровнена по странице, поэтому каст безопасен).
internal VkShaderModule CreateShaderModule(VkDevice device, void *code, uint32 codeSize)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = (const uint32_t *)code;

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
    {
        DebugLog("Fail to create shader module\n");
        return VK_NULL_HANDLE;
    }
    return module;
}

// 2.10: VkPipelineLayout описывает ресурсы шейдеров (descriptor sets, push
// constants). У треугольника их нет — всё по нулям, но объект обязателен.
internal bool32 CreatePipelineLayout(vulkan_context *context)
{
    // push-константы прямоугольника (Min/Max/Color/Screen) — их читает rect.vert
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = (uint32)sizeof(rect_push_constants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pSetLayouts = nullptr;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &context->pipelineLayout) != VK_SUCCESS)
    {
        DebugLog("Fail to create pipeline layout\n");
        return false;
    }

    DebugLog("Pipeline layout created\n");
    return true;
}

internal bool32 CreateGraphicsPipeline(vulkan_context *context)
{
    vulkan_shader shader = LoadShader("rect");
    if (!shader.valid)
    {
        DebugLog("Fail to load 'rect' shader\n");
        return false;
    }

    VkShaderModule vertModule = CreateShaderModule(context->device, shader.vert.Contents, shader.vert.ContentsSize);
    VkShaderModule fragModule = CreateShaderModule(context->device, shader.frag.Contents, shader.frag.ContentsSize);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE)
    {
        FreeShader(&shader);
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName  = "main";

    shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName  = "main";

    // --- 2.10: фиксированные стадии ---
    // вершинный ввод ПУСТ: треугольник зашит в шейдере (gl_VertexIndex), VkBuffer нет
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 0;
    vertexInput.pVertexBindingDescriptions = nullptr;
    vertexInput.vertexAttributeDescriptionCount = 0;
    vertexInput.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // каждые 3 вершины — треугольник
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // viewport/scissor — ДИНАМИЧЕСКИЕ: задаём при записи команд, чтобы не
    // пересобирать конвейер при ресайзе. Здесь — только их количество.
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;       // заливка (не каркас)
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;             // 2D-прямоугольники — без отсечения граней
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // без MSAA
    multisampling.minSampleShading = 1.0f;

    // блендинг выключен: цвет фрагмента просто перезаписывает пиксель
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = (uint32)ArrayCount(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;

    // --- 2.11: создание конвейера ---
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;   // глубины пока нет
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = context->pipelineLayout;
    pipelineInfo.renderPass = context->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkResult result = vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &context->graphicsPipeline);

    // модули нужны только во время компиляции конвейера — сносим сразу
    vkDestroyShaderModule(context->device, fragModule, nullptr);
    vkDestroyShaderModule(context->device, vertModule, nullptr);
    FreeShader(&shader);

    if (result != VK_SUCCESS)
    {
        DebugLog("Fail to create graphics pipeline\n");
        return false;
    }

    DebugLog("Graphics pipeline created\n");
    return true;
}

// ============================================================================
//  Буферы кадров (2.12)
// ============================================================================

// Один VkFramebuffer на каждый image view цепочки: привязывает render pass к
// конкретной картинке, в которую рисуем.
internal bool32 CreateFramebuffers(vulkan_context *context)
{
    for (uint32 i = 0; i < context->swapchainImageCount; ++i)
    {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = context->renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &context->swapchainImageViews[i];  // адрес одного view
        framebufferInfo.width = context->swapchainExtent.width;
        framebufferInfo.height = context->swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(context->device, &framebufferInfo, nullptr, &context->swapchainFramebuffers[i]) != VK_SUCCESS)
        {
            DebugLog("Fail to create framebuffer\n");
            return false;
        }
    }

    DebugLog("Framebuffers created (%u)\n", context->swapchainImageCount);
    return true;
}

// ============================================================================
//  Пул команд и буфер команд (2.13)
// ============================================================================

internal bool32 CreateCommandPool(vulkan_context *context)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // буфер можно перезаписывать каждый кадр
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
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;   // можно слать прямо в очередь
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(context->device, &allocInfo, &context->commandBuffer) != VK_SUCCESS)
    {
        DebugLog("Fail to allocate command buffer\n");
        return false;
    }

    DebugLog("Command buffer allocated\n");
    return true;
}

// ============================================================================
//  Объекты синхронизации (2.14)
// ============================================================================

// imageAvailable (GPU↔GPU): acquire -> submit. renderFinished (GPU↔GPU):
// submit -> present, по одному на картинку цепочки (нельзя переиспользовать,
// пока present ещё ждёт прошлый). inFlightFence (CPU↔GPU): не даёт CPU
// перезаписать буфер команд, пока GPU рисует прошлый кадр. Забор создаём СРАЗУ
// сигнальным — иначе первый vkWaitForFences завис бы навсегда.
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

// ============================================================================
//  Пересоздание swapchain при ресайзе (урок 2.16)
// ============================================================================

// Окно изменило размер -> swapchain устарел. Render pass и конвейер переживают
// (формат тот же, а viewport/scissor у нас ДИНАМИЧЕСКИЕ), поэтому пересоздаём
// только swapchain + image views + framebuffers.
internal bool32 RecreateSwapchain(vulkan_context *context)
{
    // окно свёрнуто (клиентская область 0) — не пересоздаём, ждём восстановления
    RECT rect;
    GetClientRect(context->windowHandle, &rect);
    if ((rect.right - rect.left) <= 0 || (rect.bottom - rect.top) <= 0)
    {
        return false;
    }

    vkDeviceWaitIdle(context->device);   // дождаться, пока GPU отпустит старые объекты

    for (uint32 i = 0; i < context->swapchainImageCount; ++i)
    {
        vkDestroyFramebuffer(context->device, context->swapchainFramebuffers[i], nullptr);
    }
    for (uint32 i = 0; i < context->swapchainImageCount; ++i)
    {
        vkDestroyImageView(context->device, context->swapchainImageViews[i], nullptr);
    }
    vkDestroySwapchainKHR(context->device, context->swapchain, nullptr);

    if (!CreateSwapchain(context, context->windowHandle)) return false;
    if (!CreateImageViews(context))                       return false;
    if (!CreateFramebuffers(context))                     return false;

    return true;
}

// ============================================================================
//  Точки входа подсистемы
// ============================================================================

internal void InitVulkan(HINSTANCE hinstance, HWND hwnd)
{
    vulkan_context *context = &GlobalVulkan;
    context->windowHandle = hwnd;   // понадобится для пересоздания swapchain при ресайзе

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

    if (!CreateSurface(context, hinstance, hwnd))      return;
    if (!SelectDevice(context))                        { DebugLog("No suitable GPU found\n"); return; }
    if (!CreateLogicalDevice(context))                 return;
    if (!CreateSwapchain(context, hwnd))               return;
    if (!CreateImageViews(context))                    return;
    if (!CreateRenderPass(context))                    return;
    if (!CreatePipelineLayout(context))                return;
    if (!CreateGraphicsPipeline(context))              return;
    if (!CreateFramebuffers(context))                  return;
    if (!CreateCommandPool(context))                   return;
    if (!CreateCommandBuffer(context))                 return;
    if (!CreateSyncObjects(context))                   return;

    DebugLog("Vulkan ready: triangle pipeline up\n");
}

// Вызывается каждый кадр из игрового цикла (Program.cpp). Тонкая обёртка, чтобы
// GlobalVulkan оставался приватным для этого файла (как InitVulkan/ShutdownVulkan).
internal void RenderVulkanFrame(render_commands *Commands)
{
    // DrawFrame вернёт true, если swapchain устарел (ресайз) -> пересоздаём; на
    // следующем кадре отрисовка пойдёт уже в новый размер.
    if (DrawFrame(&GlobalVulkan, Commands))
    {
        RecreateSwapchain(&GlobalVulkan);
    }
}

internal void ShutdownVulkan()
{
    vulkan_context *context = &GlobalVulkan;

    // дождаться завершения GPU, затем сносить в обратном порядке создания
    if (context->device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(context->device);

        // снос в обратном порядке создания (синхронизация -> команды -> framebuffers
        // -> конвейер -> layout), затем уже существующие render pass / views / swapchain
        vkDestroySemaphore(context->device, context->imageAvailableSemaphore, nullptr);
        for (uint32 i = 0; i < context->swapchainImageCount; ++i)
        {
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], nullptr);
        }
        vkDestroyFence(context->device, context->inFlightFence, nullptr);

        vkDestroyCommandPool(context->device, context->commandPool, nullptr);  // освобождает и буферы

        for (uint32 i = 0; i < context->swapchainImageCount; ++i)
        {
            vkDestroyFramebuffer(context->device, context->swapchainFramebuffers[i], nullptr);
        }

        vkDestroyPipeline(context->device, context->graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, nullptr);

        vkDestroyRenderPass(context->device, context->renderPass, nullptr);

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
