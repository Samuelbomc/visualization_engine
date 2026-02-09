// =============================================================================
// main.cpp
// Punto de entrada de la aplicación. Orquesta los tres subsistemas principales:
//   1. WindowCreator: ventana GLFW con soporte de pantalla completa.
//   2. VulkanRenderer: motor de renderizado Vulkan completo.
//   3. SharedGeometryReader: lectura de geometría y transformaciones desde
//      memoria compartida (IPC con el proceso geometry_writer).
//
// Bucle principal:
//   - Procesar eventos de ventana (input, redimensionamiento).
//   - Detectar la tecla F11 para alternar pantalla completa.
//   - Leer actualizaciones de geometría/transformación desde IPC.
//   - Renderizar un frame con Vulkan.
//   - Al salir del bucle, esperar a que la GPU termine antes de destruir.
// =============================================================================

#include "window/window_creator.hpp"
#include "vulkan/vulkan_renderer.hpp"
#include "ipc/shared_geometry.hpp"
#include <iostream>

int main() {
    try {
        // Crear la ventana con ancho de 1800 píxeles; la altura se ajusta
        // automáticamente a la relación de aspecto del monitor.
        WindowCreator appWindow(1800, "Vulkan Menu");

        // Inicializar el renderer de Vulkan vinculado a la ventana.
        // Esto crea toda la infraestructura: instancia, dispositivo, swapchain,
        // pipeline, buffers, sincronización, etc.
        VulkanRenderer renderer(appWindow);

        // Abrir la conexión IPC con el proceso escritor de geometría.
        // Si el proceso escritor aún no ha creado la memoria compartida,
        // open() retornará false y tryRead() simplemente no leerá nada.
        SharedGeometryReader reader;
        reader.open();

        while (!appWindow.shouldClose()) {
            // Procesar eventos del sistema de ventanas para mantener la
            // ventana responsiva (teclado, ratón, resize, cierre, etc.).
            appWindow.pollEvents();

            // Detección de flanco ascendente de F11 para alternar fullscreen.
            // Se usa una variable estática para detectar el momento exacto
            // en que la tecla pasa de no-pulsada a pulsada, evitando que
            // se alterne múltiples veces mientras se mantiene presionada.
            static bool wasF11Down = false;
            bool isF11Down = glfwGetKey(appWindow.getGLFWwindow(), GLFW_KEY_F11) == GLFW_PRESS;
            if (isF11Down && !wasF11Down) {
                appWindow.toggleFullscreen();
            }
            wasF11Down = isF11Down;

            // Intentar leer una actualización desde la memoria compartida.
            // tryRead() usa un protocolo seqlock: solo devuelve datos cuando
            // la secuencia es par (escritura completada) y ha cambiado desde
            // la última lectura.
            SharedGeometryUpdate update{};
            if (reader.tryRead(update)) {
                // Si hay geometría nueva, crear un objeto Mesh y pasarlo al
                // renderer, que validará los datos, recreará los buffers de
                // GPU y, si el layout de vértices cambió, recreará el pipeline.
                if (update.hasGeometry) {
                    Mesh mesh(update.geometry);
                    renderer.setMesh(mesh);
                }

                // Si hay transformación, aplicarla como override; si no,
                // volver a la rotación automática por defecto.
                if (update.hasTransform) {
                    renderer.setTransform(update.transform);
                }
                else {
                    renderer.clearTransformOverride();
                }
            }

            // Ejecutar el ciclo completo de un frame: adquirir imagen del
            // swapchain, grabar comandos, enviar a la GPU y presentar.
            renderer.drawFrame();
        }

        // Esperar a que la GPU termine todo el trabajo pendiente antes de
        // destruir los recursos de Vulkan en los destructores.
        vkDeviceWaitIdle(renderer.getDevice());
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}