#include "Vulkan.h"

internal gpu_texture CreateRenderTarget(vulkan_context *context, VkFormat format)
{
    gpu_texture target = {};

    target.Image = CreateImage(
        context,
        context->swapchainExtent.width,
        context->swapchainExtent.height,
        format, 1,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &target.Memory);

    target.View = CreateColorImageView(context->device, target.Image, format);

    return target;
}

internal render_pass CreateScenePass(vulkan_context *context, VkImageView target)
{
    render_pass pass = {};
    pass.ColorLoad         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    pass.ClearColor        = Vector4(0.05f, 0.05f, 0.08f, 1.0f);
    pass.ColorViews[0]     = target;
    pass.UseDepth          = true;
    pass.PerSwapchainImage = false;

    return pass;
}

internal render_pass CreatePostPass(vulkan_context *context, VkImageView target)
{
    render_pass pass = {};
    pass.ColorLoad         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    pass.ColorViews[0]     = target;
    pass.UseDepth          = false;
    pass.PerSwapchainImage = false;

    return pass;
}

internal render_pass CreateUIPass(vulkan_context *context, VkImageView *swapchainViews, uint32 swapchainViewCount)
{
    render_pass pass = {};
    pass.ColorLoad         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    pass.UseDepth          = false;
    pass.PerSwapchainImage = true;

    for (uint32 i = 0; i < swapchainViewCount; ++i)
    {
        pass.ColorViews[i] = swapchainViews[i];
    }

    return pass;
}

internal void BeginPass(vulkan_context *context, VkCommandBuffer cmd, render_pass *pass, VkExtent2D extent, uint32 imageIndex)
{
    uint32 viewIndex = pass->PerSwapchainImage ? imageIndex : 0;

    VkRenderingAttachmentInfo color{};
    color.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView   = pass->ColorViews[viewIndex];
    color.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    color.loadOp      = pass->ColorLoad;
    color.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    color.clearValue.color.float32[0] = pass->ClearColor.X;
    color.clearValue.color.float32[1] = pass->ClearColor.Y;
    color.clearValue.color.float32[2] = pass->ClearColor.Z;
    color.clearValue.color.float32[3] = pass->ClearColor.W;

    VkRenderingAttachmentInfo depth{};
    depth.sType                        = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth.imageView                    = context->depth.View;
    depth.imageLayout                  = VK_IMAGE_LAYOUT_GENERAL;
    depth.loadOp                       = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp                      = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue.depthStencil.depth = 1.0f;

    VkRenderingInfo rendering{};
    rendering.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent    = extent;
    rendering.layerCount           = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments    = &color;
    rendering.pDepthAttachment     = pass->UseDepth ? &depth : nullptr;

    vkCmdBeginRendering(cmd, &rendering);
}

internal void EndPass(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
}

internal void GpuBarrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
{
    VkMemoryBarrier2 barrier{};
    barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask  = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask  = dstStage;
    barrier.dstAccessMask = dstAccess;

    VkDependencyInfo dependency{};
    dependency.sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers    = &barrier;

    vkCmdPipelineBarrier2(cmd, &dependency);
}
