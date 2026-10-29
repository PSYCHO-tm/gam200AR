#include "Collider.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

void Collider::Include(Vector3D const& point)
{
    if (!hasBounds)
    {
        minPoint  = point;
        maxPoint  = point;
        hasBounds = true;
        return;
    }
    minPoint = Vector3D{std::min(minPoint.x, point.x), std::min(minPoint.y, point.y), std::min(minPoint.z, point.z)};
    maxPoint = Vector3D{std::max(maxPoint.x, point.x), std::max(maxPoint.y, point.y), std::max(maxPoint.z, point.z)};
}

Collider Collider::Transformed(Matrix4x4 const& transform) const
{
    if (!hasBounds)
    {
        return *this;
    }

    Collider result;
    for (int i = 0; i < 8; ++i)
    {
        // Bit 0 picks x, bit 1 picks y, bit 2 picks z: the min side when the bit is 0, the max side when it is 1.
        const Vector3D corner{(i & 1) ? maxPoint.x : minPoint.x, (i & 2) ? maxPoint.y : minPoint.y,
                              (i & 4) ? maxPoint.z : minPoint.z};
        result.Include(transform.TransformPoint(corner));
    }
    return result;
}

bool Collider::IntersectsRay(Raycast const& ray, float& distance) const
{
    if (!hasBounds)
    {
        return false;
    }

    const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float dir[3]    = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float low[3]    = {minPoint.x, minPoint.y, minPoint.z};
    const float high[3]   = {maxPoint.x, maxPoint.y, maxPoint.z};

    float entry = 0.0f;
    float exit  = 1e30f;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::fabs(dir[axis]) < 1e-12f)
        {
            if (origin[axis] < low[axis] || origin[axis] > high[axis])
            {
                return false; // parallel to this pair of faces and outside them
            }
            continue;
        }

        float tNear = (low[axis] - origin[axis]) / dir[axis];
        float tFar  = (high[axis] - origin[axis]) / dir[axis];
        if (tNear > tFar)
        {
            std::swap(tNear, tFar);
        }
        entry = std::max(entry, tNear);
        exit  = std::min(exit, tFar);
        if (entry > exit)
        {
            return false;
        }
    }
    distance = entry;
    return true;
}
