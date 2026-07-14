#include "Vulkan.h"

internal void RecordCommandBuffer(vulkan_context *context, VkCommandBuffer cmd, uint32 imageIndex, render_commands *commands)
{
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearValue clearValues[2] = {};
    clearValues[0].color.float32[0] = commands->ClearR;
    clearValues[0].color.float32[1] = commands->ClearG;
    clearValues[0].color.float32[2] = commands->ClearB;
    clearValues[0].color.float32[3] = 1.0f;
    clearValues[1].depthStencil.depth = 1.0f;
    clearValues[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = context->renderPass;
    renderPassInfo.framebuffer = context->swapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset.x = 0;
    renderPassInfo.renderArea.offset.y = 0;
    renderPassInfo.renderArea.extent = context->swapchainExtent;
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->Pipeline.Handle);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width  = (float)context->swapchainExtent.width;
    viewport.height = (float)context->swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = context->swapchainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    real32  aspect = (real32)context->swapchainExtent.width / (real32)context->swapchainExtent.height;
    Matrix4 view   = commands->CameraView;
    real32  fovY   = commands->CameraFovY;
    Matrix4 proj   = Mat4Perspective(fovY, aspect, 0.1f, 100.0f);

    camera_uniforms *camera = (camera_uniforms *)context->Pipeline.CameraMapped;
    camera->ViewProj = Mat4Multiply(proj, view);

    BindDescriptorSet(cmd, &context->Pipeline, Frequency_PerFrame, context->Pipeline.CameraSet);

    v4 tintPalette[4] = { V4(1.0f, 1.0f, 1.0f, 1.0f), V4(1.0f, 0.5f, 0.5f, 1.0f), V4(0.5f, 1.0f, 0.5f, 1.0f), V4(0.5f, 0.5f, 1.0f, 1.0f) };

    for (uint32 i = 0; i < commands->MeshCount; ++i)
    {
        render_entry_mesh *entry = &commands->Meshes[i];
        if (entry->MeshID >= context->MeshCount)
        {
            continue;
        }

        gpu_mesh *mesh = &context->Meshes[entry->MeshID];

        uint32 texId = entry->TextureID < context->TextureCount ? entry->TextureID : 0;
        BindDescriptorSet(cmd, &context->Pipeline, Frequency_PerMaterial, context->Textures[texId].DescriptorSet);

        uint32 objIndex = i < MAX_OBJECTS ? i : 0;
        uint32 objOffset = objIndex * OBJECT_STRIDE;
        object_uniforms *object = (object_uniforms *)((uint8 *)context->Pipeline.ObjectMapped + objOffset);
        object->Tint = tintPalette[i % 4];
        BindDynamicDescriptorSet(cmd, &context->Pipeline, Frequency_PerObject, context->Pipeline.ObjectSet, objOffset);

        Matrix4 translate = Mat4Translation(entry->Position.X, entry->Position.Y, entry->Position.Z);
        Matrix4 rotate    = Mat4Multiply(Mat4RotationY(entry->Rotation), Mat4RotationX(entry->Rotation * 0.7f));
        Matrix4 model     = Mat4Multiply(translate, rotate);

        cube_push_constants pc;
        pc.Model = model;
        vkCmdPushConstants(cmd, context->Pipeline.Layout, VK_SHADER_STAGE_VERTEX_BIT, 0, (uint32)sizeof(pc), &pc);

        VkBuffer vertexBuffers[] = { mesh->VertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdDraw(cmd, mesh->VertexCount, 1, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

internal bool32 DrawFrame(vulkan_context *context, render_commands *commands)
{
    if (context->Pipeline.Handle == VK_NULL_HANDLE)
    {
        return false;
    }

    vkWaitForFences(context->device, 1, &context->inFlightFence, VK_TRUE, UINT64_MAX);

    uint32 imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX,
                                             context->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return true;
    }

    vkResetFences(context->device, 1, &context->inFlightFence);

    RecordCommandBuffer(context, context->commandBuffer, imageIndex, commands);

    VkSemaphore          waitSems[]   = { context->imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore          signalSems[] = { context->renderFinishedSemaphores[imageIndex] };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSems;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &context->commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSems;

    if (vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, context->inFlightFence) != VK_SUCCESS)
    {
        DebugLog("Fail to submit draw command buffer\n");
        return false;
    }

    VkSwapchainKHR swapchains[] = { context->swapchain };
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSems;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult present = vkQueuePresentKHR(context->presentQueue, &presentInfo);
    if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR)
    {
        return true;
    }

    return false;
}
