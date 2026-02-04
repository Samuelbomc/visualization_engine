#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

class WindowCreator {
public:
    struct WindowDimensions {
        int width;
        int height;
    };

    WindowCreator(int width, int height, std::string title);
    ~WindowCreator();

    // Disable copying (Window is a unique resource)
    WindowCreator(const WindowCreator&) = delete;
    WindowCreator& operator=(const WindowCreator&) = delete;

    bool shouldClose() const;
    void pollEvents();

    // Getters
    GLFWwindow* getGLFWwindow() const { return window; }
    WindowDimensions getDimensions() const;

    // Helper to create the Vulkan Surface (bridge between Window and Vulkan)
    void createSurface(VkInstance instance, VkSurfaceKHR* surface);

private:
    GLFWwindow* window;
    int width;
    int height;
    std::string title;

    void initWindow();
};