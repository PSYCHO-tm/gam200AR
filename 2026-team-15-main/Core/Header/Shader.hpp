#pragma once
#include <GLES3/gl3.h>

#include <string>
#include <unordered_map>

// One linked GPU program (vertex + fragment shader). Uniform locations are looked up once and cached.
class Shader
{
public:
    Shader()  = default;
    ~Shader() = default; // the GPU program is freed by Destroy(), not here (the context may already be gone)

    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool Load(const char* vertexSource, const char* fragmentSource);
    void Destroy();
    void ContextLost(); // forget the handle without deleting: the driver already freed it

    void Use() const;
    bool IsValid() const;

    void SetInt(const char* name, int value);
    void SetFloat(const char* name, float value);
    void SetVec3(const char* name, const float* value);
    void SetMat4(const char* name, const float* value);

private:
    GLint         Location(const char* name);
    static GLuint Compile(GLenum type, const char* source);

    GLuint                                 program{0};
    std::unordered_map<std::string, GLint> locations{};
};