#include "Vector.hpp"

/*------------------ Vector2D Class Functions -------------------*/
Vector2D::Vector2D(float x, float y) : x{ x }, y{ y } {}

Vector2D::~Vector2D() {}

float Vector2D::Magnitude() const
{        
    return std::sqrt((this->x * this->x) + (this->y * this->y));
}

Vector2D Vector2D::Normalize() const
{
    float mag = this->Magnitude();
    
    if(mag > 0.0f)
    {
        return Vector2D(this->x / mag, this->y / mag);
    }

    return *this;
}

float Vector2D::DotProduct(Vector2D const& rhs) const
{
    return ((this->x * rhs.x) + (this->y * rhs.y));
}

float Vector2D::CrossProduct(Vector2D const& rhs) const
{
    return ((this->x * rhs.y) - (this->y * rhs.x));
}

/*------------------ End Of Vector2D Class Functions -------------------*/

/*------------------ Vector3D Class Functions -------------------*/
float Vector3D::Magnitude() const
{
    return std::sqrt((this->x * this->x)+(this->y * this->y)+(this->z * this->z));
}

Vector3D Vector3D::Normalize() const
{
    float mag = this->Magnitude();

    if(mag > 0.0f)
    {
        return Vector3D(this->x / mag, this->y / mag, this->z / mag);
    }

    return *this;
}

float Vector3D::DotProduct(Vector3D const& rhs) const
{
    return ((this->x * rhs.x) + (this->y * rhs.y) + (this->z * rhs.z));
}

Vector3D Vector3D::CrossProduct(Vector3D const& rhs)  const //Returns a Normal vector
{
    float x_Norm = ((this->y * rhs.z) - (this->z * rhs.y));
    float y_Norm = ((this->z * rhs.x) - (this->x * rhs.z));
    float z_Norm = ((this->x * rhs.y) - (this->y * rhs.x));

    return Vector3D(x_Norm, y_Norm, z_Norm);
}