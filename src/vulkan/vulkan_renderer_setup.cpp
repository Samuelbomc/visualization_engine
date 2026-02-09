// =============================================================================
// vulkan_renderer_setup.cpp
// Funciones de configuración inicial: instancia Vulkan, selección de GPU,
// dispositivo lógico, asignador VMA, render pass, recursos de profundidad/MSAA,
// creación de imágenes/vistas, y funciones auxiliares de formatos.
// =============================================================================

#include "vulkan_renderer.hpp"
#include <stdexcept>
#include <set>
#include <string>
#include <cstring>

// -----------------------------------------------------------------------------
// createInstance: crea la instancia de Vulkan, que es el punto de entrada a la API.
// Registra las extensiones requeridas por GLFW para crear superficies de ventana
// y, en builds de depuración, habilita las capas de validación de Khronos para
// detectar errores de uso de la API en tiempo de ejecución.
// -----------------------------------------------------------------------------
void VulkanRenderer::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Menu";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance!");
    }
}

// -----------------------------------------------------------------------------
// createSurface: crea la superficie de dibujo de Vulkan asociada a la ventana
// GLFW. Esta superficie es necesaria para que el swapchain pueda presentar
// imágenes renderizadas en la ventana.
// -----------------------------------------------------------------------------
void VulkanRenderer::createSurface() {
    window.createSurface(instance, &surface);
}

// -----------------------------------------------------------------------------
// checkDeviceExtensionSupport: verifica que la GPU soporte todas las extensiones
// de dispositivo requeridas (actualmente solo VK_KHR_swapchain).
// Enumera las extensiones disponibles y elimina del conjunto de requeridas las
// que encuentra; si el conjunto queda vacío, la GPU cumple los requisitos.
// -----------------------------------------------------------------------------
bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

// -----------------------------------------------------------------------------
// pickPhysicalDevice: enumera las GPUs del sistema y selecciona la primera que:
//   1. Tenga familias de colas de gráficos y presentación.
//   2. Soporte VK_KHR_swapchain.
// En un sistema con múltiples GPUs, se podría extender con un sistema de
// puntuación para preferir GPUs discretas.
// -----------------------------------------------------------------------------
void VulkanRenderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        QueueFamilyIndices indices = findQueueFamilies(device);
        bool extensionsSupported = checkDeviceExtensionSupport(device);
        if (indices.isComplete() && extensionsSupported) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

// -----------------------------------------------------------------------------
// createLogicalDevice: crea el dispositivo lógico a partir de la GPU física.
// Configura las colas necesarias:
//   - Gráficos: para comandos de dibujo y render passes.
//   - Presentación: para entregar imágenes al swapchain (puede coincidir con gráficos).
//   - Transferencia dedicada: para copias DMA en paralelo (si la GPU la tiene).
// Habilita sampleRateShading para el sombreado por muestra de MSAA.
// -----------------------------------------------------------------------------
void VulkanRenderer::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    if (indices.transferFamily.has_value()) {
        uniqueQueueFamilies.insert(indices.transferFamily.value());
    }

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.sampleRateShading = VK_TRUE;
    createInfo.pEnabledFeatures = &deviceFeatures;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

    if (indices.transferFamily.has_value()) {
        vkGetDeviceQueue(device, indices.transferFamily.value(), 0, &transferQueue);
    }
    else {
        transferQueue = graphicsQueue;
    }
}

// -----------------------------------------------------------------------------
// createAllocator: inicializa Vulkan Memory Allocator (VMA).
// VMA gestiona pools de memoria internos, sub-asignando de bloques grandes
// para minimizar las llamadas a vkAllocateMemory (limitadas a ~4096 por driver).
// Esto mejora el rendimiento y evita fragmentación de memoria de GPU.
// -----------------------------------------------------------------------------
void VulkanRenderer::createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;

    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator!");
    }
}

// -----------------------------------------------------------------------------
// destroyAllocator: destruye el asignador VMA. Debe llamarse después de
// haber destruido todos los buffers e imágenes que fueron asignados con él,
// y antes de destruir el dispositivo lógico.
// -----------------------------------------------------------------------------
void VulkanRenderer::destroyAllocator() {
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }
}

// -----------------------------------------------------------------------------
// findQueueFamilies: recorre las familias de colas de la GPU y busca:
//   - graphicsFamily: familia con VK_QUEUE_GRAPHICS_BIT.
//   - presentFamily: familia que soporte presentación en la superficie.
//   - transferFamily: familia con VK_QUEUE_TRANSFER_BIT pero SIN GRAPHICS_BIT,
//     para transferencias DMA en paralelo con el renderizado.
// -----------------------------------------------------------------------------
VulkanRenderer::QueueFamilyIndices VulkanRenderer::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.transferFamily = i;
        }

        if (indices.isComplete() && indices.transferFamily.has_value()) break;
        i++;
    }

    return indices;
}

// -----------------------------------------------------------------------------
// checkValidationLayerSupport: verifica que las capas de validación solicitadas
// estén disponibles en la instalación del driver/SDK de Vulkan.
// -----------------------------------------------------------------------------
bool VulkanRenderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// createDepthResources: crea la imagen de profundidad con el formato ya resuelto
// (depthFormat) y el nivel de MSAA correspondiente, más su image view.
// El depth buffer permite que el hardware descarte fragmentos que están detrás
// de otros ya dibujados (depth test con VK_COMPARE_OP_LESS).
// -----------------------------------------------------------------------------
void VulkanRenderer::createDepthResources() {
    createImage(
        swapChainExtent.width,
        swapChainExtent.height,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        depthImage,
        depthImageAllocation,
        msaaSamples);

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (hasStencilComponent(depthFormat)) {
        aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    depthImageView = createImageView(depthImage, depthFormat, aspectFlags);
}

// -----------------------------------------------------------------------------
// createColorResources: crea la imagen de color multisampled para MSAA.
// Solo se crea si msaaSamples > 1. Esta imagen recibe el renderizado con
// N muestras por píxel; el render pass la resuelve automáticamente a la
// imagen 1x del swapchain.
// Se marca como TRANSIENT_ATTACHMENT porque su contenido no necesita persistir
// entre frames, lo que permite al driver usar memoria lazily allocated.
// -----------------------------------------------------------------------------
void VulkanRenderer::createColorResources() {
    if (msaaSamples == VK_SAMPLE_COUNT_1_BIT) {
        return;
    }

    createImage(
        swapChainExtent.width,
        swapChainExtent.height,
        swapChainImageFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        colorImage,
        colorImageAllocation,
        msaaSamples);

    colorImageView = createImageView(colorImage, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

// -----------------------------------------------------------------------------
// createRenderPass: define la estructura de los attachments y las dependencias
// del render pass, adaptándose dinámicamente al nivel de MSAA.
//
// Sin MSAA (1x):
//   Attachment 0: color (swapchain, 1 muestra) → PRESENT_SRC_KHR
//   Attachment 1: depth (1 muestra) → DEPTH_STENCIL_ATTACHMENT_OPTIMAL
//
// Con MSAA (Nx):
//   Attachment 0: color MSAA (N muestras) → no se almacena (DONT_CARE)
//   Attachment 1: depth MSAA (N muestras) → no se almacena
//   Attachment 2: resolve (swapchain, 1 muestra) → PRESENT_SRC_KHR
//   El subpass resuelve automáticamente el color MSAA al attachment de resolve.
//
// La dependencia de subpass asegura que los attachments de color y depth estén
// listos antes de que el subpass comience a escribir en ellos.
// -----------------------------------------------------------------------------
void VulkanRenderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = (msaaSamples == VK_SAMPLE_COUNT_1_BIT)
        ? VK_ATTACHMENT_STORE_OP_STORE
        : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = (msaaSamples == VK_SAMPLE_COUNT_1_BIT)
        ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkAttachmentDescription colorResolveAttachment{};
    VkAttachmentReference colorResolveRef{};

    std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };

    if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
        colorResolveAttachment.format = swapChainImageFormat;
        colorResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        colorResolveRef.attachment = 2;
        colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        subpass.pResolveAttachments = &colorResolveRef;
        attachments.push_back(colorResolveAttachment);
    }

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

// -----------------------------------------------------------------------------
// findSupportedFormat: busca el primer formato de la lista de candidatos que
// soporte las features requeridas en el modo de tiling especificado.
// Se usa para encontrar el mejor formato de depth disponible.
// -----------------------------------------------------------------------------
VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format!");
}

// -----------------------------------------------------------------------------
// findDepthFormat: busca el mejor formato de depth soportado por la GPU.
// Prioriza D32_SFLOAT (32 bits float, máxima precisión), luego
// D32_SFLOAT_S8_UINT (con stencil) y finalmente D24_UNORM_S8_UINT (24 bits).
// -----------------------------------------------------------------------------
VkFormat VulkanRenderer::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

// -----------------------------------------------------------------------------
// hasStencilComponent: devuelve true si el formato incluye componente de stencil,
// necesario para configurar correctamente la máscara de aspecto de la image view.
// -----------------------------------------------------------------------------
bool VulkanRenderer::hasStencilComponent(VkFormat format) const {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

// -----------------------------------------------------------------------------
// createImageView: crea una vista 2D sobre una imagen, especificando el formato
// y la máscara de aspecto (COLOR, DEPTH, STENCIL). Las vistas son necesarias
// para vincular imágenes a framebuffers y descriptor sets.
// -----------------------------------------------------------------------------
VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view!");
    }
    return imageView;
}

// -----------------------------------------------------------------------------
// createImage: crea una imagen 2D usando VMA para la asignación de memoria.
// VMA elige automáticamente el tipo de memoria adecuado según las propiedades
// requeridas (DEVICE_LOCAL, HOST_VISIBLE, etc.).
// Para attachments transitorios (ej: MSAA color), usa asignación dedicada y
// solicita memoria lazily allocated cuando esté disponible.
// -----------------------------------------------------------------------------
void VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
    VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
    VkImage& image, VmaAllocation& allocation,
    VkSampleCountFlagBits numSamples) {

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.requiredFlags = properties;

    if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) {
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }

    if (vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &image, &allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image with VMA!");
    }
}

// -----------------------------------------------------------------------------
// getMaxUsableSampleCount: consulta los límites de la GPU para determinar el
// nivel máximo de MSAA soportado tanto para color como para depth.
// Se limita a 4x como compromiso entre calidad visual y rendimiento.
// -----------------------------------------------------------------------------
VkSampleCountFlagBits VulkanRenderer::getMaxUsableSampleCount() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts
        & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}