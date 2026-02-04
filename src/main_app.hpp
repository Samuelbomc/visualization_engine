#pragma once

#include "window_creator.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    // Ayudante: describe la separación de los vértices en memoria.
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    // Ayudante: describe cómo se organizan los atributos (pos, color).
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // Posición (Location 0 en el shader)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // vec2
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color (Location 1 en el shader)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// Geometría base del rectángulo.
extern const std::vector<Vertex> vertices;
extern const std::vector<uint16_t> indices;

class VulkanRenderer {
public:
    VulkanRenderer(WindowCreator& window);
    ~VulkanRenderer();

    // Copia deshabilitada (el renderer gestiona recursos únicos).
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Renderiza un frame completo.
    void drawFrame();

    // Acceso al dispositivo lógico.
    VkDevice getDevice() { return device; }

private:
    WindowCreator& window; // Referencia a la ventana

    // --- Handles de Vulkan ---
    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // GPU física
    VkDevice device;                                  // Dispositivo lógico
    VkQueue graphicsQueue;                            // Cola para enviar trabajo
    VkSurfaceKHR surface;                             // Superficie de dibujo
    VkRenderPass renderPass;
    VkSwapchainKHR swapChain;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkCommandPool commandPool;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    // --- Ayudantes de configuración ---
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createRenderPass();
    void createSwapChain();
    void createImageViews();
    void createFramebuffers();
    void createGraphicsPipeline();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // --- Capas de validación ---
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    // Ayudante: comprobar si la GPU soporta todo lo necesario.
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily; // Cola que puede presentar en pantalla

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool checkValidationLayerSupport();

    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    const int MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t currentFrame = 0;

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    static std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
};