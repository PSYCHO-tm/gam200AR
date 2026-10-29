#include "Shader.hpp"

#include <string>
#include <utility>

#include "PlatformLog.hpp"

namespace
{
#if !defined(__ANDROID__)
    // The shader files are written for OpenGL ES ("#version 300 es").
    // Desktop OpenGL wants "#version 330 core" and has no precision qualifiers.
    std::string ToDesktopGlsl(const char* source)
    {
        std::string text{source};

        constexpr const char* esVersion = "#version 300 es";
        if (text.rfind(esVersion, 0) == 0)
        {
            text.replace(0, std::char_traits<char>::length(esVersion), "#version 330 core");
        }

        constexpr const char* precisionLine = "precision mediump float;";
        const std::size_t at = text.find(precisionLine);
        if (at != std::string::npos)
        {
            text.erase(at, std::char_traits<char>::length(precisionLine));
        }
        return text;
    }
#endif
}

Shader::Shader(Shader&& other) noexcept
    : program{other.program},
      locations{std::move(other.locations)}
{
    other.program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        program       = other.program;
        locations     = std::move(other.locations);
        other.program = 0;
    }
    return *this;
}

GLuint Shader::Compile(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
#if !defined(__ANDROID__)
    const std::string desktopSource = ToDesktopGlsl(source);
    source = desktopSource.c_str();
#endif
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOGE("Shader compile error: (%s)", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::Load(const char* vertexSource, const char* fragmentSource)
{
    Destroy();

    const GLuint vs = Compile(GL_VERTEX_SHADER, vertexSource);
    const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragmentSource);
    if (vs == 0 || fs == 0)
    {
        if (vs != 0)
        {
            glDeleteShader(vs);
        }
        if (fs != 0)
        {
            glDeleteShader(fs);
        }
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        LOGE("Program link error: (%s)", log);
        Destroy();
        return false;
    }
    return true;
}

void Shader::Destroy()
{
    if (program != 0)
    {
        glDeleteProgram(program);
    }
    program = 0;
    locations.clear();
}

void Shader::ContextLost()
{
    program = 0;
    locations.clear();
}

void Shader::Use() const { glUseProgram(program); }

bool Shader::IsValid() const { return program != 0; }

GLint Shader::Location(const char* name)
{
    const auto it = locations.find(name);
    if (it != locations.end())
    {
        return it->second;
    }

    const GLint loc = glGetUniformLocation(program, name);
    if (loc == -1)
    {
        LOGE("Uniform '%s' not found (misspelled, or unused so the compiler removed it)", name);
    }
    locations.emplace(name, loc);
    return loc;
}

void Shader::SetInt(const char* name, int value) { glUniform1i(Location(name), value); }

void Shader::SetFloat(const char* name, float value) { glUniform1f(Location(name), value); }

void Shader::SetVec3(const char* name, const float* value) { glUniform3fv(Location(name), 1, value); }

void Shader::SetMat4(const char* name, const float* value) { glUniformMatrix4fv(Location(name), 1, GL_FALSE, value); }