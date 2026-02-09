// =============================================================================
// window_creator.cpp
// Implementación de WindowCreator: inicialización de GLFW, creación de ventana
// con relación de aspecto del monitor, alternancia de pantalla completa con
// restauración de estado, y creación de superficie de Vulkan.
// =============================================================================

#include "window_creator.hpp"
#include <stdexcept>
#include <iostream>

// -----------------------------------------------------------------------------
// Constructor: almacena el ancho deseado y el título. La altura se calcula
// en initWindow() para respetar la relación de aspecto del monitor.
// -----------------------------------------------------------------------------
WindowCreator::WindowCreator(int w, std::string t)
    : width{ w }, height{ 0 }, title{ std::move(t) }, window{ nullptr }, aspectWidth{ 0 }, aspectHeight{ 0 } {
    initWindow();
}

// -----------------------------------------------------------------------------
// Destructor: destruye la ventana y finaliza GLFW, liberando todos los
// recursos del sistema de ventanas.
// -----------------------------------------------------------------------------
WindowCreator::~WindowCreator() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

// -----------------------------------------------------------------------------
// initWindow: configura y crea la ventana GLFW.
//
// Pasos:
//   1. Inicializa GLFW.
//   2. Indica que no se usará OpenGL (Vulkan gestiona su propio contexto).
//   3. Habilita el redimensionamiento de la ventana.
//   4. Consulta el monitor principal para obtener su resolución nativa y
//      calcular la relación de aspecto (ej: 16:9, 21:9).
//   5. Calcula la altura de la ventana a partir del ancho dado y la
//      relación de aspecto del monitor: height = width * (monH / monW).
//   6. Crea la ventana y guarda su posición/tamaño inicial para poder
//      restaurarlos al salir de pantalla completa.
//   7. Fija la relación de aspecto en GLFW para que el usuario no pueda
//      distorsionar la ventana al redimensionarla.
// -----------------------------------------------------------------------------
void WindowCreator::initWindow() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW!");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    aspectWidth = mode->width;
    aspectHeight = mode->height;

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

// -----------------------------------------------------------------------------
// shouldClose: consulta si el usuario ha solicitado cerrar la ventana
// (pulsando el botón X o mediante Alt+F4).
// -----------------------------------------------------------------------------
bool WindowCreator::shouldClose() const {
    return glfwWindowShouldClose(window);
}

// -----------------------------------------------------------------------------
// pollEvents: procesa todos los eventos pendientes de GLFW (input de teclado,
// ratón, eventos de redimensionamiento, foco, etc.). Debe llamarse cada frame
// para mantener la ventana responsiva.
// -----------------------------------------------------------------------------
void WindowCreator::pollEvents() {
    glfwPollEvents();
}

// -----------------------------------------------------------------------------
// getDimensions: devuelve las dimensiones actuales almacenadas en el objeto.
// Estas se actualizan al cambiar entre modo ventana y pantalla completa.
// -----------------------------------------------------------------------------
WindowCreator::WindowDimensions WindowCreator::getDimensions() const {
    return { width, height };
}

// -----------------------------------------------------------------------------
// toggleFullscreen: alterna entre modo ventana y pantalla completa.
//
// Al entrar en pantalla completa:
//   1. Guarda la posición y tamaño actuales de la ventana.
//   2. Elimina la restricción de relación de aspecto (GLFW la requiere
//      para cambiar a fullscreen sin conflictos).
//   3. Establece la ventana en el monitor principal a resolución nativa.
//
// Al salir de pantalla completa:
//   1. Restaura la ventana a la posición y tamaño guardados.
//   2. Reestablece la restricción de relación de aspecto.
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// createSurface: crea una superficie de Vulkan vinculada a esta ventana GLFW.
// GLFW abstrae las diferencias entre plataformas (Win32, X11, Wayland) para
// crear la superficie con la extensión correcta (VK_KHR_win32_surface, etc.).
// La superficie es necesaria para que el swapchain de Vulkan pueda presentar
// imágenes renderizadas en la ventana.
// -----------------------------------------------------------------------------
void WindowCreator::createSurface(VkInstance instance, VkSurfaceKHR* surface) {
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}