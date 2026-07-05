#include "VulkanRender.h"   // свой интерфейс (DrawFrame) — чтобы объявление и определение совпадали
#include "VulkanContext.h"  // vulkan_context (общие данные подсистемы)
#include "Debug.h"          // DebugLog

internal void RecordCommandBuffer(vulkan_context *context, VkCommandBuffer cmd, uint32 imageIndex, render_commands *commands)
{
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // цвет очистки берём из первой команды Clear (иначе чёрный)
    VkClearValue clear = {};
    clear.color.float32[3] = 1.0f;
    for (uint64 off = 0; off < commands->BufferUsed; )
    {
        render_command_header *h = (render_command_header *)(commands->BufferBase + off);
        if (h->Type == RenderCommand_Clear)
        {
            render_command_clear *c = (render_command_clear *)h;
            clear.color.float32[0] = c->R;
            clear.color.float32[1] = c->G;
            clear.color.float32[2] = c->B;
            off += sizeof(render_command_clear);
        }
        else if (h->Type == RenderCommand_Rect) { off += sizeof(render_command_rect); }
        else { break; }
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = context->renderPass;
    renderPassInfo.framebuffer = context->swapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset.x = 0;
    renderPassInfo.renderArea.offset.y = 0;
    renderPassInfo.renderArea.extent = context->swapchainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clear;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->graphicsPipeline);

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

    // исполняем команды Rect: каждый прямоугольник = push-константа + draw(6 вершин)
    for (uint64 off = 0; off < commands->BufferUsed; )
    {
        render_command_header *h = (render_command_header *)(commands->BufferBase + off);
        if (h->Type == RenderCommand_Clear)
        {
            off += sizeof(render_command_clear);
        }
        else if (h->Type == RenderCommand_Rect)
        {
            render_command_rect *r = (render_command_rect *)h;
            rect_push_constants pc;
            pc.MinX = r->MinX; pc.MinY = r->MinY;
            pc.MaxX = r->MaxX; pc.MaxY = r->MaxY;
            pc.R = r->R; pc.G = r->G; pc.B = r->B; pc.A = 1.0f;
            pc.ScreenW = (float)context->swapchainExtent.width;
            pc.ScreenH = (float)context->swapchainExtent.height;
            vkCmdPushConstants(cmd, context->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, (uint32)sizeof(pc), &pc);
            vkCmdDraw(cmd, 6, 1, 0, 0);
            off += sizeof(render_command_rect);
        }
        else { break; }
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

// Отрисовка кадра (2.15): ждать забор -> получить картинку -> записать команды ->
// отправить в очередь -> показать на экране.
// Возвращает true, если swapchain устарел (ресайз) — пересоздание делает вызывающий.
internal bool32 DrawFrame(vulkan_context *context, render_commands *commands)
{
    // если инициализация не завершилась (например, шейдеры не нашлись) — не рисуем,
    // иначе ниже разыменуем null-хэндлы (забор/семафор) и упадём
    if (context->graphicsPipeline == VK_NULL_HANDLE)
    {
        return false;
    }

    // 1. дождаться, пока GPU закончит ПРОШЛЫЙ кадр (забор стартует сигнальным)
    vkWaitForFences(context->device, 1, &context->inFlightFence, VK_TRUE, UINT64_MAX);

    // 2. забрать свободную картинку цепочки; семафор сигналит, когда она готова
    uint32 imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX,
                                             context->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // окно изменило размер — сигналим вызывающему пересоздать swapchain
        return true;
    }

    // 3. сбрасываем забор только когда точно будем сабмитить (иначе ранний return
    //    выше оставил бы забор несигнальным -> дедлок на следующем кадре)
    vkResetFences(context->device, 1, &context->inFlightFence);

    // 4. записать команды рисования для выбранной картинки
    RecordCommandBuffer(context, context->commandBuffer, imageIndex, commands);

    // 5. отправить буфер в графическую очередь
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

    // 6. показать картинку: present ждёт renderFinished[imageIndex]
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
        return true;   // окно изменило размер -> пересоздать swapchain
    }

    return false;
}
