// =============================================================================
// shared_geometry.hpp
// Sistema de comunicación inter-proceso (IPC) basado en memoria compartida
// para transferir geometría y transformaciones entre el proceso escritor
// (geometry_writer) y el proceso lector (la aplicación Vulkan).
//
// Protocolo de sincronización: Seqlock
// ────────────────────────────────────
// El escritor incrementa un número de secuencia a un valor impar antes de
// escribir, y a un valor par cuando termina. El lector verifica que la
// secuencia sea par y no haya cambiado durante la lectura, garantizando
// consistencia sin mutex (lock-free).
//
// Estructura de la memoria compartida:
//   ┌──────────────────────────────────┐
//   │ SharedGeometryHeader             │  Metadata + matrices de transformación
//   ├──────────────────────────────────┤
//   │ vertexData[4 MB]                 │  Datos crudos de vértices
//   ├──────────────────────────────────┤
//   │ indexData[2 MB]                  │  Datos crudos de índices
//   └──────────────────────────────────┘
//
// Límites:
//   - Máximo 8 atributos de vértice por malla
//   - Máximo 4 MB de datos de vértices
//   - Máximo 2 MB de datos de índices
// =============================================================================

#pragma once

#include "geometry/mesh.hpp"
#include "geometry/transform.hpp"
#include <windows.h>
#include <cstdint>

// Constante mágica "GEOM" (en little-endian) para validar que la memoria
// compartida contiene datos válidos y no basura.
constexpr uint32_t SharedGeometryMagic = 0x4D4F4547;

// Versión del protocolo. Si el escritor y el lector tienen versiones
// diferentes, el lector descarta los datos para evitar incompatibilidades.
constexpr uint32_t SharedGeometryVersion = 1;

// Límites de capacidad de la memoria compartida.
constexpr size_t SharedGeometryMaxAttributes = 8;
constexpr size_t SharedGeometryMaxVertexBytes = 4 * 1024 * 1024;
constexpr size_t SharedGeometryMaxIndexBytes = 2 * 1024 * 1024;

// Nombre del mapeo de memoria compartida en el espacio de nombres local
// de la sesión de Windows. Ambos procesos deben usar el mismo nombre.
constexpr wchar_t SharedGeometryMappingName[] = L"Local\\VulkanSharedGeometry";

// Versión serializable de VkVertexInputBindingDescription, usando uint32_t
// planos para evitar dependencias de tipos de Vulkan en la estructura compartida.
struct SharedBindingDescription {
    uint32_t binding;   // Índice del binding (normalmente 0)
    uint32_t stride;    // Bytes entre vértices consecutivos
    uint32_t inputRate; // VK_VERTEX_INPUT_RATE_VERTEX o _INSTANCE
};

// Versión serializable de VkVertexInputAttributeDescription.
struct SharedAttributeDescription {
    uint32_t location; // Ubicación en el shader (layout(location = N))
    uint32_t binding;  // Binding al que pertenece
    uint32_t format;   // VkFormat del atributo (ej: R32G32B32_SFLOAT)
    uint32_t offset;   // Offset en bytes dentro del vértice
};

// Cabecera de la memoria compartida. Contiene toda la metadata necesaria
// para reconstruir un GeometryData y un TransformData en el lado del lector.
struct SharedGeometryHeader {
    uint32_t magic;           // Debe ser SharedGeometryMagic para ser válido
    uint32_t version;         // Versión del protocolo
    uint32_t sequence;        // Número de secuencia del seqlock (par = estable)
    uint32_t hasGeometry;     // 1 si esta actualización incluye geometría nueva
    uint32_t hasTransform;    // 1 si esta actualización incluye transformación
    uint32_t vertexStride;    // Bytes por vértice
    uint32_t vertexCount;     // Número de vértices
    uint32_t indexCount;      // Número de índices (0 = sin índices)
    uint32_t indexType;       // VK_INDEX_TYPE_UINT16 o VK_INDEX_TYPE_UINT32
    uint32_t topology;        // VkPrimitiveTopology (ej: TRIANGLE_LIST)
    uint32_t attributeCount;  // Número de atributos de vértice (máx 8)

    // Descripción del layout de vértices
    SharedBindingDescription bindingDescription;
    SharedAttributeDescription attributes[SharedGeometryMaxAttributes];

    // Matrices de transformación almacenadas como arrays de 16 floats
    // en orden column-major (compatible con GLM).
    float model[16];
    float view[16];
    float proj[16];

    // Secuencia confirmada por el consumidor. El escritor la compara con
    // su propia secuencia para saber si el lector ya procesó la geometría,
    // y así dejar de reenviarla en cada frame.
    uint32_t consumerSequence;
};

// Estructura completa de la memoria compartida: cabecera + datos crudos.
struct SharedGeometryBuffer {
    SharedGeometryHeader header;
    uint8_t vertexData[SharedGeometryMaxVertexBytes]; // Vértices en formato crudo
    uint8_t indexData[SharedGeometryMaxIndexBytes];    // Índices en formato crudo
};

// Resultado de una lectura exitosa desde la memoria compartida.
// Contiene la geometría y/o transformación extraída, junto con banderas
// que indican cuáles de los dos están presentes.
struct SharedGeometryUpdate {
    GeometryData geometry;       // Datos de geometría reconstruidos
    TransformData transform;     // Matrices de transformación reconstruidas
    bool hasGeometry = false;    // true si esta actualización incluye geometría
    bool hasTransform = false;   // true si esta actualización incluye transformación
    uint32_t sequence = 0;       // Secuencia de la lectura para tracking
};

// Lector de geometría compartida. Abre la memoria compartida creada por
// el proceso escritor y lee actualizaciones de forma lock-free usando seqlock.
class SharedGeometryReader {
public:
    SharedGeometryReader() = default;

    // Cierra la conexión a la memoria compartida al destruir el lector.
    ~SharedGeometryReader();

    // Abre la memoria compartida por nombre. Devuelve true si la conexión
    // se estableció correctamente. Si el escritor aún no la ha creado,
    // devuelve false (se puede reintentar más tarde).
    bool open(const wchar_t* name = SharedGeometryMappingName);

    // Cierra la vista de la memoria compartida y el handle del mapeo.
    void close();

    // Intenta leer una actualización de la memoria compartida.
    // Devuelve true si se leyeron datos nuevos y consistentes.
    // Devuelve false si no hay datos nuevos, la escritura está en curso
    // (secuencia impar), o los datos son inválidos.
    bool tryRead(SharedGeometryUpdate& outUpdate);

private:
    HANDLE mappingHandle = nullptr;           // Handle del mapeo de memoria de Windows
    SharedGeometryBuffer* buffer = nullptr;   // Puntero al buffer mapeado
    uint32_t lastSequence = 0;                // Última secuencia leída exitosamente
};