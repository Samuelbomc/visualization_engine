#include "window_creator.hpp"
#include <stdexcept>
#include <iostream>

WindowCreator::WindowCreator(int w, std::string t)
    : width{ w }, height{ 0 }, title{ std::move(t) }, window{ nullptr }, aspectWidth{ 0 }, aspectHeight{ 0 } {
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

    // Vulkan no usa contexto tradicional; gestiona su propio contexto.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Habilitar redimensionamiento
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    // Usar la relación de aspecto del monitor principal
    aspectWidth = mode->width;
    aspectHeight = mode->height;

    // Ajustar el tamaño inicial de la ventana para respetar el aspecto del monitor
    height = static_cast<int>((static_cast<long long>(width) * aspectHeight) / aspectWidth);

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("Failed to create GLFW window!");
    }

    windowedWidth = width;
    windowedHeight = height;
    glfwGetWindowPos(window, &windowedX, &windowedY);

    glfwSetWindowAspectRatio(window, aspectWidth, aspectHeight);
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

void WindowCreator::toggleFullscreen() {
    if (!window) {
        return;
    }

    if (!fullscreen) {
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        glfwSetWindowAspectRatio(window, GLFW_DONT_CARE, GLFW_DONT_CARE);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        width = mode->width;
        height = mode->height;
        fullscreen = true;
    }
    else {
        glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
        glfwSetWindowAspectRatio(window, aspectWidth, aspectHeight);
        width = windowedWidth;
        height = windowedHeight;
        fullscreen = false;
    }
}

void WindowCreator::createSurface(VkInstance instance, VkSurfaceKHR* surface) {
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}