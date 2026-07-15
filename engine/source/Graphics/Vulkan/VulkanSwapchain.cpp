#include "Vulkan.h"

#define PREFERRED_SURFACE_FORMAT VK_FORMAT_B8G8R8A8_UNORM
#define FALLBACK_SURFACE_FORMAT  VK_FORMAT_R8G8B8A8_UNORM
#define REQUIRED_COLOR_SPACE     VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define PREFERRED_PRESENT_MODE   VK_PRESENT_MODE_MAILBOX_KHR
#define FALLBACK_PRESENT_MODE    VK_PRESENT_MODE_FIFO_KHR

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
        if (support->formats[i].format == FALLBACK_SURFACE_FORMAT)
        {
            return support->formats[i];
        }
    }

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

    DebugLog("Swapchain created (%u images, %ux%u)\n", count, extent.width, extent.height);
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

    if (!CreateImage(context, context->swapchainExtent.width, context->swapchainExtent.height,
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

internal bool32 CreateRenderPass(vulkan_context *context)
{

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = context->swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = context->depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(context->device, &renderPassInfo, nullptr, &context->renderPass);
    if (result != VK_SUCCESS)
    {
        DebugLog("Fail to create render pass\n");
        return false;
    }

    DebugLog("Render pass created\n");
    return true;
}

internal bool32 CreateFramebuffers(vulkan_context *context)
{
    for (uint32 i = 0; i < context->swapchainImageCount; ++i)
    {

        VkImageView attachments[2] = { context->swapchainImageViews[i], context->depthImageView };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = context->renderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
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

internal bool32 RecreateSwapchain(vulkan_context *context)
{

    RECT rect;
    GetClientRect(context->windowHandle, &rect);
    if ((rect.right - rect.left) <= 0 || (rect.bottom - rect.top) <= 0)
    {
        return false;
    }

    vkDeviceWaitIdle(context->device);

    for (uint32 i = 0; i < context->swapchainImageCount; ++i)
    {
        vkDestroyFramebuffer(context->device, context->swapchainFramebuffers[i], nullptr);
    }

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
    if (!CreateFramebuffers(context))                     return false;

    return true;
}
