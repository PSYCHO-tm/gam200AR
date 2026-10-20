#pragma once
#include <cmath>

class Vector2D
{
    // Public Variables
    public:
    float x, y;

    public: // Constructors, Destructors and Variables
    Vector2D(float x = 0, float y = 0);
    ~Vector2D();

    // Public Functions
    public: 

    Vector2D& operator+=(Vector2D const& rhs);
    Vector2D& operator+=(float rhs);
    
    Vector2D& operator-=(Vector2D const& rhs);
    Vector2D& operator-=(float rhs);
    
    Vector2D& operator/=(Vector2D const& rhs);
    Vector2D& operator/=(float rhs);
    
    Vector2D& operator*=(Vector2D const& rhs);
    Vector2D& operator*=(float rhs);

    float Magnitude() const;
    Vector2D Normalize() const;
    float DotProduct(Vector2D const& rhs) const;
    float CrossProduct(Vector2D const& rhs) const;

};

class Vector3D
{
    // Public Variables
    public:
    float x, y, z;

    public: // Constructors, Destructors and Variables
    
    Vector3D(float x = 0, float y = 0, float z = 0) :x{x} , y{y}, z{z} {}

    ~Vector3D() {}


    public: // Member Functions
    Vector3D& operator+=(Vector3D const& rhs);
    Vector3D& operator+=(float rhs);
    
    Vector3D& operator-=(Vector3D const& rhs);
    Vector3D& operator-=(float rhs);
    
    Vector3D& operator/=(Vector3D const& rhs);
    Vector3D& operator/=(float rhs);
    
    Vector3D& operator*=(Vector3D const& rhs);
    Vector3D& operator*=(float rhs);

    float Magnitude() const;
    Vector3D Normalize() const;
    float DotProduct(Vector3D const& rhs) const;
    Vector3D CrossProduct(Vector3D const& rhs) const;
};

#include "Vector.inl"
/*---------------- Non-Member Functions ------------------*/ 

/*------------------ Vector2D Functions -------------------*/
Vector2D operator+(Vector2D const& lhs, Vector2D const& rhs);
Vector2D operator-(Vector2D const& lhs, Vector2D const& rhs);
Vector2D operator*(Vector2D const& lhs, Vector2D const& rhs);
Vector2D operator/(Vector2D const& lhs, Vector2D const& rhs);
bool operator==(Vector2D const& lhs, Vector2D const& rhs);
bool operator!=(Vector2D const& lhs, Vector2D const& rhs);
/*-------------------- End Of Vector2D --------------------*/

/*------------------ Vector3D Functions -------------------*/
Vector3D operator+(Vector3D const& lhs, Vector3D const& rhs);
Vector3D operator-(Vector3D const& lhs, Vector3D const& rhs);
Vector3D operator*(Vector3D const& lhs, Vector3D const& rhs);
Vector3D operator/(Vector3D const& lhs, Vector3D const& rhs);
bool operator==(Vector3D const& lhs, Vector3D const& rhs);
bool operator!=(Vector3D const& lhs, Vector3D const& rhs);
/*-------------------- End Of Vector3D --------------------*/

/*-------------- End Of Non-member Functions --------------*/