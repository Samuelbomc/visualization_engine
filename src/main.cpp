#include "window_creator.hpp"
#include "main_app.hpp"
#include <iostream>

int main() {
    try {
        WindowCreator appWindow(800, 600, "Vulkan Menu");
        VulkanRenderer renderer(appWindow);

        while (!appWindow.shouldClose()) {
            appWindow.pollEvents();
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