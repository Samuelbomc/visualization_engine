#include "ipc/shared_geometry.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <cstring>

SharedGeometryReader::~SharedGeometryReader() {
    close();
}

bool SharedGeometryReader::open(const wchar_t* name) {
    if (buffer) {
        return true;
    }

    mappingHandle = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
    if (!mappingHandle) {
        return false;
    }

    buffer = static_cast<SharedGeometryBuffer*>(MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, sizeof(SharedGeometryBuffer)));
    if (!buffer) {
        CloseHandle(mappingHandle);
        mappingHandle = nullptr;
        return false;
    }

    return true;
}

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

bool SharedGeometryReader::tryRead(SharedGeometryUpdate& outUpdate) {
    if (!buffer) {
        return false;
    }

    uint32_t seq1 = buffer->header.sequence;
    if (seq1 == lastSequence || (seq1 & 1u) != 0) {
        return false;
    }

    const SharedGeometryHeader header = buffer->header;

    uint32_t seq2 = buffer->header.sequence;
    if (seq1 != seq2 || (seq2 & 1u) != 0) {
        return false;
    }

    if (header.magic != SharedGeometryMagic || header.version != SharedGeometryVersion) {
        return false;
    }

    if (header.hasGeometry == 0) {
        return false;
    }

    if (header.attributeCount == 0 || header.attributeCount > SharedGeometryMaxAttributes) {
        return false;
    }

    const size_t vertexBytes = static_cast<size_t>(header.vertexCount) * header.vertexStride;
    if (vertexBytes == 0 || vertexBytes > SharedGeometryMaxVertexBytes) {
        return false;
    }

    size_t indexBytes = 0;
    if (header.indexCount > 0) {
        const uint32_t indexStride = (header.indexType == VK_INDEX_TYPE_UINT32) ? sizeof(uint32_t) : sizeof(uint16_t);
        indexBytes = static_cast<size_t>(header.indexCount) * indexStride;
        if (indexBytes > SharedGeometryMaxIndexBytes) {
            return false;
        }
    }

    GeometryData geometry{};
    geometry.bindingDescription.binding = header.bindingDescription.binding;
    geometry.bindingDescription.stride = header.bindingDescription.stride;
    geometry.bindingDescription.inputRate = static_cast<VkVertexInputRate>(header.bindingDescription.inputRate);
    geometry.topology = static_cast<VkPrimitiveTopology>(header.topology);
    geometry.indexType = static_cast<VkIndexType>(header.indexType);

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

    geometry.vertexData.resize(vertexBytes);
    std::memcpy(geometry.vertexData.data(), buffer->vertexData, vertexBytes);
    geometry.vertexCount = header.vertexCount;

    if (indexBytes > 0) {
        geometry.indexData.resize(indexBytes);
        std::memcpy(geometry.indexData.data(), buffer->indexData, indexBytes);
        geometry.indexCount = header.indexCount;
    }

    outUpdate.geometry = std::move(geometry);
    outUpdate.hasGeometry = true;

    if (header.hasTransform != 0) {
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

    outUpdate.sequence = seq1;
    lastSequence = seq1;
    return true;
}