#include "Transform2D.hpp"

/*------------------ Transform2D Class Functions -------------------*/
Transform2D::Transform2D(Vector2D const& scale, float rotation, Vector2D const& position)
    : scale{ scale }, rotation{ rotation }, position{ position } {}

Matrix3x3 Transform2D::GetMatrix() const
{
    return Matrix3x3::SRT(this->scale, this->rotation, this->position);
}
/*------------------ End Of Transform2D Class Functions -------------------*/
