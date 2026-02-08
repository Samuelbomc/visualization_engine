#include "ipc/shared_geometry.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <windows.h>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <iostream>

static void writeSharedGeometry(SharedGeometryBuffer* buffer,
    const std::vector<Vertex>& vertices,
    const std::vector<uint16_t>& indices,
    const TransformData& transform,
    bool writeGeometry) {
    const uint32_t seq = buffer->header.sequence;
    buffer->header.sequence = seq + 1;
    MemoryBarrier();

    buffer->header.magic = SharedGeometryMagic;
    buffer->header.version = SharedGeometryVersion;
    buffer->header.hasGeometry = writeGeometry ? 1u : 0u;
    buffer->header.hasTransform = 1;

    if (writeGeometry) {
        buffer->header.vertexStride = sizeof(Vertex);
        buffer->header.vertexCount = static_cast<uint32_t>(vertices.size());
        buffer->header.indexCount = static_cast<uint32_t>(indices.size());
        buffer->header.indexType = VK_INDEX_TYPE_UINT16;
        buffer->header.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        buffer->header.attributeCount = 2;
        buffer->header.bindingDescription.binding = 0;
        buffer->header.bindingDescription.stride = sizeof(Vertex);
        buffer->header.bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        buffer->header.attributes[0].location = 0;
        buffer->header.attributes[0].binding = 0;
        buffer->header.attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        buffer->header.attributes[0].offset = offsetof(Vertex, pos);

        buffer->header.attributes[1].location = 1;
        buffer->header.attributes[1].binding = 0;
        buffer->header.attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        buffer->header.attributes[1].offset = offsetof(Vertex, color);

        std::memcpy(buffer->vertexData, vertices.data(), vertices.size() * sizeof(Vertex));
        std::memcpy(buffer->indexData, indices.data(), indices.size() * sizeof(uint16_t));
    }
    else {
        buffer->header.vertexStride = 0;
        buffer->header.vertexCount = 0;
        buffer->header.indexCount = 0;
        buffer->header.attributeCount = 0;
    }

    std::memcpy(buffer->header.model, glm::value_ptr(transform.model), sizeof(float) * 16);
    std::memcpy(buffer->header.view, glm::value_ptr(transform.view), sizeof(float) * 16);
    std::memcpy(buffer->header.proj, glm::value_ptr(transform.proj), sizeof(float) * 16);

    MemoryBarrier();
    buffer->header.sequence = seq + 2;
}

int main() {
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        sizeof(SharedGeometryBuffer), SharedGeometryMappingName);
    if (!mapping) {
        std::cerr << "Failed to create shared memory.\n";
        return 1;
    }

    auto* buffer = static_cast<SharedGeometryBuffer*>(
        MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(SharedGeometryBuffer)));

    if (!buffer) {
        CloseHandle(mapping);
        std::cerr << "Failed to map shared memory.\n";
        return 1;
    }

    std::memset(buffer, 0, sizeof(SharedGeometryBuffer));

    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.2f, 0.2f, 0.2f}}
    };

    std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 4, 7, 7, 3, 0,
        1, 5, 6, 6, 2, 1,
        3, 2, 6, 6, 7, 3,
        0, 1, 5, 5, 4, 0
    };

    auto start = std::chrono::high_resolution_clock::now();
    auto last = start;
    float angle = 0.0f;
    const float rotationSpeed = glm::radians(45.0f); // 45 deg/sec

    bool geometryAcked = false;
    uint32_t lastGeometrySequence = 0;

    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        angle += rotationSpeed * dt;
        if (angle > glm::two_pi<float>()) {
            angle -= glm::two_pi<float>();
        }

        TransformData transform{};
        transform.model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));
        transform.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f));
        transform.proj = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 10.0f);
        transform.proj[1][1] *= -1;

        if (!geometryAcked) {
            writeSharedGeometry(buffer, vertices, indices, transform, true);
            lastGeometrySequence = buffer->header.sequence;
            MemoryBarrier();
            geometryAcked = buffer->header.consumerSequence == lastGeometrySequence;
        }
        else {
            writeSharedGeometry(buffer, vertices, indices, transform, false);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}