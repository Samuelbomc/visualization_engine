#include "window/window_creator.hpp"
#include "vulkan/vulkan_renderer.hpp"
#include "ipc/shared_geometry.hpp"
#include <iostream>

int main() {
    try {
        WindowCreator appWindow(1800, "Vulkan Menu");
        VulkanRenderer renderer(appWindow);

        SharedGeometryReader reader;
        reader.open();

        while (!appWindow.shouldClose()) {
            appWindow.pollEvents();

            static bool wasF11Down = false;
            bool isF11Down = glfwGetKey(appWindow.getGLFWwindow(), GLFW_KEY_F11) == GLFW_PRESS;
            if (isF11Down && !wasF11Down) {
                appWindow.toggleFullscreen();
            }
            wasF11Down = isF11Down;

            SharedGeometryUpdate update{};
            if (reader.tryRead(update)) {
                if (update.hasGeometry) {
                    Mesh mesh(update.geometry);
                    renderer.setMesh(mesh);
                }

                if (update.hasTransform) {
                    renderer.setTransform(update.transform);
                }
                else {
                    renderer.clearTransformOverride();
                }
            }

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