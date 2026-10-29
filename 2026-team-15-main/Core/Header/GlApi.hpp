#pragma once
// One include for OpenGL on every platform, and KOPI_HAS_GL to ask "is there a GPU API here at all?".
// Android -> OpenGL ES 3. Windows -> GLEW (call glewInit() once after the GL context exists).
// Linux/WSL console build (tests, CI) -> no OpenGL; the GL sources are left out of the build (Core/CMakeLists.txt).
#if defined(__ANDROID__)
  #include <GLES3/gl3.h>
  #define KOPI_HAS_GL 1
#elif defined(_WIN32)
  #include <GL/glew.h>
  #define KOPI_HAS_GL 1
#endif
