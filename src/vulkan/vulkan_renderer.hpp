#pragma once

#include "window/window_creator.hpp"
#include "geometry/mesh.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include <cstdint>

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct TransformData {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

class VulkanRenderer {
public:
    VulkanRenderer(WindowCreator& window);
    ~VulkanRenderer();

    // Copia deshabilitada (el renderer gestiona recursos únicos).
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Renderiza un frame completo.
    void drawFrame();

    // Configuración en tiempo de ejecución
    void setMesh(const Mesh& newMesh);
    void setTransform(const TransformData& transform);
    void clearTransformOverride();

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
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkCommandPool commandPool;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
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
    void recreateGraphicsPipeline();
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
    bool isVertexLayoutDifferent(const GeometryData& other) const;

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

    Mesh mesh;
    std::optional<TransformData> transformOverride;

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