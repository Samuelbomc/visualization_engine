// =============================================================================
// vulkan_renderer_commands.cpp
// Gestión de command pools, command buffers, objetos de sincronización,
// grabación de comandos de renderizado y el bucle principal de dibujo (drawFrame).
// =============================================================================

#include "vulkan_renderer.hpp"
#include <stdexcept>

// -----------------------------------------------------------------------------
// createCommandPool: crea el command pool de gráficos vinculado a la familia
// de colas de gráficos, con el flag RESET_COMMAND_BUFFER_BIT para permitir
// resetear command buffers individuales entre frames.
// Si existe una familia de transferencia dedicada, crea un pool separado para
// ella; si no, reutiliza el pool de gráficos.
// -----------------------------------------------------------------------------
void VulkanRenderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    if (queueFamilyIndices.transferFamily.has_value() &&
        queueFamilyIndices.transferFamily.value() != queueFamilyIndices.graphicsFamily.value()) {
        VkCommandPoolCreateInfo transferPoolInfo{};
        transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        transferPoolInfo.queueFamilyIndex = queueFamilyIndices.transferFamily.value();

        if (vkCreateCommandPool(device, &transferPoolInfo, nullptr, &transferCommandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create transfer command pool!");
        }
    }
    else {
        transferCommandPool = commandPool;
    }
}

// -----------------------------------------------------------------------------
// createCommandBuffers: asigna MAX_FRAMES_IN_FLIGHT command buffers primarios
// del pool de gráficos, uno por cada frame en vuelo.
// -----------------------------------------------------------------------------
void VulkanRenderer::createCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

// -----------------------------------------------------------------------------
// createSyncObjects: crea los primitivos de sincronización para cada frame:
//   - imageAvailableSemaphores: la GPU los señaliza cuando adquiere una imagen.
//   - renderFinishedSemaphores: la GPU los señaliza cuando termina el renderizado.
//   - inFlightFences: la CPU espera en ellos para no sobreescribir recursos en uso.
// Los fences se crean señalizados para que el primer frame no se bloquee.
// -----------------------------------------------------------------------------
void VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects!");
        }
    }
}

// -----------------------------------------------------------------------------
// recordCommandBuffer: graba los comandos de renderizado para un frame.
//   1. Inicia el render pass con los valores de limpieza (negro para color,
//      1.0 para depth).
//   2. Si hay geometría cargada y pipeline válido:
//      a. Vincula el pipeline gráfico.
//      b. Configura viewport y scissor dinámicos al tamaño del swapchain.
//      c. Vincula el buffer de vértices.
//      d. Vincula el descriptor set del frame actual (UBO).
//      e. Ejecuta el dibujo: indexado si hay índices, directo si no.
//   3. Finaliza el render pass y el command buffer.
// -----------------------------------------------------------------------------
void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearValues[2]{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    const auto& geometry = mesh.getData();
    bool hasGeometry = graphicsPipeline != VK_NULL_HANDLE && geometry.vertexCount > 0;

    if (hasGeometry) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = { vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

        if (geometry.indexCount > 0) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, geometry.indexType);
            vkCmdDrawIndexed(commandBuffer, geometry.indexCount, 1, 0, 0, 0);
        }
        else {
            vkCmdDraw(commandBuffer, geometry.vertexCount, 1, 0, 0);
        }
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

// -----------------------------------------------------------------------------
// drawFrame: ejecuta el ciclo completo de un frame de renderizado.
//
// Flujo de sincronización (con 2 frames en vuelo):
//   CPU espera fence[N] → adquiere imagen → resetea fence[N] →
//   graba comandos → submit con wait(imageAvailable[N]) y signal(renderFinished[N])
//   y signal fence[N] → presenta con wait(renderFinished[N])
//
// Manejo de swapchain desactualizado:
//   - Si vkAcquireNextImageKHR devuelve OUT_OF_DATE, recrea el swapchain y
//     aborta el frame actual (el semáforo no fue consumido, pero el fence
//     no se reseteó, así que el próximo frame funciona correctamente).
//   - Si vkQueuePresentKHR devuelve OUT_OF_DATE, SUBOPTIMAL o se marcó
//     framebufferResized, recrea el swapchain tras la presentación.
// -----------------------------------------------------------------------------
void VulkanRenderer::drawFrame() {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    updateUniformBuffer(currentFrame);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}