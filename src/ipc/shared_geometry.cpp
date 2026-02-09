// =============================================================================
// shared_geometry.cpp
// Implementación del lector de memoria compartida (SharedGeometryReader).
// Usa el protocolo seqlock para leer datos de forma lock-free desde la
// memoria compartida escrita por el proceso geometry_writer.
// =============================================================================

#include "ipc/shared_geometry.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <cstring>

// -----------------------------------------------------------------------------
// Destructor: cierra la conexión a la memoria compartida si está abierta.
// -----------------------------------------------------------------------------
SharedGeometryReader::~SharedGeometryReader() {
    close();
}

// -----------------------------------------------------------------------------
// open: abre la memoria compartida creada por el proceso escritor.
// Usa OpenFileMappingW con permisos de lectura y escritura (la escritura es
// necesaria para actualizar consumerSequence, que confirma al escritor que
// la geometría fue procesada).
// Si la memoria aún no existe (el escritor no se ha iniciado), retorna false.
// -----------------------------------------------------------------------------
bool SharedGeometryReader::open(const wchar_t* name) {
    if (buffer) {
        return true;
    }

    mappingHandle = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name);
    if (!mappingHandle) {
        return false;
    }

    buffer = static_cast<SharedGeometryBuffer*>(MapViewOfFile(mappingHandle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SharedGeometryBuffer)));
    if (!buffer) {
        CloseHandle(mappingHandle);
        mappingHandle = nullptr;
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// close: desmapea la vista de la memoria compartida y cierra el handle del
// objeto de mapeo de Windows. Después de llamar a close(), tryRead() retornará
// false hasta que se llame a open() de nuevo.
// -----------------------------------------------------------------------------
void SharedGeometryReader::close() {
    if (buffer) {
        UnmapViewOfFile(buffer);
        buffer = nullptr;
    }
    if (mappingHandle) {
        CloseHandle(mappingHandle);
        mappingHandle = nullptr;
    }
}

// -----------------------------------------------------------------------------
// tryRead: intenta leer una actualización consistente desde la memoria compartida.
//
// Protocolo seqlock:
//   1. Leer la secuencia (seq1). Si es igual a la última leída o es impar
//      (escritura en curso), abortar.
//   2. Copiar la cabecera completa a una variable local.
//   3. Releer la secuencia (seq2). Si seq1 != seq2 o seq2 es impar, la
//      escritura ocurrió durante la lectura → datos inconsistentes, abortar.
//   4. Validar magic y version para asegurar compatibilidad.
//
// Reconstrucción de datos:
//   - Si hasGeometry: reconstruir un GeometryData con el binding description,
//     attribute descriptions, topología, tipo de índice y los datos crudos
//     de vértices e índices copiados desde la memoria compartida.
//   - Si hasTransform: reconstruir un TransformData convirtiendo los arrays
//     de 16 floats a matrices mat4 con glm::make_mat4.
//
// Confirmación:
//   Tras una lectura exitosa, escribe la secuencia en consumerSequence con
//   un MemoryBarrier para asegurar visibilidad. El escritor usa este valor
//   para saber que la geometría fue procesada y puede dejar de reenviarla.
// -----------------------------------------------------------------------------
bool SharedGeometryReader::tryRead(SharedGeometryUpdate& outUpdate) {
    if (!buffer) {
        return false;
    }

    // Paso 1: leer la secuencia inicial
    uint32_t seq1 = buffer->header.sequence;

    // Si no ha cambiado o la escritura está en curso (impar), no hay datos nuevos
    if (seq1 == lastSequence || (seq1 & 1u) != 0) {
        return false;
    }

    // Paso 2: copiar la cabecera a memoria local para lectura atómica
    const SharedGeometryHeader header = buffer->header;

    // Paso 3: verificar que la secuencia no cambió durante la copia
    uint32_t seq2 = buffer->header.sequence;
    if (seq1 != seq2 || (seq2 & 1u) != 0) {
        return false;
    }

    // Paso 4: validar magic y versión del protocolo
    if (header.magic != SharedGeometryMagic || header.version != SharedGeometryVersion) {
        return false;
    }

    const bool hasGeometry = header.hasGeometry != 0;
    const bool hasTransform = header.hasTransform != 0;

    // Si no hay ni geometría ni transformación, no hay nada útil que procesar
    if (!hasGeometry && !hasTransform) {
        return false;
    }

    // --- Reconstrucción de la geometría ---
    if (hasGeometry) {
        // Validar que el número de atributos esté dentro del rango permitido
        if (header.attributeCount == 0 || header.attributeCount > SharedGeometryMaxAttributes) {
            return false;
        }

        // Calcular y validar el tamaño de los datos de vértices
        const size_t vertexBytes = static_cast<size_t>(header.vertexCount) * header.vertexStride;
        if (vertexBytes == 0 || vertexBytes > SharedGeometryMaxVertexBytes) {
            return false;
        }

        // Calcular y validar el tamaño de los datos de índices
        size_t indexBytes = 0;
        if (header.indexCount > 0) {
            const uint32_t indexStride = (header.indexType == VK_INDEX_TYPE_UINT32) ? sizeof(uint32_t) : sizeof(uint16_t);
            indexBytes = static_cast<size_t>(header.indexCount) * indexStride;
            if (indexBytes > SharedGeometryMaxIndexBytes) {
                return false;
            }
        }

        // Reconstruir GeometryData a partir de los datos serializados
        GeometryData geometry{};
        geometry.bindingDescription.binding = header.bindingDescription.binding;
        geometry.bindingDescription.stride = header.bindingDescription.stride;
        geometry.bindingDescription.inputRate = static_cast<VkVertexInputRate>(header.bindingDescription.inputRate);
        geometry.topology = static_cast<VkPrimitiveTopology>(header.topology);
        geometry.indexType = static_cast<VkIndexType>(header.indexType);

        // Convertir los atributos serializados (uint32_t planos) a
        // VkVertexInputAttributeDescription (con enums de Vulkan correctos)
        geometry.attributeDescriptions.clear();
        geometry.attributeDescriptions.reserve(header.attributeCount);
        for (uint32_t i = 0; i < header.attributeCount; i++) {
            VkVertexInputAttributeDescription attr{};
            attr.location = header.attributes[i].location;
            attr.binding = header.attributes[i].binding;
            attr.format = static_cast<VkFormat>(header.attributes[i].format);
            attr.offset = header.attributes[i].offset;
            geometry.attributeDescriptions.push_back(attr);
        }

        // Copiar los datos crudos de vértices desde la memoria compartida
        geometry.vertexData.resize(vertexBytes);
        std::memcpy(geometry.vertexData.data(), buffer->vertexData, vertexBytes);
        geometry.vertexCount = header.vertexCount;

        // Copiar los datos crudos de índices (si existen)
        if (indexBytes > 0) {
            geometry.indexData.resize(indexBytes);
            std::memcpy(geometry.indexData.data(), buffer->indexData, indexBytes);
            geometry.indexCount = header.indexCount;
        }

        outUpdate.geometry = std::move(geometry);
        outUpdate.hasGeometry = true;
    }
    else {
        outUpdate.hasGeometry = false;
    }

    // --- Reconstrucción de la transformación ---
    if (hasTransform) {
        // Convertir arrays de 16 floats (column-major) a matrices mat4 de GLM
        TransformData transform{};
        transform.model = glm::make_mat4(header.model);
        transform.view = glm::make_mat4(header.view);
        transform.proj = glm::make_mat4(header.proj);
        outUpdate.transform = transform;
        outUpdate.hasTransform = true;
    }
    else {
        outUpdate.hasTransform = false;
    }

    // Registrar la secuencia leída para no reprocesar los mismos datos
    outUpdate.sequence = seq1;
    lastSequence = seq1;

    // Confirmar al escritor que la geometría fue procesada.
    // MemoryBarrier asegura que la escritura de consumerSequence sea visible
    // para el otro proceso antes de que éste lea el valor.
    MemoryBarrier();
    buffer->header.consumerSequence = seq1;

    return true;
}