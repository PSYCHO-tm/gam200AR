#pragma once
#include <GLES3/gl3.h>

#include "Geometry.hpp"

// One mesh's vertices and indices, uploaded to the GPU once and drawn many times.
// Vertex layout: location 0 = position (vec3), location 1 = normal (vec3).
class Mesh
{
public:
    Mesh()  = default;
    ~Mesh() = default; // GPU buffers are freed by Destroy(), not here (the context may already be gone)

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void Upload(MeshData const& data);
    void Draw() const; // binds this mesh's VAO and draws it; the shader must already be in use
    void Destroy();

private:
    GLuint  vao{0};
    GLuint  vbo{0};
    GLuint  ebo{0};
    GLsizei indexCount{0};
};