// =============================================================================
// window_creator.hpp
// Declaración de la clase WindowCreator, que encapsula la creación y gestión
// de una ventana GLFW compatible con Vulkan. Maneja redimensionamiento,
// pantalla completa con restauración de posición, y preservación de la
// relación de aspecto del monitor principal.
// =============================================================================

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

class WindowCreator {
public:
    // Estructura que almacena las dimensiones actuales de la ventana en píxeles.
    struct WindowDimensions {
        int width;
        int height;
    };

    // Crea una ventana con el ancho especificado. La altura se calcula
    // automáticamente para respetar la relación de aspecto del monitor principal.
    WindowCreator(int width, std::string title);

    // Destruye la ventana GLFW y finaliza la librería.
    ~WindowCreator();

    // La ventana es un recurso único; no se permite copiar.
    WindowCreator(const WindowCreator&) = delete;
    WindowCreator& operator=(const WindowCreator&) = delete;

    // Devuelve true si el usuario ha solicitado cerrar la ventana (ej: botón X).
    bool shouldClose() const;

    // Procesa todos los eventos pendientes del sistema de ventanas (teclado,
    // ratón, redimensionamiento, etc.).
    void pollEvents();

    // Devuelve el handle nativo de GLFW para acceso directo (ej: lectura de teclas).
    GLFWwindow* getGLFWwindow() const { return window; }

    // Devuelve las dimensiones actuales de la ventana.
    WindowDimensions getDimensions() const;

    // Alterna entre modo ventana y pantalla completa. Al entrar en fullscreen,
    // guarda la posición y tamaño actuales para restaurarlos al salir.
    void toggleFullscreen();

    // Devuelve true si la ventana está actualmente en modo pantalla completa.
    bool isFullscreen() const { return fullscreen; }

    // Crea la superficie de Vulkan vinculada a esta ventana. La superficie es
    // el puente entre el sistema de ventanas y el swapchain de Vulkan.
    void createSurface(VkInstance instance, VkSurfaceKHR* surface);

private:
    // Handle nativo de la ventana GLFW.
    GLFWwindow* window;

    // Dimensiones actuales de la ventana (se actualizan al cambiar de modo).
    int width;
    int height;

    // Título mostrado en la barra de la ventana.
    std::string title;

    // Estado de pantalla completa.
    bool fullscreen = false;

    // Posición y tamaño guardados del modo ventana, para restaurar al salir
    // de pantalla completa sin perder la ubicación original.
    int windowedX = 0;
    int windowedY = 0;
    int windowedWidth = 0;
    int windowedHeight = 0;

    // Relación de aspecto del monitor principal, usada para restringir el
    // redimensionamiento de la ventana y calcular la altura inicial.
    int aspectWidth = 0;
    int aspectHeight = 0;

    // Inicializa GLFW, configura los hints de ventana (sin API gráfica propia,
    // redimensionable) y crea la ventana con la relación de aspecto correcta.
    void initWindow();
};