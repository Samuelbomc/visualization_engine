// =============================================================================
// vulkan_renderer_swapchain.cpp
// Creación y configuración del swapchain, sus image views y framebuffers.
// El swapchain es la cadena de imágenes de presentación que Vulkan usa para
// implementar doble o triple buffering con el sistema de ventanas.
// =============================================================================

#include "vulkan_renderer.hpp"
#include <stdexcept>
#include <algorithm>
#include <limits>

// -----------------------------------------------------------------------------
// createSwapChain: crea el swapchain seleccionando el mejor formato de superficie,
// modo de presentación y resolución disponibles.
// Solicita minImageCount + 1 imágenes para permitir triple buffering cuando
// el modo MAILBOX está disponible.
// Si las familias de gráficos y presentación son diferentes, configura acceso
// concurrente; si son iguales, usa acceso exclusivo (más eficiente).
// -----------------------------------------------------------------------------
void VulkanRenderer::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

// -----------------------------------------------------------------------------
// chooseSwapSurfaceFormat: selecciona B8G8R8A8_SRGB con espacio de color
// SRGB_NONLINEAR si está disponible, ya que proporciona percepción de color
// correcta para la mayoría de monitores. Si no, usa el primer formato disponible.
// -----------------------------------------------------------------------------
VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

// -----------------------------------------------------------------------------
// chooseSwapPresentMode: selecciona MAILBOX (triple buffering sin tearing ni
// latencia de V-Sync) si está disponible. Si no, usa FIFO (V-Sync garantizado
// por la especificación de Vulkan).
// -----------------------------------------------------------------------------
VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

// -----------------------------------------------------------------------------
// chooseSwapExtent: determina la resolución del swapchain.
// Si el compositor ya especifica una extensión fija, la usa directamente.
// Si no (currentExtent.width == UINT32_MAX, ventana redimensionable), toma
// las dimensiones de la ventana GLFW y las restringe al rango soportado.
// -----------------------------------------------------------------------------
VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    else {
        WindowCreator::WindowDimensions dims = window.getDimensions();

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(dims.width),
            static_cast<uint32_t>(dims.height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

// -----------------------------------------------------------------------------
// querySwapChainSupport: consulta las capacidades, formatos y modos de
// presentación soportados por la superficie para la GPU especificada.
// Esta información se usa para configurar el swapchain de forma óptima.
// -----------------------------------------------------------------------------
VulkanRenderer::SwapChainSupportDetails VulkanRenderer::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

// -----------------------------------------------------------------------------
// createImageViews: crea una image view para cada imagen del swapchain.
// Estas vistas permiten usar las imágenes como attachments de color en los
// framebuffers del render pass.
// -----------------------------------------------------------------------------
void VulkanRenderer::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

// -----------------------------------------------------------------------------
// createFramebuffers: crea un framebuffer por cada imagen del swapchain,
// vinculando los attachments según el modo de MSAA:
//
// Con MSAA: [color MSAA, depth MSAA, resolve (imagen del swapchain)]
//   El render pass resuelve el color MSAA en el attachment de resolve.
//
// Sin MSAA: [color (imagen del swapchain), depth]
//   El render pass escribe directamente en la imagen del swapchain.
// -----------------------------------------------------------------------------
void VulkanRenderer::createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::vector<VkImageView> attachments;

        if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
            attachments = { colorImageView, depthImageView, swapChainImageViews[i] };
        }
        else {
            attachments = { swapChainImageViews[i], depthImageView };
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}