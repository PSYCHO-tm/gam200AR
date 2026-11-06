#pragma once
#include "GlApi.hpp"

#include <string>
#include <unordered_map>
#include <vector>

// One linked GPU program (vertex + fragment shader). Uniform locations are looked up once and cached.
// The shader text comes in as strings: CMake turns Assets/Shader/*.vert|frag into lit_vert.hpp / lit_frag.hpp, so the
// app needs no shader files at run time. The sources are written for OpenGL ES 3.0; on desktop they are rewritten to
// GLSL 330 core before compiling (see ToDesktopGlsl in Shader.cpp).
class Shader
{
public:
    Shader()  = default;
    ~Shader() = default; // the GPU program is freed by Destroy(), not here (the context may already be gone)

    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;            // moving hands the program over; 'other' is left empty
    Shader& operator=(Shader&& other) noexcept;

    bool Load(const char* vertexSource, const char* fragmentSource); // compile + link; false (and logged) on any error
    void Destroy();     // deletes the program (needs a live context)
    void ContextLost(); // forget the handle without deleting: the driver already freed it

    void Use() const;     // makes this the current program: call before the Set* functions
    bool IsValid() const; // true once Load succeeded

    // Uniform setters. The program must be in use. A misspelled or unused name logs once and is then ignored.
    void SetInt(const char* name, int value); // also for samplers: the value is the texture unit
    void SetFloat(const char* name, float value);
    void SetVec3(const char* name, const float* value);
    void SetMat4(const char* name, const float* value); // 16 floats, column-major (Matrix4x4's memory, as it is)

private:
    GLint         Location(const char* name);                // cached glGetUniformLocation
    static GLuint Compile(GLenum type, const char* source); // one stage; 0 on error (log printed)

    GLuint                                 program{0};       // 0 = not loaded
    std::unordered_map<std::string, GLint> locations{};      // uniform name -> location (-1 kept too, so it logs once)
    // Sandbox optimisation: the names passed in are string literals, so the same pointer comes back every frame.
    // A tiny pointer-keyed cache in front of the map skips building a std::string (a heap allocation) per uniform.
    struct FastLocation
    {
        const char* name;
        GLint       location;
    };
    std::vector<FastLocation> fastLocations{};
};