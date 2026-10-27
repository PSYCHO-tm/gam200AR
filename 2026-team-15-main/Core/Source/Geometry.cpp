#include "Geometry.hpp"

MeshData MakeQuad()
{
    const Vector3D normal{0.0f, 0.0f, 1.0f};

    MeshData quad;
    quad.vertices = {
        Vertex{Vector3D{0.0f, 0.0f, 0.0f}, normal, Vector2D{0.0f, 0.0f}}, // 0: corner (0,0)
        Vertex{Vector3D{1.0f, 0.0f, 0.0f}, normal, Vector2D{1.0f, 0.0f}}, // 1: corner (1,0)
        Vertex{Vector3D{1.0f, 1.0f, 0.0f}, normal, Vector2D{1.0f, 1.0f}}, // 2: corner (1,1)
        Vertex{Vector3D{0.0f, 1.0f, 0.0f}, normal, Vector2D{0.0f, 1.0f}}, // 3: corner (0,1)
    };
    quad.indices = {0, 1, 2,   // first triangle
                    0, 2, 3};  // second triangle: together they cover the square
    return quad;
}
