// Este archivo compila el código de implementación para librerías de un solo header.

// VMA: Vulkan Memory Allocator, una biblioteca de gestión de memoria para Vulkan que simplifica 
// la asignación y gestión de memoria en aplicaciones Vulkan. 
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

// STB Image: Una biblioteca de carga de imágenes que soporta varios formatos de imagen,
// como PNG, JPEG, BMP, etc. Es ampliamente utilizada para cargar texturas en aplicaciones gráficas.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// STB TrueType: Una biblioteca de renderizado de fuentes TrueType que permite cargar y renderizar
// fuentes TrueType en aplicaciones gráficas. Es útil para mostrar texto en la pantalla.
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"