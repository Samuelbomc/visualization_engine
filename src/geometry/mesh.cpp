#include "geometry/mesh.hpp"

Mesh::Mesh(const GeometryData& data) : data{ data } {}

Mesh::Mesh(GeometryData&& data) : data{ std::move(data) } {}

void Mesh::setData(const GeometryData& data) {
    this->data = data;
}

void Mesh::setData(GeometryData&& data) {
    this->data = std::move(data);
}