// =============================================================================
// mesh.cpp
// Implementación de la clase Mesh: constructores y setters que almacenan
// los datos de geometría por copia o por movimiento.
// =============================================================================

#include "geometry/mesh.hpp"

// Construye la malla copiando todos los datos de geometría (vértices, índices,
// descripciones de layout). Útil cuando el origen necesita conservar sus datos.
Mesh::Mesh(const GeometryData& data) : data{ data } {}

// Construye la malla moviendo los datos de geometría. Transfiere la propiedad
// de los vectores internos (vertexData, indexData, attributeDescriptions) sin
// copiarlos, lo que es óptimo para datos temporales.
Mesh::Mesh(GeometryData&& data) : data{ std::move(data) } {}

// Reemplaza los datos de geometría con una copia del origen.
void Mesh::setData(const GeometryData& data) {
    this->data = data;
}

// Reemplaza los datos de geometría moviendo desde el origen.
void Mesh::setData(GeometryData&& data) {
    this->data = std::move(data);
}