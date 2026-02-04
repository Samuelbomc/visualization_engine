#pragma once

#include "window_creator.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

class VulkanRenderer {
public:
    VulkanRenderer(WindowCreator& window);
    ~VulkanRenderer();

    // Copying a renderer disabled for now
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

private:
    WindowCreator& window; // Reference to our window wrapper

    // --- Vulkan Handles ---
    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // The GPU hardware
    VkDevice device;                                  // The logical interface
    VkQueue graphicsQueue;                            // The queue we submit work to
    VkSurfaceKHR surface;                             // The drawing surface
    VkRenderPass renderPass;
    VkSwapchainKHR swapChain;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    // --- Setup Helpers ---
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
	void createRenderPass();
    void createSwapChain();
    void createImageViews();
    void createFramebuffers();

    // --- Validation Layers ---
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    // Helper: Check if GPU supports everything we need
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily; // Queue that can display to screen

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

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

};