// =============================================================================
// vulkan_renderer.cpp
// Implementación del constructor, destructor y funciones de gestión del ciclo
// de vida del swapchain (recreación y limpieza).
// =============================================================================

#include "vulkan_renderer.hpp"

// -----------------------------------------------------------------------------
// Constructor: inicializa todos los subsistemas de Vulkan en el orden requerido
// por las dependencias entre recursos.
// Orden de creación:
//   1. Instancia y superficie (conexión con el sistema de ventanas)
//   2. GPU física y dispositivo lógico (acceso al hardware)
//   3. Asignador VMA (gestión eficiente de memoria de GPU)
//   4. Formato de depth y nivel de MSAA (consultando capacidades de la GPU)
//   5. Pipeline cache y shader modules (preparación para crear pipelines)
//   6. Swapchain, image views, render pass, attachments, framebuffers
//   7. Descriptor layout, command pool, staging ring, uniform buffers
//   8. Descriptor pool/sets, command buffers, objetos de sincronización
// -----------------------------------------------------------------------------
VulkanRenderer::VulkanRenderer(WindowCreator& w)
    : window{ w }
    , startTime{ std::chrono::high_resolution_clock::now() }
{
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createAllocator();

    depthFormat = findDepthFormat();
    msaaSamples = getMaxUsableSampleCount();

    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vkCreatePipelineCache(device, &cacheInfo, nullptr, &pipelineCache);

    loadShaderModules();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    createDescriptorSetLayout();
    createCommandPool();
    createStagingRing();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

// -----------------------------------------------------------------------------
// Destructor: libera todos los recursos en orden inverso al de creación.
// Primero espera a que la GPU termine todo el trabajo pendiente para evitar
// destruir recursos que aún estén en uso.
// -----------------------------------------------------------------------------
VulkanRenderer::~VulkanRenderer() {
    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    destroyShaderModules();

    if (pipelineCache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
    }

    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    if (indexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, indexBuffer, indexBufferAllocation);
    }
    if (vertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAllocation);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(allocator, uniformBuffers[i], uniformBufferAllocations[i]);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    destroyStagingRing();
    vkDestroyCommandPool(device, commandPool, nullptr);

    if (transferCommandPool != VK_NULL_HANDLE && transferCommandPool != commandPool) {
        vkDestroyCommandPool(device, transferCommandPool, nullptr);
    }

    destroyAllocator();
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}

// -----------------------------------------------------------------------------
// cleanupSwapChain: destruye todos los recursos cuyo tamaño o configuración
// depende de la resolución del swapchain. Se invoca antes de recrear el
// swapchain (resize, fullscreen toggle) o al destruir el renderer.
// Orden de destrucción: MSAA color → depth → framebuffers → image views →
// render pass → swapchain.
// -----------------------------------------------------------------------------
void VulkanRenderer::cleanupSwapChain() {
    if (colorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, colorImageView, nullptr);
        colorImageView = VK_NULL_HANDLE;
    }
    if (colorImage != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, colorImage, colorImageAllocation);
        colorImage = VK_NULL_HANDLE;
        colorImageAllocation = VK_NULL_HANDLE;
    }

    if (depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depthImageView, nullptr);
        depthImageView = VK_NULL_HANDLE;
    }
    if (depthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, depthImage, depthImageAllocation);
        depthImage = VK_NULL_HANDLE;
        depthImageAllocation = VK_NULL_HANDLE;
    }

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    swapChainFramebuffers.clear();

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    swapChainImageViews.clear();

    vkDestroyRenderPass(device, renderPass, nullptr);
    renderPass = VK_NULL_HANDLE;

    vkDestroySwapchainKHR(device, swapChain, nullptr);
    swapChain = VK_NULL_HANDLE;
}

// -----------------------------------------------------------------------------
// recreateSwapChain: se invoca cuando Vulkan reporta OUT_OF_DATE o SUBOPTIMAL,
// o cuando la ventana cambia de tamaño.
// Si la ventana está minimizada (dimensiones 0x0), espera con pollEvents hasta
// que recupere un tamaño válido.
// Recrea el swapchain y todos sus recursos dependientes, incluyendo el pipeline
// gráfico si ya existía (ya que depende del render pass).
// -----------------------------------------------------------------------------
void VulkanRenderer::recreateSwapChain() {
    WindowCreator::WindowDimensions dims = window.getDimensions();
    while (dims.width == 0 || dims.height == 0) {
        window.pollEvents();
        dims = window.getDimensions();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createColorResources();
    createDepthResources();
    createFramebuffers();

    if (graphicsPipeline != VK_NULL_HANDLE) {
        recreateGraphicsPipeline();
    }
}