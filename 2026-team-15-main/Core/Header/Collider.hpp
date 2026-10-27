#pragma once
#include "Matrix.hpp"
#include "Raycast.hpp"
#include "Vector.hpp"

// "This object can be hit by a ray." The shape is an axis-aligned box (AABB): just a minimum and a maximum corner,
// kept here with the functions that work on it. A collider knows nothing about meshes: whoever owns the mesh feeds
// it points with Include().
class Collider
{
public:
    Vector3D minPoint{0.0f, 0.0f, 0.0f};
    Vector3D maxPoint{0.0f, 0.0f, 0.0f};
    bool     hasBounds = false; // false until the first point is included

    // Grows the box so that it contains 'point'.
    void Include(Vector3D const& point);

    // The box around the 8 corners of this box after 'transform' (row vectors: p * transform).
    // Used to turn a local box into a world box.
    Collider Transformed(Matrix4x4 const& transform) const;

    // Slab test against a WORLD-space ray. On a hit, 'distance' is how far along the ray the box is entered
    // (0 when the ray starts inside it).
    bool IntersectsRay(Ray const& ray, float& distance) const;
};
