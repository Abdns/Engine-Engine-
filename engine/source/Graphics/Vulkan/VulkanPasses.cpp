#include "Vulkan.h"

internal render_target CreateRenderTarget(vulkan_context *context, VkFormat format)
{
    render_target target = {};

    if (!CreateStandaloneImage(
        context, 
        context->swapchainExtent.width, 
        context->swapchainExtent.height,
        format, 
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        &target.Image, &target.Memory))
    {
        DebugLog("Fail to create render target image\n");
        return target;
    }

    target.View = CreateColorImageView(context->device, target.Image, format);

    return target;
}

internal void DestroyRenderTarget(vulkan_context *context, render_target *target)
{
    vkDestroyImageView(context->device, target->View, nullptr);
    vkDestroyImage(context->device, target->Image, nullptr);
    vkFreeMemory(context->device, target->Memory, nullptr);

    *target = {};
}

internal uint32 BuildPassDependencies(pass_sync sync, VkSubpassDependency *deps)
{
    switch (sync)
    {
        case Sync_WriteThenSample:
        {
            deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
            deps[0].dstSubpass    = 0;
            deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            deps[1].srcSubpass    = 0;
            deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
            deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            return 2;
        }

        case Sync_WriteThenPresent:
        {
            deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
            deps[0].dstSubpass    = 0;
            deps[0].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            deps[0].srcAccessMask = 0;
            deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            return 1;
        }

        case Sync_None:
        {
            return 0;
        }
    }

    return 0;
}

internal render_pass CreatePass(vulkan_context *context, pass_desc *Desc, VkImageView *colorViews, uint32 colorViewCount, VkImageView depthView)
{
    render_pass pass = {};
    pass.Desc = *Desc;

    VkAttachmentDescription attachments[2] = {};
    attachments[0].format         = Desc->ColorFormat;
    attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp         = Desc->ColorLoad;
    attachments[0].storeOp        = Desc->ColorStore;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout    = Desc->ColorFinalLayout;

    uint32 attachmentCount = 1;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    if (Desc->UseDepth)
    {
        attachments[1].format         = context->depthFormat;
        attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachmentCount = 2;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = Desc->UseDepth ? &depthRef : nullptr;

    VkSubpassDependency deps[MAX_PASS_DEPENDENCIES] = {};
    uint32 depCount = BuildPassDependencies(Desc->Sync, deps);

    VkRenderPassCreateInfo passInfo{};
    passInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    passInfo.attachmentCount = attachmentCount;
    passInfo.pAttachments    = attachments;
    passInfo.subpassCount    = 1;
    passInfo.pSubpasses      = &subpass;
    passInfo.dependencyCount = depCount;
    passInfo.pDependencies   = deps;

    if (vkCreateRenderPass(context->device, &passInfo, nullptr, &pass.Handle) != VK_SUCCESS)
    {
        DebugLog("Fail to create '%s' render pass\n", Desc->Name);
        return pass;
    }

    pass.FramebufferCount = colorViewCount;
    for (uint32 i = 0; i < colorViewCount; ++i)
    {
        VkImageView views[2] = { colorViews[i], depthView };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = pass.Handle;
        framebufferInfo.attachmentCount = attachmentCount;
        framebufferInfo.pAttachments    = views;
        framebufferInfo.width           = context->swapchainExtent.width;
        framebufferInfo.height          = context->swapchainExtent.height;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(context->device, &framebufferInfo, nullptr, &pass.Framebuffers[i]) != VK_SUCCESS)
        {
            DebugLog("Fail to create '%s' pass framebuffer\n", Desc->Name);
        }
    }

    return pass;
}

internal render_pass CreateScenePass(vulkan_context *context, VkImageView target, VkImageView depthView)
{
    pass_desc Desc = {};
    Desc.Name              = "scene";
    Desc.ColorFormat       = VK_FORMAT_R16G16B16A16_SFLOAT;
    Desc.ColorLoad         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Desc.ColorStore        = VK_ATTACHMENT_STORE_OP_STORE;
    Desc.ColorFinalLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Desc.ClearColor        = Vector4(0.05f, 0.05f, 0.08f, 1.0f);
    Desc.UseDepth          = true;
    Desc.PerSwapchainImage = false;
    Desc.Sync              = Sync_WriteThenSample;

    return CreatePass(context, &Desc, &target, 1, depthView);
}

internal render_pass CreatePostPass(vulkan_context *context, VkImageView *swapchainViews, uint32 swapchainViewCount)
{
    pass_desc Desc = {};
    Desc.Name              = "post";
    Desc.ColorFormat       = context->swapchainImageFormat;
    Desc.ColorLoad         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Desc.ColorStore        = VK_ATTACHMENT_STORE_OP_STORE;
    Desc.ColorFinalLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    Desc.UseDepth          = false;
    Desc.PerSwapchainImage = true;
    Desc.Sync              = Sync_WriteThenPresent;

    return CreatePass(context, &Desc, swapchainViews, swapchainViewCount, VK_NULL_HANDLE);
}

internal void DestroyPass(vulkan_context *context, render_pass *pass)
{
    for (uint32 i = 0; i < pass->FramebufferCount; ++i)
    {
        vkDestroyFramebuffer(context->device, pass->Framebuffers[i], nullptr);
    }

    vkDestroyRenderPass(context->device, pass->Handle, nullptr);

    *pass = {};
}

internal void BeginPass(VkCommandBuffer cmd, render_pass *pass, VkExtent2D extent, uint32 imageIndex)
{
    VkClearValue clears[2] = {};
    clears[0].color.float32[0] = pass->Desc.ClearColor.X;
    clears[0].color.float32[1] = pass->Desc.ClearColor.Y;
    clears[0].color.float32[2] = pass->Desc.ClearColor.Z;
    clears[0].color.float32[3] = pass->Desc.ClearColor.W;
    clears[1].depthStencil.depth   = 1.0f;
    clears[1].depthStencil.stencil = 0;

    uint32 framebufferIndex = pass->Desc.PerSwapchainImage ? imageIndex : 0;

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass        = pass->Handle;
    beginInfo.framebuffer       = pass->Framebuffers[framebufferIndex];
    beginInfo.renderArea.extent = extent;
    beginInfo.clearValueCount   = pass->Desc.UseDepth ? 2 : 1;
    beginInfo.pClearValues      = clears;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

internal void EndPass(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}
