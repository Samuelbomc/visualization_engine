// =============================================================================
// vulkan_renderer_geometry.cpp
// Gestión de la geometría del renderer: validación de mallas, comparación de
// layouts de vértices, y actualización en caliente de los buffers de GPU.
// =============================================================================

#include "vulkan_renderer.hpp"
#include <cstring>
#include <stdexcept>

// -----------------------------------------------------------------------------
// areBindingsEqual: compara dos VkVertexInputBindingDescription campo a campo
// para determinar si tienen la misma configuración de binding, stride e input rate.
// -----------------------------------------------------------------------------
static bool areBindingsEqual(const VkVertexInputBindingDescription& a, const VkVertexInputBindingDescription& b) {
    return a.binding == b.binding && a.stride == b.stride && a.inputRate == b.inputRate;
}

// -----------------------------------------------------------------------------
// areAttributesEqual: compara dos vectores de atributos de vértices para
// verificar que tengan el mismo número de atributos con idénticos binding,
// location, format y offset. Si difieren, el pipeline debe recrearse.
// -----------------------------------------------------------------------------
static bool areAttributesEqual(const std::vector<VkVertexInputAttributeDescription>& a, const std::vector<VkVertexInputAttributeDescription>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].binding != b[i].binding ||
            a[i].location != b[i].location ||
            a[i].format != b[i].format ||
            a[i].offset != b[i].offset) {
            return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// isVertexLayoutDifferent: compara el layout de vértices de la geometría actual
// con el de una nueva geometría. Si el binding o los atributos difieren, el
// pipeline gráfico debe recrearse porque el vertex input state está horneado
// en el pipeline compilado.
// -----------------------------------------------------------------------------
bool VulkanRenderer::isVertexLayoutDifferent(const GeometryData& other) const {
    const auto& current = mesh.getData();
    if (!areBindingsEqual(current.bindingDescription, other.bindingDescription)) {
        return true;
    }
    return !areAttributesEqual(current.attributeDescriptions, other.attributeDescriptions);
}

// -----------------------------------------------------------------------------
// setMesh: reemplaza la geometría activa del renderer.
//
// Proceso:
//   1. Valida la geometría entrante: stride no nulo, datos de vértices presentes,
//      tamaño de datos coherente con stride e indexType.
//   2. Calcula vertexCount e indexCount a partir del tamaño de los datos.
//   3. Determina si el layout de vértices o la topología cambiaron
//      (lo que requiere recrear el pipeline).
//   4. Sincronización: espera a que todos los frames en vuelo terminen
//      (usando los fences individuales, no vkDeviceWaitIdle) y espera
//      las transferencias pendientes que podrían estar llenando los buffers
//      actuales.
//   5. Destruye los buffers de vértices e índices antiguos.
//   6. Almacena la nueva malla.
//   7. Recrea el pipeline gráfico si es necesario (o lo crea por primera vez).
//   8. Crea los nuevos buffers de vértices e índices y los llena
//      mediante el staging ring buffer.
// -----------------------------------------------------------------------------
void VulkanRenderer::setMesh(const Mesh& newMesh) {
    GeometryData validated = newMesh.getData();

    if (validated.bindingDescription.stride == 0) {
        throw std::runtime_error("Geometry must define a valid stride.");
    }
    if (validated.vertexData.empty()) {
        throw std::runtime_error("Geometry must have vertexData.");
    }
    if (validated.vertexData.size() % validated.bindingDescription.stride != 0) {
        throw std::runtime_error("vertexData size does not match stride.");
    }

    validated.vertexCount = static_cast<uint32_t>(validated.vertexData.size() / validated.bindingDescription.stride);

    if (!validated.indexData.empty()) {
        uint32_t indexStride = (validated.indexType == VK_INDEX_TYPE_UINT32) ? sizeof(uint32_t) : sizeof(uint16_t);
        if (validated.indexData.size() % indexStride != 0) {
            throw std::runtime_error("indexData size does not match index type.");
        }
        validated.indexCount = static_cast<uint32_t>(validated.indexData.size() / indexStride);
    }
    else {
        validated.indexCount = 0;
    }

    bool recreatePipeline = isVertexLayoutDifferent(validated) || mesh.getData().topology != validated.topology;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkWaitForFences(device, 1, &inFlightFences[i], VK_TRUE, UINT64_MAX);
    }

    waitAllTransfers();

    if (vertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAllocation);
        vertexBuffer = VK_NULL_HANDLE;
        vertexBufferAllocation = VK_NULL_HANDLE;
    }
    if (indexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, indexBuffer, indexBufferAllocation);
        indexBuffer = VK_NULL_HANDLE;
        indexBufferAllocation = VK_NULL_HANDLE;
    }

    mesh = Mesh(std::move(validated));

    if (graphicsPipeline == VK_NULL_HANDLE) {
        createGraphicsPipeline();
    }
    else if (recreatePipeline) {
        recreateGraphicsPipeline();
    }

    createVertexBuffer();
    if (mesh.getData().indexCount > 0) {
        createIndexBuffer();
    }
}