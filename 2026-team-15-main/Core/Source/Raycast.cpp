#include "Raycast.hpp"

#include <cmath>

namespace
{
    // The view matrix only moves and turns the world (no scaling), so as a row-vector matrix it is [R 0; t 1].
    // Its inverse is [R^T 0; -t R^T 1]: undo the turn, then undo the move. No general matrix inverse is needed.
    Matrix4x4 RigidInverse(Matrix4x4 const& a)
    {
        Matrix4x4 inverse;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                inverse.m[i][j] = a.m[j][i];
            }
        }
        for (int j = 0; j < 3; ++j)
        {
            inverse.m[3][j] = -(a.m[3][0] * a.m[j][0] + a.m[3][1] * a.m[j][1] + a.m[3][2] * a.m[j][2]);
        }
        return inverse;
    }
}

Ray ScreenPointToRay(float pixelX, float pixelY, int viewportW, int viewportH, Matrix4x4 const& view,
                     Matrix4x4 const& projection)
{
    const float width  = static_cast<float>(viewportW > 0 ? viewportW : 1);
    const float height = static_cast<float>(viewportH > 0 ? viewportH : 1);

    // Screen pixel -> NDC (x and y in -1..+1, y UP). Screen y goes down, hence the flip.
    const float ndcX = 2.0f * pixelX / width - 1.0f;
    const float ndcY = 1.0f - 2.0f * pixelY / height;

    // Which direction, in VIEW space (camera at the origin, looking down -z), lands on that NDC point?
    // For a point at view-space depth z = -1 the projection gives  ndc = x * m00 - m20  (and y likewise with m11, m21),
    // so  x = (ndcX + m20) / m00  and  y = (ndcY + m21) / m11. The m20/m21 terms are zero for a centred lens.
    Ray ray;
    if (std::fabs(projection.m[0][0]) < 1e-12f || std::fabs(projection.m[1][1]) < 1e-12f)
    {
        return ray; // not a usable projection
    }
    const Vector3D viewDirection{(ndcX + projection.m[2][0]) / projection.m[0][0],
                                 (ndcY + projection.m[2][1]) / projection.m[1][1], -1.0f};

    // View space -> WORLD space with the camera's own pose (the inverse of the view matrix).
    const Matrix4x4 cameraToWorld = RigidInverse(view);
    ray.origin                    = cameraToWorld.TransformPoint(Vector3D{0.0f, 0.0f, 0.0f});
    ray.direction                 = cameraToWorld.TransformDirection(viewDirection).Normalize();
    return ray;
}
