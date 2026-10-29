#include "Button.hpp"

Button2D::Button2D(const Transform2D t, ButtonFunction f)
: transform{t}, function{std::move(f)} 
{}

const Transform2D& Button2D::GetTransform() const
{
    return this->transform;
}

void Button2D::SetButtonFunction(ButtonFunction f)
{
    function = f;
}

bool Button2D::Contains(float x, float y) const
{
    // ASSUMPTION: Position = top-left corner, Scale = width/height, both in screen pixels.
    const float left = this->transform.position.x;
    const float top  = this->transform.position.y;
    return x >= left && x <= left + this->transform.scale.x && y >= top && y <= top + this->transform.scale.y;
}

void Button2D::Update(InputSnapshot const& input)
{
    const InputState& s = input.Is;

    if (s.PtrCancelled)
        pressed = false;

    if (s.PtrPressed)
        pressed = Contains(s.PressX, s.PressY);

    if (s.PtrReleased && !s.PtrCancelled)
    {
        const bool clicked = pressed && Contains(s.Px, s.Py);
        pressed = false;
        if (clicked && function) 
        {
            function();
        }
    }
}