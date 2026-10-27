// Generated from C:/Users/WenJie/Documents/GitHub/2026-team-15/Assets/Shader/lit.vert
// Do not edit: edit the shader file instead.
#pragma once
inline constexpr const char* gLitVert = R"GLSL(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat4 uModel;
out vec3 vNormal;
void main()
{
    vNormal = mat3(uModel) * aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)GLSL";
