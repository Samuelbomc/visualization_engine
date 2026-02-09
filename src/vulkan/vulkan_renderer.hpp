#pragma once

// =============================================================================
// vulkan_renderer.hpp
// Declaración de la clase VulkanRenderer, que encapsula todo el ciclo de vida
// de una aplicación de renderizado con Vulkan: instancia, dispositivo, swapchain,
// pipeline gráfico, buffers de geometría, sincronización y presentación.
// La memoria de GPU se gestiona mediante Vulkan Memory Allocator (VMA), que
// sub-asigna desde pools internos para evitar el límite de ~4096 asignaciones
// individuales que imponen los drivers.
// =============================================================================

#include "window/window_creator.hpp"
#include "geometry/mesh.hpp"
#include "geometry/transform.hpp"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <array>
#include <optional>
#include <glm/glm.hpp>
#include <cstdint>
#include <chrono>

// Estructura que contiene las tres matrices de transformación (modelo, vista y
// proyección) que se envían al vertex shader a través de un Uniform Buffer Object.
// El alignas(16) garantiza que cada mat4 cumpla con el alineamiento std140 de GLSL.
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class VulkanRenderer {
public:
    // Construye el renderer, inicializando todos los recursos de Vulkan en el
    // orden correcto: instancia → superficie → dispositivo → swapchain → pipeline.
    VulkanRenderer(WindowCreator& window);

    // Destruye todos los recursos de Vulkan en orden inverso al de creación,
    // asegurando que la GPU haya terminado todo el trabajo pendiente primero.
    ~VulkanRenderer();

    // El renderer gestiona handles únicos de Vulkan; no se permite la copia.
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Ejecuta el ciclo completo de un frame: adquiere imagen del swapchain,
    // graba comandos, envía a la cola de gráficos y presenta en pantalla.
    void drawFrame();

    // Reemplaza la geometría activa del renderer. Valida la malla, recrea los
    // buffers de vértices/índices y, si el layout de vértices cambió, recrea
    // el pipeline gráfico completo.
    void setMesh(const Mesh& newMesh);

    // Establece una transformación externa (modelo/vista/proyección) que
    // sobreescribe la rotación automática por defecto.
    void setTransform(const TransformData& transform);

    // Limpia la transformación externa, volviendo a la rotación automática.
    void clearTransformOverride();

    // Devuelve el handle del dispositivo lógico para uso externo (por ejemplo,
    // para esperar con vkDeviceWaitIdle antes de cerrar la aplicación).
    VkDevice getDevice() { return device; }

private:
    // Número máximo de frames que pueden estar en vuelo simultáneamente.
    // Con 2, mientras la GPU renderiza el frame N, la CPU puede preparar el N+1.
    // Es constexpr para permitir su uso como tamaño de std::array en tiempo de compilación.
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // Referencia a la ventana GLFW que posee la superficie de dibujo.
    WindowCreator& window;

    // ==========================================================================
    // Handles fundamentales de Vulkan
    // ==========================================================================

    // Instancia de Vulkan: punto de entrada a la API, conecta la app con el loader.
    VkInstance instance;

    // GPU física seleccionada (la primera que cumple con los requisitos de colas
    // y extensiones como VK_KHR_swapchain).
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    // Dispositivo lógico: interfaz de la aplicación con la GPU física.
    // Todos los recursos (buffers, imágenes, pipelines) se crean a través de él.
    VkDevice device;

    // Asignador de memoria VMA: gestiona pools internos de memoria de GPU,
    // sub-asignando para evitar las llamadas individuales a vkAllocateMemory.
    VmaAllocator allocator = VK_NULL_HANDLE;

    // Cola de gráficos: recibe command buffers con comandos de dibujo y compute.
    VkQueue graphicsQueue;

    // Cola de presentación: entrega las imágenes renderizadas al swapchain.
    // Puede ser la misma cola que graphicsQueue si la familia lo soporta.
    VkQueue presentQueue;

    // Cola de transferencia dedicada: se usa para copias DMA de staging a VRAM.
    // Si la GPU tiene una familia de transferencia exclusiva, las copias se
    // ejecutan en paralelo con el renderizado. Si no, se usa graphicsQueue.
    VkQueue transferQueue = VK_NULL_HANDLE;

    // Superficie de dibujo: puente entre la ventana GLFW y Vulkan.
    VkSurfaceKHR surface;

    // Render pass: define la estructura de los attachments (color, depth, resolve)
    // y las dependencias de subpass para sincronización automática.
    VkRenderPass renderPass;

    // Swapchain: cadena de imágenes de presentación propiedad del sistema de
    // ventanas. El renderer adquiere una, dibuja sobre ella y la presenta.
    VkSwapchainKHR swapChain;

    // Formato de color de las imágenes del swapchain (ej: B8G8R8A8_SRGB).
    VkFormat swapChainImageFormat;

    // Resolución en píxeles de las imágenes del swapchain.
    VkExtent2D swapChainExtent;

    // Layout del pipeline: describe los descriptor sets y push constants
    // disponibles durante la ejecución del pipeline.
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    // Pipeline gráfico: objeto compilado que contiene todas las etapas
    // (vertex, fragment), estados fijos (rasterización, blending, depth)
    // y la configuración del vertex input de la geometría actual.
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    // Caché de pipeline: acelera la creación de pipelines al reutilizar
    // resultados de compilación previos durante la misma ejecución.
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;

    // Command pool de gráficos: pool de donde se asignan los command buffers
    // de renderizado. Vinculado a la familia de colas de gráficos.
    VkCommandPool commandPool;

    // Command pool de transferencia: pool separado para la familia de transferencia
    // dedicada. Si no hay familia dedicada, apunta al mismo pool de gráficos.
    VkCommandPool transferCommandPool = VK_NULL_HANDLE;

    // ==========================================================================
    // Buffers de geometría (vértices e índices)
    // ==========================================================================

    // Buffer de vértices en memoria DEVICE_LOCAL (VRAM). Se llena mediante una
    // copia desde el staging ring buffer para máximo rendimiento de lectura.
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexBufferAllocation = VK_NULL_HANDLE;

    // Buffer de índices en DEVICE_LOCAL. Permite reutilizar vértices compartidos
    // entre triángulos, reduciendo el consumo de memoria y ancho de banda.
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation indexBufferAllocation = VK_NULL_HANDLE;

    // ==========================================================================
    // Descriptores (UBO binding)
    // ==========================================================================

    // Layout del descriptor set: describe que el binding 0 es un uniform buffer
    // accesible desde el vertex shader.
    VkDescriptorSetLayout descriptorSetLayout;

    // Pool de descriptores: reserva espacio para MAX_FRAMES_IN_FLIGHT
    // descriptor sets de tipo uniform buffer.
    VkDescriptorPool descriptorPool;

    // Descriptor sets asignados, uno por frame en vuelo, cada uno apuntando
    // a su propio uniform buffer para evitar conflictos de escritura.
    std::vector<VkDescriptorSet> descriptorSets;

    // ==========================================================================
    // Recursos de profundidad (depth buffer)
    // ==========================================================================

    // Imagen de profundidad: almacena la distancia de cada fragmento a la cámara.
    // Permite el depth test para que los objetos más cercanos oculten a los lejanos.
    VkImage depthImage = VK_NULL_HANDLE;
    VmaAllocation depthImageAllocation = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    // Formato del depth buffer, determinado una sola vez al inicio según las
    // capacidades de la GPU (ej: D32_SFLOAT, D32_SFLOAT_S8_UINT, D24_UNORM_S8_UINT).
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    // ==========================================================================
    // Recursos de MSAA (multisampling antialiasing)
    // ==========================================================================

    // Nivel de muestras por píxel, determinado al inicio (máximo 4x).
    // Reduce el aliasing en los bordes de la geometría.
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    // Imagen de color multisampled: el render pass dibuja aquí con N muestras.
    // Se resuelve automáticamente a la imagen 1x del swapchain.
    VkImage colorImage = VK_NULL_HANDLE;
    VmaAllocation colorImageAllocation = VK_NULL_HANDLE;
    VkImageView colorImageView = VK_NULL_HANDLE;

    // ==========================================================================
    // Módulos de shader cacheados
    // ==========================================================================

    // Los módulos SPIR-V del vertex y fragment shader se cargan una sola vez
    // desde disco y se reutilizan en cada recreación del pipeline, evitando
    // lecturas de archivo repetidas.
    VkShaderModule cachedVertShaderModule = VK_NULL_HANDLE;
    VkShaderModule cachedFragShaderModule = VK_NULL_HANDLE;

    // ==========================================================================
    // Staging Ring Buffer
    // Buffer circular de 8 MB en memoria HOST_VISIBLE para subir datos a la GPU.
    // Los datos se escriben secuencialmente y se copian a buffers DEVICE_LOCAL
    // mediante comandos de transferencia asíncronos con fences individuales.
    // Antes de sobreescribir una región, se verifica que las transferencias
    // pendientes que la usan hayan completado.
    // ==========================================================================

    static constexpr VkDeviceSize STAGING_RING_SIZE = 8 * 1024 * 1024;
    VkBuffer stagingRingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingRingAllocation = VK_NULL_HANDLE;
    void* stagingRingMapped = nullptr;  // Puntero persistente al mapeo del buffer
    VkDeviceSize stagingRingOffset = 0; // Posición de escritura actual en el anillo

    // Representa una transferencia DMA en curso con su fence de sincronización
    // y la región del ring buffer que ocupa (para detección de solapamiento).
    struct PendingTransfer {
        VkFence fence;              // Fence que se señaliza cuando la copia termina
        VkCommandBuffer commandBuffer; // Command buffer de un solo uso para la copia
        VkDeviceSize ringOffset;    // Inicio de la región usada en el staging ring
        VkDeviceSize ringSize;      // Tamaño de la región usada
    };
    std::vector<PendingTransfer> pendingTransfers;

    // Crea el staging ring buffer con VMA y obtiene el puntero mapeado persistente.
    void createStagingRing();

    // Espera todas las transferencias pendientes y destruye el staging ring buffer.
    void destroyStagingRing();

    // Escribe datos en la posición actual del ring. Si al avanzar se solaparía con
    // transferencias pendientes, espera a que éstas terminen antes de sobreescribir.
    // Devuelve el offset dentro del ring donde se escribieron los datos.
    VkDeviceSize stagingRingWrite(const void* data, VkDeviceSize size);

    // Sube datos a un buffer DEVICE_LOCAL: escribe en el staging ring, graba un
    // comando de copia, lo envía a la cola de transferencia con un fence y lo
    // registra como transferencia pendiente.
    void transferToDeviceLocal(VkBuffer dstBuffer, const void* data, VkDeviceSize size);

    // Recorre las transferencias pendientes y libera las que ya han completado
    // (según su fence), recuperando sus regiones del ring para reutilización.
    void flushCompletedTransfers();

    // Bloquea hasta que todas las transferencias pendientes hayan completado.
    // Se usa antes de destruir buffers que podrían estar siendo copiados.
    void waitAllTransfers();

    // ==========================================================================
    // Funciones de inicialización (se llaman en orden desde el constructor)
    // ==========================================================================

    // Crea la instancia de Vulkan con las extensiones requeridas por GLFW
    // y, opcionalmente, las capas de validación para depuración.
    void createInstance();

    // Crea la superficie de dibujo vinculada a la ventana GLFW.
    void createSurface();

    // Enumera las GPUs disponibles y selecciona la primera que soporte las
    // familias de colas necesarias y la extensión VK_KHR_swapchain.
    void pickPhysicalDevice();

    // Crea el dispositivo lógico con las colas de gráficos, presentación y
    // transferencia (si hay familia dedicada), y habilita sample rate shading.
    void createLogicalDevice();

    // Inicializa el asignador VMA, que gestiona la memoria de GPU en pools.
    void createAllocator();

    // Destruye el asignador VMA y todas sus asignaciones pendientes.
    void destroyAllocator();

    // Crea el render pass que describe los attachments de color (con MSAA),
    // profundidad y resolución, así como las dependencias de subpass.
    void createRenderPass();

    // Crea el swapchain con el formato, modo de presentación y resolución óptimos.
    void createSwapChain();

    // Crea image views para cada imagen del swapchain.
    void createImageViews();

    // Crea los framebuffers vinculando las image views del swapchain con los
    // attachments de profundidad y, si MSAA está activo, el attachment de color.
    void createFramebuffers();

    // Crea el pipeline gráfico completo: shaders, vertex input, rasterización,
    // multisampling, depth test, color blending y estados dinámicos.
    void createGraphicsPipeline();

    // Destruye el pipeline y layout actuales y los recrea. Se invoca cuando
    // cambia el layout de vértices o la topología de la malla.
    void recreateGraphicsPipeline();

    // Crea los command pools para las colas de gráficos y transferencia.
    void createCommandPool();

    // Asigna MAX_FRAMES_IN_FLIGHT command buffers del pool de gráficos.
    void createCommandBuffers();

    // Crea los semáforos (imagen disponible, render terminado) y fences
    // (sincronización CPU-GPU) para cada frame en vuelo.
    void createSyncObjects();

    // Crea la imagen y vista de profundidad con el nivel de MSAA correspondiente.
    void createDepthResources();

    // Crea la imagen de color multisampled para MSAA (solo si msaaSamples > 1).
    void createColorResources();

    // ==========================================================================
    // Funciones auxiliares de creación de recursos
    // ==========================================================================

    // Crea una imagen 2D con VMA. Configura asignación dedicada para attachments
    // transitorios (MSAA color) y permite solicitar memoria lazily allocated.
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VmaAllocation& allocation,
        VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);

    // Crea una image view 2D con el formato y máscara de aspecto especificados.
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    // Busca el mejor formato de profundidad soportado por la GPU.
    VkFormat findDepthFormat();

    // Busca entre los formatos candidatos el primero que soporte las features
    // requeridas en el tiling especificado (linear u optimal).
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    // Devuelve true si el formato incluye componente de stencil.
    bool hasStencilComponent(VkFormat format) const;

    // Crea un buffer con VMA. Para buffers host-visible, solicita mapeo
    // persistente y acceso de escritura secuencial automáticamente.
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VmaAllocation& allocation);

    // Crea el buffer de vértices DEVICE_LOCAL y lo llena con transferToDeviceLocal.
    void createVertexBuffer();

    // Crea el buffer de índices DEVICE_LOCAL y lo llena con transferToDeviceLocal.
    void createIndexBuffer();

    // Crea un uniform buffer HOST_VISIBLE por cada frame en vuelo, con mapeo
    // persistente para actualizaciones directas con memcpy cada frame.
    void createUniformBuffers();

    // Define el layout del descriptor set: un binding de uniform buffer en el
    // vertex shader (binding = 0).
    void createDescriptorSetLayout();

    // Crea el pool de descriptores con capacidad para MAX_FRAMES_IN_FLIGHT sets.
    void createDescriptorPool();

    // Asigna y configura los descriptor sets, vinculando cada uno a su
    // uniform buffer correspondiente.
    void createDescriptorSets();

    // Actualiza el uniform buffer del frame actual con las matrices de
    // transformación. Si hay un override externo, lo usa; si no, aplica
    // una rotación automática con cámara y proyección cacheadas.
    void updateUniformBuffer(uint32_t currentImage);

    // Compara el layout de vértices (binding description y attribute descriptions)
    // de la geometría actual con una nueva, para decidir si hay que recrear el pipeline.
    bool isVertexLayoutDifferent(const GeometryData& other) const;

    // Asigna un command buffer temporal del pool de gráficos, marcado como
    // ONE_TIME_SUBMIT para operaciones puntuales.
    VkCommandBuffer beginSingleTimeCommands();

    // Finaliza, envía y espera (con fence) un command buffer temporal, luego lo libera.
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Copia el contenido de un buffer a otro mediante un command buffer temporal.
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // Lee los archivos SPIR-V desde disco y crea los módulos de shader cacheados.
    void loadShaderModules();

    // Destruye los módulos de shader cacheados.
    void destroyShaderModules();

    // Consulta los límites de la GPU y devuelve el máximo nivel de MSAA soportado,
    // limitado a 4x como compromiso entre calidad visual y rendimiento.
    VkSampleCountFlagBits getMaxUsableSampleCount();

    // ==========================================================================
    // Recreación del swapchain
    // Se invoca cuando la ventana cambia de tamaño, entra/sale de pantalla
    // completa, o Vulkan reporta VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR.
    // ==========================================================================

    // Destruye los recursos dependientes del swapchain (framebuffers, image views,
    // render pass, depth/color images, swapchain) y los recrea con la nueva resolución.
    void recreateSwapChain();

    // Libera todos los recursos que dependen de la resolución del swapchain.
    void cleanupSwapChain();

    // Bandera que indica si el framebuffer necesita recreación (ej: resize de ventana).
    bool framebufferResized = false;

    // ==========================================================================
    // Capas de validación (solo en builds de depuración)
    // ==========================================================================

    // Lista de capas de validación de Khronos para detectar errores de uso
    // de la API durante el desarrollo.
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    // ==========================================================================
    // Familias de colas
    // ==========================================================================

    // Almacena los índices de las familias de colas encontradas en la GPU.
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;  // Familia con capacidad de gráficos
        std::optional<uint32_t> presentFamily;   // Familia que puede presentar a la superficie
        std::optional<uint32_t> transferFamily;  // Familia exclusiva de transferencia (DMA)

        // El renderer requiere al menos una familia de gráficos y una de presentación.
        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    // Enumera las familias de colas de la GPU y busca las de gráficos,
    // presentación y transferencia dedicada.
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    // Verifica que la GPU soporte VK_KHR_swapchain.
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    // Verifica que las capas de validación solicitadas estén instaladas.
    bool checkValidationLayerSupport();

    // ==========================================================================
    // Recursos del swapchain y sincronización
    // ==========================================================================

    // Imágenes internas del swapchain (propiedad del sistema de ventanas).
    std::vector<VkImage> swapChainImages;

    // Vistas sobre las imágenes del swapchain para usarlas como attachments.
    std::vector<VkImageView> swapChainImageViews;

    // Framebuffers que combinan las vistas de color, depth y resolve (MSAA).
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Command buffers pre-asignados, uno por frame en vuelo.
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers;

    // Semáforos que señalizan que una imagen del swapchain está disponible
    // para renderizar (uno por frame en vuelo).
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores;

    // Semáforos que señalizan que el renderizado terminó y la imagen está
    // lista para presentar (uno por frame en vuelo).
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores;

    // Fences para sincronización CPU-GPU: la CPU espera a que el fence del
    // frame actual se señalice antes de reutilizar sus recursos.
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences;

    // ==========================================================================
    // Uniform buffers (uno por frame en vuelo)
    // ==========================================================================

    // Buffers de uniforms en memoria host-visible, uno por frame, para que la
    // CPU pueda escribir las matrices mientras la GPU lee las del frame anterior.
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
    std::array<VmaAllocation, MAX_FRAMES_IN_FLIGHT> uniformBufferAllocations;

    // Punteros persistentes al mapeo de cada uniform buffer (VMA los mantiene
    // mapeados durante toda la vida del buffer).
    std::array<void*, MAX_FRAMES_IN_FLIGHT> uniformBuffersMapped{};

    // Índice del frame actual dentro del ciclo de frames en vuelo (0 o 1).
    uint32_t currentFrame = 0;

    // ==========================================================================
    // Estado de la malla y transformaciones
    // ==========================================================================

    // Malla actualmente cargada en los buffers de vértices/índices.
    Mesh mesh;

    // Transformación externa opcional. Si tiene valor, sobreescribe la rotación
    // automática por defecto en updateUniformBuffer.
    std::optional<TransformData> transformOverride;

    // Marca temporal de inicio para la animación de rotación automática.
    std::chrono::high_resolution_clock::time_point startTime;

    // Caché de la resolución del swapchain para recalcular la proyección solo
    // cuando cambia el tamaño de la ventana.
    VkExtent2D cachedExtent{};

    // Matrices de vista y proyección cacheadas, recalculadas solo cuando
    // cambia la resolución del swapchain.
    glm::mat4 cachedView{ 1.0f };
    glm::mat4 cachedProj{ 1.0f };

    // ==========================================================================
    // Soporte del swapchain
    // ==========================================================================

    // Encapsula las capacidades, formatos y modos de presentación soportados
    // por la superficie para la GPU seleccionada.
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    // Consulta las capacidades del swapchain para la GPU y superficie actuales.
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    // Selecciona el formato de superficie preferido (B8G8R8A8_SRGB si está disponible).
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    // Selecciona el modo de presentación preferido (MAILBOX para triple buffering,
    // con fallback a FIFO que es V-Sync garantizado).
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    // Determina la resolución del swapchain según las capacidades de la superficie
    // y las dimensiones actuales de la ventana.
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // Lee un archivo binario completo (usado para cargar SPIR-V compilado).
    static std::vector<char> readFile(const std::string& filename);

    // Crea un módulo de shader a partir de bytecode SPIR-V.
    VkShaderModule createShaderModule(const std::vector<char>& code);

    // Graba los comandos de renderizado de un frame en el command buffer:
    // begin render pass, bind pipeline, set viewport/scissor, bind vertex/index
    // buffers, bind descriptor sets, draw, end render pass.
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
};