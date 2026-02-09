// =============================================================================
// vulkan_renderer_descriptors.cpp
// Gestión de descriptor sets (vinculación de uniform buffers al shader),
// creación de uniform buffers con mapeo persistente VMA, y actualización
// de las matrices de transformación cada frame.
// =============================================================================

#include "vulkan_renderer.hpp"
#include <stdexcept>
#include <chrono>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

// -----------------------------------------------------------------------------
// createDescriptorSetLayout: define la estructura de los descriptor sets que
// el pipeline espera recibir. En este caso, un solo binding (0) de tipo
// UNIFORM_BUFFER accesible desde el VERTEX_BIT (vertex shader).
// El layout es un "contrato" entre el pipeline y los datos que se le pasan.
// -----------------------------------------------------------------------------
void VulkanRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

// -----------------------------------------------------------------------------
// createUniformBuffers: crea un uniform buffer por cada frame en vuelo en
// memoria HOST_VISIBLE|HOST_COHERENT. VMA los mapea persistentemente, lo que
// permite escribir las matrices cada frame con un simple memcpy sin necesidad
// de llamar a vkMapMemory/vkUnmapMemory.
// Tener un buffer por frame evita que la CPU sobreescriba datos que la GPU
// todavía está leyendo del frame anterior.
// -----------------------------------------------------------------------------
void VulkanRenderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers[i],
            uniformBufferAllocations[i]);

        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(allocator, uniformBufferAllocations[i], &allocInfo);
        uniformBuffersMapped[i] = allocInfo.pMappedData;
    }
}

// -----------------------------------------------------------------------------
// createDescriptorPool: crea el pool de donde se asignan los descriptor sets.
// Reserva espacio para MAX_FRAMES_IN_FLIGHT descriptores de tipo uniform buffer.
// -----------------------------------------------------------------------------
void VulkanRenderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

// -----------------------------------------------------------------------------
// createDescriptorSets: asigna un descriptor set por frame en vuelo y los
// configura para apuntar a sus respectivos uniform buffers.
// Cada VkWriteDescriptorSet conecta el binding 0 del descriptor set con el
// buffer uniform correspondiente, indicando el rango completo del UBO.
// Esta conexión permite que el shader acceda a las matrices de transformación.
// -----------------------------------------------------------------------------
void VulkanRenderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

// -----------------------------------------------------------------------------
// setTransform / clearTransformOverride: permiten que un proceso externo
// (como el geometry writer via IPC) controle las matrices de transformación.
// Cuando hay un override activo, updateUniformBuffer usa esas matrices
// directamente en lugar de la rotación automática.
// -----------------------------------------------------------------------------
void VulkanRenderer::setTransform(const TransformData& transform) {
    transformOverride = transform;
}

void VulkanRenderer::clearTransformOverride() {
    transformOverride.reset();
}

// -----------------------------------------------------------------------------
// updateUniformBuffer: actualiza las matrices modelo/vista/proyección en el
// uniform buffer del frame actual mediante memcpy al puntero mapeado persistente.
//
// Dos modos de operación:
//   1. Con override externo: usa las matrices proporcionadas por setTransform.
//   2. Sin override (modo por defecto): aplica una rotación automática sobre
//      el eje Z basada en el tiempo transcurrido desde el inicio. La vista
//      y proyección se cachean y solo se recalculan cuando cambia la resolución
//      del swapchain, evitando cálculos trigonométricos innecesarios.
//
// La proyección en Vulkan invierte el eje Y (proj[1][1] *= -1) porque el
// sistema de coordenadas de clip space de Vulkan tiene Y hacia abajo,
// al contrario que OpenGL.
// -----------------------------------------------------------------------------
void VulkanRenderer::updateUniformBuffer(uint32_t currentImage) {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime).count();

    UniformBufferObject ubo{};

    if (transformOverride.has_value()) {
        ubo.model = transformOverride->model;
        ubo.view = transformOverride->view;
        ubo.proj = transformOverride->proj;
    }
    else {
        if (cachedExtent.width != swapChainExtent.width || cachedExtent.height != swapChainExtent.height) {
            cachedView = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, 1.0f));

            cachedProj = glm::perspective(glm::radians(45.0f),
                swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
            cachedProj[1][1] *= -1;

            cachedExtent = swapChainExtent;
        }

        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = cachedView;
        ubo.proj = cachedProj;
    }

    std::memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}