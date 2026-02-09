// =============================================================================
// vulkan_renderer_buffers.cpp
// Gestión de buffers de memoria: creación con VMA, comandos de copia,
// staging ring buffer circular para transferencias asíncronas a VRAM,
// y creación de buffers de vértices e índices.
// =============================================================================

#include "vulkan_renderer.hpp"
#include <stdexcept>
#include <cstring>

// -----------------------------------------------------------------------------
// createBuffer: crea un buffer Vulkan con VMA, que sub-asigna de pools internos.
// Para buffers host-visible (uniform buffers, staging), solicita mapeo persistente
// y acceso de escritura secuencial, lo que evita llamadas explícitas a
// vkMapMemory/vkUnmapMemory.
// Para buffers device-local (vértices, índices), VMA elige automáticamente
// la memoria VRAM más rápida disponible.
// -----------------------------------------------------------------------------
void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    VkBuffer& buffer, VmaAllocation& allocation) {

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.requiredFlags = properties;

    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    if (vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer with VMA!");
    }
}

// -----------------------------------------------------------------------------
// beginSingleTimeCommands: asigna un command buffer temporal del pool de gráficos,
// marcado con ONE_TIME_SUBMIT para indicar al driver que se usará una sola vez.
// Se usa para operaciones puntuales como copias de buffer.
// -----------------------------------------------------------------------------
VkCommandBuffer VulkanRenderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffer!");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin command buffer!");
    }

    return commandBuffer;
}

// -----------------------------------------------------------------------------
// endSingleTimeCommands: finaliza, envía y espera un command buffer temporal.
// Usa un fence dedicado en lugar de vkQueueWaitIdle para evitar bloquear
// todo el trabajo pendiente en la cola: solo espera a que este comando
// específico termine.
// -----------------------------------------------------------------------------
void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create fence for single-time commands!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        vkDestroyFence(device, fence, nullptr);
        throw std::runtime_error("failed to submit command buffer!");
    }

    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// -----------------------------------------------------------------------------
// copyBuffer: copia el contenido de un buffer a otro usando un command buffer
// temporal. Es una operación síncrona (bloquea hasta que la copia termina).
// Se usa como alternativa simple cuando no se necesita el staging ring.
// -----------------------------------------------------------------------------
void VulkanRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

// =============================================================================
// Staging Ring Buffer
// Buffer circular de 8 MB en memoria HOST_VISIBLE|HOST_COHERENT que sirve como
// área de transferencia temporal para subir datos de CPU a GPU.
//
// Funcionamiento:
//   1. Los datos se escriben secuencialmente en el ring con stagingRingWrite().
//   2. Se graba un comando de copia al buffer destino DEVICE_LOCAL.
//   3. Se envía a la cola de transferencia con un fence individual.
//   4. El fence y la región del ring se registran como PendingTransfer.
//   5. Antes de escribir nuevos datos, se verifica que no se solapen con
//      transferencias pendientes; si hay solapamiento, se espera al fence.
//   6. Las transferencias completadas se limpian periódicamente.
// =============================================================================

// -----------------------------------------------------------------------------
// createStagingRing: crea el buffer circular en memoria host-visible y obtiene
// el puntero mapeado persistente proporcionado por VMA.
// -----------------------------------------------------------------------------
void VulkanRenderer::createStagingRing() {
    createBuffer(STAGING_RING_SIZE,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingRingBuffer,
        stagingRingAllocation);

    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(allocator, stagingRingAllocation, &allocInfo);
    stagingRingMapped = allocInfo.pMappedData;
    stagingRingOffset = 0;
}

// -----------------------------------------------------------------------------
// destroyStagingRing: espera todas las transferencias pendientes y destruye
// el buffer. VMA gestiona el desmapeo automáticamente.
// -----------------------------------------------------------------------------
void VulkanRenderer::destroyStagingRing() {
    waitAllTransfers();

    stagingRingMapped = nullptr;

    if (stagingRingBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, stagingRingBuffer, stagingRingAllocation);
        stagingRingBuffer = VK_NULL_HANDLE;
        stagingRingAllocation = VK_NULL_HANDLE;
    }
}

// -----------------------------------------------------------------------------
// flushCompletedTransfers: recorre las transferencias pendientes y libera las
// que ya han completado (su fence está señalizado), recuperando el fence,
// el command buffer y marcando la región del ring como disponible.
// Se llama al inicio de cada transferToDeviceLocal para reciclar recursos.
// -----------------------------------------------------------------------------
void VulkanRenderer::flushCompletedTransfers() {
    auto it = pendingTransfers.begin();
    while (it != pendingTransfers.end()) {
        VkResult result = vkGetFenceStatus(device, it->fence);
        if (result == VK_SUCCESS) {
            vkDestroyFence(device, it->fence, nullptr);
            vkFreeCommandBuffers(device, transferCommandPool, 1, &it->commandBuffer);
            it = pendingTransfers.erase(it);
        }
        else {
            ++it;
        }
    }
}

// -----------------------------------------------------------------------------
// waitAllTransfers: bloquea hasta que todas las transferencias pendientes
// hayan completado. Se usa antes de destruir buffers que podrían estar
// siendo copiados, o antes de destruir el staging ring.
// -----------------------------------------------------------------------------
void VulkanRenderer::waitAllTransfers() {
    for (auto& pt : pendingTransfers) {
        vkWaitForFences(device, 1, &pt.fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(device, pt.fence, nullptr);
        vkFreeCommandBuffers(device, transferCommandPool, 1, &pt.commandBuffer);
    }
    pendingTransfers.clear();
}

// -----------------------------------------------------------------------------
// stagingRingWrite: escribe datos en la posición actual del ring buffer.
// Si la escritura se desborda del final del ring, reinicia al inicio (wrap).
// Antes de escribir, verifica que ninguna transferencia pendiente esté usando
// la región que se va a sobreescribir; si hay solapamiento, espera a que
// esas transferencias terminen.
// Devuelve el offset dentro del ring donde se escribieron los datos.
// -----------------------------------------------------------------------------
VkDeviceSize VulkanRenderer::stagingRingWrite(const void* data, VkDeviceSize size) {
    if (size > STAGING_RING_SIZE) {
        throw std::runtime_error("Transfer size exceeds staging ring buffer capacity!");
    }

    VkDeviceSize writeOffset = stagingRingOffset;
    bool wrapped = false;

    if (writeOffset + size > STAGING_RING_SIZE) {
        writeOffset = 0;
        wrapped = true;
    }

    VkDeviceSize writeEnd = writeOffset + size;

    auto it = pendingTransfers.begin();
    while (it != pendingTransfers.end()) {
        VkDeviceSize tStart = it->ringOffset;
        VkDeviceSize tEnd = tStart + it->ringSize;

        bool overlaps = (writeOffset < tEnd) && (tStart < writeEnd);

        if (wrapped && !overlaps) {
            overlaps = (tStart >= stagingRingOffset);
        }

        if (overlaps) {
            vkWaitForFences(device, 1, &it->fence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(device, it->fence, nullptr);
            vkFreeCommandBuffers(device, transferCommandPool, 1, &it->commandBuffer);
            it = pendingTransfers.erase(it);
        }
        else {
            ++it;
        }
    }

    std::memcpy(static_cast<uint8_t*>(stagingRingMapped) + writeOffset, data, static_cast<size_t>(size));
    stagingRingOffset = writeEnd;

    return writeOffset;
}

// -----------------------------------------------------------------------------
// transferToDeviceLocal: sube datos de CPU a un buffer DEVICE_LOCAL (VRAM).
//   1. Limpia transferencias completadas para reciclar regiones del ring.
//   2. Escribe los datos en el staging ring (con protección de solapamiento).
//   3. Graba un comando de copia (vkCmdCopyBuffer) en un command buffer temporal.
//   4. Envía el comando a la cola de transferencia con un fence individual.
//   5. Registra la transferencia como pendiente con su región del ring.
// La copia se ejecuta de forma asíncrona en la cola de transferencia (que puede
// ser una cola DMA dedicada, ejecutándose en paralelo con el renderizado).
// -----------------------------------------------------------------------------
void VulkanRenderer::transferToDeviceLocal(VkBuffer dstBuffer, const void* data, VkDeviceSize size) {
    flushCompletedTransfers();

    VkDeviceSize srcOffset = stagingRingWrite(data, size);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = transferCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuf;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmdBuf) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate transfer command buffer!");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmdBuf, stagingRingBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(cmdBuf);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create transfer fence!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;

    if (vkQueueSubmit(transferQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit transfer command!");
    }

    pendingTransfers.push_back({ fence, cmdBuf, srcOffset, size });
}

// -----------------------------------------------------------------------------
// createVertexBuffer: crea un buffer de vértices en memoria DEVICE_LOCAL y lo
// llena con los datos de la malla actual mediante transferToDeviceLocal.
// El flag TRANSFER_DST_BIT indica que será destino de una operación de copia.
// -----------------------------------------------------------------------------
void VulkanRenderer::createVertexBuffer() {
    const auto& geometry = mesh.getData();
    VkDeviceSize bufferSize = static_cast<VkDeviceSize>(geometry.vertexData.size());
    if (bufferSize == 0) {
        return;
    }

    createBuffer(bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer,
        vertexBufferAllocation);

    transferToDeviceLocal(vertexBuffer, geometry.vertexData.data(), bufferSize);
}

// -----------------------------------------------------------------------------
// createIndexBuffer: crea un buffer de índices en memoria DEVICE_LOCAL y lo
// llena con los datos de índices de la malla actual. Permite reutilizar
// vértices compartidos entre triángulos, reduciendo el ancho de banda.
// -----------------------------------------------------------------------------
void VulkanRenderer::createIndexBuffer() {
    const auto& geometry = mesh.getData();
    if (geometry.indexCount == 0) {
        return;
    }

    VkDeviceSize bufferSize = static_cast<VkDeviceSize>(geometry.indexData.size());
    if (bufferSize == 0) {
        return;
    }

    createBuffer(bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer,
        indexBufferAllocation);

    transferToDeviceLocal(indexBuffer, geometry.indexData.data(), bufferSize);
}