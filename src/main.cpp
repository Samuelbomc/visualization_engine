#include "window/window_creator.hpp"
#include "vulkan/vulkan_renderer.hpp"
#include <iostream>

int main() {
    try {
        WindowCreator appWindow(1800, "Vulkan Menu");
        VulkanRenderer renderer(appWindow);

        // Bucle principal de renderizado
        while (!appWindow.shouldClose()) {
            appWindow.pollEvents();

            static bool wasF11Down = false;
            bool isF11Down = glfwGetKey(appWindow.getGLFWwindow(), GLFW_KEY_F11) == GLFW_PRESS;
            if (isF11Down && !wasF11Down) {
                appWindow.toggleFullscreen();
            }
            wasF11Down = isF11Down;

            renderer.drawFrame();
        }
        vkDeviceWaitIdle(renderer.getDevice());
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}