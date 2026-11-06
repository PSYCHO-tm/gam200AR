#include "Mesh.hpp"

#include <cstdint>
#include <utility>
#include <vector>

Mesh::Mesh(Mesh&& other) noexcept
    : Vao{std::exchange(other.Vao, 0)},
      Vbo{std::exchange(other.Vbo, 0)},
      Ebo{std::exchange(other.Ebo, 0)},
      IndexCount{std::exchange(other.IndexCount, 0)}
{
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        Vao = std::exchange(other.Vao, 0);
        Vbo = std::exchange(other.Vbo, 0);
        Ebo = std::exchange(other.Ebo, 0);
        IndexCount = std::exchange(other.IndexCount, 0);
    }
    return *this;
}

// CPU MeshData -> GPU buffers. GL_STATIC_DRAW: uploaded once, drawn every frame, never changed.
void Mesh::Upload(MeshData const& data)
{
    static_assert(sizeof(data.Indices[0]) == sizeof(std::uint32_t), "Draw() uses GL_UNSIGNED_INT");

    Destroy(); // allow re-uploading

    // Interleave as x,y,z, nx,ny,nz per vertex.
    std::vector<float> interleaved;
    interleaved.reserve(data.Vertices.size() * 6);
    for (Vertex const& v : data.Vertices)
    {
        interleaved.insert(interleaved.end(), {v.Pos.x, v.Pos.y, v.Pos.z, v.Normal.x, v.Normal.y, v.Normal.z});
    }

    glGenVertexArrays(1, &Vao);
    glGenBuffers(1, &Vbo);
    glGenBuffers(1, &Ebo);

    glBindVertexArray(Vao); // the vao records the buffer + layout setup below
    glBindBuffer(GL_ARRAY_BUFFER, Vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)), interleaved.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.Indices.size() * sizeof(std::uint32_t)),
                 data.Indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void const*>(3 * sizeof(float)));

    glBindVertexArray(0);
    IndexCount = static_cast<GLsizei>(data.Indices.size());
}

// One draw call for the whole mesh. The VAO brings back the buffers and layout recorded in Upload.
void Mesh::Draw() const
{
    if (Vao == 0)
    {
        return; // never uploaded (or already destroyed): nothing to draw
    }
    glBindVertexArray(Vao);
    glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_INT, nullptr);
}

// Frees the three GPU objects. Safe to call twice: zero handles are skipped.
void Mesh::Destroy()
{
    if (Vao != 0)
    {
        glDeleteVertexArrays(1, &Vao);
    }
    if (Vbo != 0)
    {
        glDeleteBuffers(1, &Vbo);
    }
    if (Ebo != 0)
    {
        glDeleteBuffers(1, &Ebo);
    }
    Vao = Vbo = Ebo = 0;
    IndexCount = 0;
}