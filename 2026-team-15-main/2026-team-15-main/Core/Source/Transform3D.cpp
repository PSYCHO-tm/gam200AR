#include "Transform3D.hpp"

/*------------------ Transform3D Class Functions -------------------*/
Transform3D::Transform3D(Vector3D const& scale, Vector3D const& rotation, Vector3D const& position)
    : scale{ scale }, rotation{ rotation }, position{ position } {}

Matrix4x4 Transform3D::GetMatrix() const
{
    return Matrix4x4::SRT(this->scale, this->rotation, this->position);
}
/*------------------ End Of Transform3D Class Functions -------------------*/
