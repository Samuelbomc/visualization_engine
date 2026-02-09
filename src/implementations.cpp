// =============================================================================
// implementations.cpp
// Archivo dedicado a compilar las implementaciones de librerías de un solo
// header (single-header libraries). Cada librería requiere que su macro
// *_IMPLEMENTATION se defina en exactamente una unidad de compilación para
// generar el código de implementación; en las demás solo se incluyen las
// declaraciones.
//
// Este patrón centraliza todas las implementaciones en un solo archivo para:
//   - Evitar definiciones duplicadas si se incluyen en múltiples .cpp.
//   - Aislar los tiempos de compilación largos de estas librerías.
//   - Facilitar la localización de dependencias de terceros.
// =============================================================================

// Vulkan Memory Allocator (VMA): gestiona la memoria de GPU en pools internos,
// sub-asignando de bloques grandes para evitar el límite de ~4096 asignaciones
// individuales que imponen los drivers de Vulkan. Reemplaza las llamadas
// manuales a vkAllocateMemory con una API de alto nivel (vmaCreateBuffer,
// vmaCreateImage, etc.).
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

// STB Image: decodificador de imágenes multi-formato (PNG, JPEG, BMP, TGA, HDR)
// en un solo header. Carga imágenes desde archivo o memoria y devuelve un
// array de píxeles RGBA listo para subir como textura a la GPU.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// STB TrueType: rasterizador de fuentes TrueType/OpenType en un solo header.
// Permite cargar archivos .ttf/.otf y generar bitmaps de glifos para renderizar
// texto en aplicaciones gráficas sin dependencias externas de tipografía.
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"