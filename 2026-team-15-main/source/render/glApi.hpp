#pragma once

// The one place that decides which GL headers and shader version the renderer uses.
// Android: OpenGL ES 3.0. Desktop: GL 4.3 core through glad2; change the include if yours differs.

#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#define KOPI_GLSL_HEADER "#version 300 es\nprecision highp float;\n"
#elif defined(KOPI_GL_HOST_EGL)
// Host check build: Mesa software GLES through EGL, same shaders as the phone.
#include <GLES3/gl3.h>
#define KOPI_GLSL_HEADER "#version 300 es\nprecision highp float;\n"
#else
#include <glad/gl.h> // glad2, generated for GL 4.3 core; vendored in Platform/desktop/third_party/glad
#define KOPI_GLSL_HEADER "#version 430 core\n"
#endif
