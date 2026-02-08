#pragma once

#include "geometry/mesh.hpp"
#include "geometry/transform.hpp"
#include <windows.h>
#include <cstdint>

constexpr uint32_t SharedGeometryMagic = 0x4D4F4547; // "GEOM"
constexpr uint32_t SharedGeometryVersion = 1;
constexpr size_t SharedGeometryMaxAttributes = 8;
constexpr size_t SharedGeometryMaxVertexBytes = 4 * 1024 * 1024;
constexpr size_t SharedGeometryMaxIndexBytes = 2 * 1024 * 1024;

constexpr wchar_t SharedGeometryMappingName[] = L"Local\\VulkanSharedGeometry";

struct SharedBindingDescription {
    uint32_t binding;
    uint32_t stride;
    uint32_t inputRate;
};

struct SharedAttributeDescription {
    uint32_t location;
    uint32_t binding;
    uint32_t format;
    uint32_t offset;
};

struct SharedGeometryHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;
    uint32_t hasGeometry;
    uint32_t hasTransform;
    uint32_t vertexStride;
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t indexType;
    uint32_t topology;
    uint32_t attributeCount;
    SharedBindingDescription bindingDescription;
    SharedAttributeDescription attributes[SharedGeometryMaxAttributes];
    float model[16];
    float view[16];
    float proj[16];
};

struct SharedGeometryBuffer {
    SharedGeometryHeader header;
    uint8_t vertexData[SharedGeometryMaxVertexBytes];
    uint8_t indexData[SharedGeometryMaxIndexBytes];
};

struct SharedGeometryUpdate {
    GeometryData geometry;
    TransformData transform;
    bool hasGeometry = false;
    bool hasTransform = false;
    uint32_t sequence = 0;
};

class SharedGeometryReader {
public:
    SharedGeometryReader() = default;
    ~SharedGeometryReader();

    bool open(const wchar_t* name = SharedGeometryMappingName);
    void close();
    bool tryRead(SharedGeometryUpdate& outUpdate);

private:
    HANDLE mappingHandle = nullptr;
    SharedGeometryBuffer* buffer = nullptr;
    uint32_t lastSequence = 0;
};