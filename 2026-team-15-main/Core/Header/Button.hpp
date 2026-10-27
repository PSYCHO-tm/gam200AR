#pragma once
#include "Transform2D.hpp"
#include "Transform3D.hpp"
#include "Input.hpp"
#include <functional>
#include <utility>

class Button2D
{
    public:
        using ButtonFunction = std::function<void()>;

        Button2D() = default;
        Button2D(const Transform2D t, ButtonFunction f);

        const Transform2D& GetTransform() const;
        void SetButtonFunction(ButtonFunction f);

        void Update(InputSnapshot const& input);
        bool Contains(float x, float y) const;

    private:
        Transform2D     transform{};
        ButtonFunction  function{};
        // Text
        // Texture
        bool            pressed{false};
};