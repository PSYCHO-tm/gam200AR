/*------------------ End Of Member Functions -------------------*/

/*------------------ Vector2D Class Functions -------------------*/
inline Vector2D& Vector2D::operator+=(Vector2D const& rhs)
{
    this->x += rhs.x;
    this->y += rhs.y;
    return *this;
}

inline Vector2D& Vector2D::operator+=(float rhs)
{
    this->x += rhs;
    this->y += rhs;
    return *this;
}

inline Vector2D& Vector2D::operator-=(Vector2D const& rhs)
{
    this->x -= rhs.x;
    this->y -= rhs.y;
    return *this;
}

inline Vector2D& Vector2D::operator-=(float rhs)
{
    this->x -= rhs;
    this->y -= rhs;
    return *this;
}

inline Vector2D& Vector2D::operator/=(Vector2D const& rhs)
{
    this->x /= rhs.x;
    this->y /= rhs.y;
    return *this;
}

inline Vector2D& Vector2D::operator/=(float rhs)
{
    this->x /= rhs;
    this->y /= rhs;
    return *this;
}

inline Vector2D& Vector2D::operator*=(Vector2D const& rhs)
{
    this->x *= rhs.x;
    this->y *= rhs.y;
    return *this;
}

inline Vector2D& Vector2D::operator*=(float rhs)
{
    this->x *= rhs;
    this->y *= rhs;
    return *this;
}
/*------------------ End Of Vector2D Class Functions -------------------*/



/*------------------ Vector3D Class Functions -------------------*/
inline Vector3D& Vector3D::operator+=(Vector3D const& rhs)
{
    this->x += rhs.x;
    this->y += rhs.y;
    this->z += rhs.z;
    return *this;
}

inline Vector3D& Vector3D::operator+=(float rhs)
{
    this->x += rhs;
    this->y += rhs;
    this->z += rhs;
    return *this;
}

inline Vector3D& Vector3D::operator-=(Vector3D const& rhs)
{
    this->x -= rhs.x;
    this->y -= rhs.y;
    this->z -= rhs.z;
    return *this;
}

inline Vector3D& Vector3D::operator-=(float rhs)
{
    this->x -= rhs;
    this->y -= rhs;
    this->z -= rhs;
    return *this;
}

inline Vector3D& Vector3D::operator*=(Vector3D const& rhs)
{
    this->x *= rhs.x;
    this->y *= rhs.y;
    this->z *= rhs.z;
    return *this;
}

inline Vector3D& Vector3D::operator*=(float rhs)
{
    this->x *= rhs;
    this->y *= rhs;
    this->z *= rhs;
    return *this;
}

inline Vector3D& Vector3D::operator/=(Vector3D const& rhs)
{
    this->x /= rhs.x;
    this->y /= rhs.y;
    this->z /= rhs.z;
    return *this;
}

inline Vector3D& Vector3D::operator/=(float rhs)
{
    this->x /= rhs;
    this->y /= rhs;
    this->z /= rhs;
    return *this;
}

/*------------------ End Of Vector3D Class Functions -------------------*/

/*------------------ End Of Member Functions -------------------*/



/*------------------ Non-Member Functions -------------------*/

/*------------------ Vector2D Functions -------------------*/
inline Vector2D operator+(Vector2D const& lhs, Vector2D const& rhs)
{
    Vector2D result;

    result.x = lhs.x + rhs.x;
    result.y = lhs.y + rhs.y;

    return result;
}

inline Vector2D operator-(Vector2D const& lhs, Vector2D const& rhs)
{
    Vector2D result;

    result.x = lhs.x - rhs.x;
    result.y = lhs.y - rhs.y;

    return result;
}

inline Vector2D operator*(Vector2D const& lhs, Vector2D const& rhs)
{
    Vector2D result;

    result.x = lhs.x * rhs.x;
    result.y = lhs.y * rhs.y;

    return result;
}

inline Vector2D operator/(Vector2D const& lhs, Vector2D const& rhs)
{
    Vector2D result;

    result.x = lhs.x / rhs.x;
    result.y = lhs.y / rhs.y;

    return result;
}

inline bool operator==(Vector2D const& lhs, Vector2D const& rhs)
{
    return ((lhs.x == rhs.x) && (lhs.y == rhs.y));
}

inline bool operator!=(Vector2D const& lhs, Vector2D const& rhs)
{
    return !(lhs == rhs);
}

/*-------------------- End Of Vector2D --------------------*/

/*------------------ Vector3D Functions -------------------*/
inline Vector3D operator+(Vector3D const& lhs, Vector3D const& rhs)
{
    Vector3D result;

    result.x = lhs.x + rhs.x;
    result.y = lhs.y + rhs.y;
    result.z = lhs.z + rhs.z;

    return result;
}

inline Vector3D operator-(Vector3D const& lhs, Vector3D const& rhs)
{
    Vector3D result;

    result.x = lhs.x - rhs.x;
    result.y = lhs.y - rhs.y;
    result.z = lhs.z - rhs.z;

    return result;
}

inline Vector3D operator*(Vector3D const& lhs, Vector3D const& rhs)
{
    Vector3D result;

    result.x = lhs.x * rhs.x;
    result.y = lhs.y * rhs.y;
    result.z = lhs.z * rhs.z;

    return result;
}

inline Vector3D operator/(Vector3D const& lhs, Vector3D const& rhs)
{
    Vector3D result;

    result.x = lhs.x / rhs.x;
    result.y = lhs.y / rhs.y;
    result.z = lhs.z / rhs.z;

    return result;
}

inline bool operator==(Vector3D const& lhs, Vector3D const& rhs)
{
    return ((lhs.x == rhs.x) && (lhs.y == rhs.y) && (lhs.z == rhs.z));
}

inline bool operator!=(Vector3D const& lhs, Vector3D const& rhs)
{
    return !(lhs == rhs);
}

/*-------------------- End Of Vector3D --------------------*/

/*------------------ End Of Non-Member Functions -------------------*/