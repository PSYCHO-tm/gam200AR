#pragma once
#include <cstddef>

// ---------------------------------------------------------------------------------------------------------------------
// Loader: the "looking for the card" spinner. A ring of see-through white dots in the middle of the screen, one dot
// brighter, stepping round once a second. Core::Render draws it whenever the station is not shown (still scanning,
// tracking lost, card covered).
//
// A drawing module: it registers its own mesh with the Renderer and draws itself. Nothing else needs to know it
// exists; to remove it, stop calling Draw.
// ---------------------------------------------------------------------------------------------------------------------
class Loader
{
public:
    static Loader& Get(); // the one loader (created the first time this is called)

    Loader(Loader const&) = delete;
    Loader& operator=(Loader const&) = delete;

    // Draws the spinner on top of whatever is on screen. 'seconds' keeps growing (real time: it drives the spin).
    // The first call registers the dot mesh with the Renderer.
    void Draw(float seconds, int viewportW, int viewportH);

private:
    Loader() = default;

    bool Registered = false;
    std::size_t DotMesh = 0; // the Renderer's id for the dot (a 1 x 1 square, Geometry's MakeQuad)
};
