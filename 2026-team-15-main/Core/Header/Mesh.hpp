#pragma once
#include "GlApi.hpp"

#include "Geometry.hpp"

// One mesh's vertices and indices, uploaded to the GPU once and drawn many times.
// Vertex layout: location 0 = position (vec3), location 1 = normal (vec3). (Vertex.uv is not uploaded yet: the lit
// shader has no textures. Text and textured models will need it as location 2.)
// GPU side: a VAO (remembers the layout), a VBO (the vertices, interleaved) and an EBO (the triangle indices).
class Mesh
{
public:
    Mesh()  = default;
    ~Mesh() = default; // GPU buffers are freed by Destroy(), not here (the context may already be gone)

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;            // moving hands the GPU buffers over; 'other' is left empty
    Mesh& operator=(Mesh&& other) noexcept;

    void Upload(MeshData const& data); // copies the CPU mesh to the GPU (replacing any earlier upload)
    void Draw() const; // binds this mesh's VAO and draws it; the shader must already be in use
    void Destroy(); // frees the GPU buffers (needs a live context)

private:
    GLuint  vao{0};
    GLuint  vbo{0};
    GLuint  ebo{0};
    GLsizei indexCount{0}; // how many indices Draw() sends: 3 per triangle
};