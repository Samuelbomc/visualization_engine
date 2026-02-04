#include "window_creator.hpp"
#include <stdexcept>
#include <iostream>

WindowCreator::WindowCreator(int w, int h, std::string t)
    : width{ w }, height{ h }, title{ std::move(t) }, window{ nullptr } {
    initWindow();
}

WindowCreator::~WindowCreator() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

void WindowCreator::initWindow() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW!");
    }

    // Vulkan is context-less; it manages the context itself.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Enable resizing
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("Failed to create GLFW window!");
    }
}

bool WindowCreator::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void WindowCreator::pollEvents() {
    glfwPollEvents();
}

WindowCreator::WindowDimensions WindowCreator::getDimensions() const {
    return { width, height };
}

void WindowCreator::createSurface(VkInstance instance, VkSurfaceKHR* surface) {
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}