// Do not edit: edit the shader file instead.
#pragma once
inline constexpr const char* gLitVert = R"GLSL(#version 300 es
// The lit shader's vertex stage: places each corner on screen and passes its normal on for lighting.
// (The #version line must stay first: Shader.cpp swaps it for the desktop 330 core version.)
// Built into the app by CMake as lit_vert.hpp; set up by Renderer::DrawMeshes.
layout(location = 0) in vec3 aPos;    // Mesh.cpp attribute 0: position, model space
layout(location = 1) in vec3 aNormal; // Mesh.cpp attribute 1: normal, model space
uniform mat4 uMvp;   // model -> clip (model * view * projection, one matrix per draw)
uniform mat4 uModel; // model -> world, to turn the normal into world space
out vec3 vNormal;    // world-space normal (not normalised: the fragment stage does that)
out vec4 vClip; // clip-space position: the fragment shader finds its screen spot and its distance from the camera
void main()
{
    vNormal = mat3(uModel) * aNormal; // fine for our uniform scales; a squashed model would need the inverse transpose
    gl_Position = uMvp * vec4(aPos, 1.0);
    vClip = gl_Position;
}
)GLSL";
