#include "Mesh.hpp"

#include <cstdint>
#include <utility>
#include <vector>

Mesh::Mesh(Mesh&& other) noexcept
    : vao{std::exchange(other.vao, 0)},
      vbo{std::exchange(other.vbo, 0)},
      ebo{std::exchange(other.ebo, 0)},
      indexCount{std::exchange(other.indexCount, 0)}
{
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        vao        = std::exchange(other.vao, 0);
        vbo        = std::exchange(other.vbo, 0);
        ebo        = std::exchange(other.ebo, 0);
        indexCount = std::exchange(other.indexCount, 0);
    }
    return *this;
}

void Mesh::Upload(MeshData const& data)
{
    static_assert(sizeof(data.indices[0]) == sizeof(std::uint32_t), "Draw() uses GL_UNSIGNED_INT");

    Destroy(); // allow re-uploading

    // Interleave as x,y,z, nx,ny,nz per vertex.
    std::vector<float> interleaved;
    interleaved.reserve(data.vertices.size() * 6);
    for (Vertex const& v : data.vertices)
    {
        interleaved.insert(interleaved.end(), {v.pos.x, v.pos.y, v.pos.z, v.normal.x, v.normal.y, v.normal.z});
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao); // the vao records the buffer + layout setup below
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)), interleaved.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
                 data.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void const*>(3 * sizeof(float)));

    glBindVertexArray(0);
    indexCount = static_cast<GLsizei>(data.indices.size());
}

void Mesh::Draw() const
{
    if (vao == 0)
    {
        return;
    }
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

void Mesh::Destroy()
{
    if (vao != 0)
    {
        glDeleteVertexArrays(1, &vao);
    }
    if (vbo != 0)
    {
        glDeleteBuffers(1, &vbo);
    }
    if (ebo != 0)
    {
        glDeleteBuffers(1, &ebo);
    }
    vao = vbo = ebo = 0;
    indexCount      = 0;
}