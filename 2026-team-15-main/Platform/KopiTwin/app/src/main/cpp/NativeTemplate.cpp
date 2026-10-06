/**
 * NativeTemplate.cpp
 *
 * JNI bridge between the Kotlin/Android layer and the OpenGL ES 3.0 renderer.
 * Self-contained - no Scene/ framework headers are used here.
 *
 * Native method signatures match MainActivity.kt:
 *   package com.example.kopitwin
 *   class   MainActivity
 */

#define LOG_TAG "kopitwin"

#include <jni.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <time.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// Vertex data
// ---------------------------------------------------------------------------

static const GLfloat gTriangleVertices[] = {
        0.0f,  0.5f,
        -0.5f, -0.5f,
        0.5f, -0.5f
};

static const GLfloat gTriangleColors[] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
};

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

static const char vertexShader[] =
        "#version 300 es                                           \n"
        "layout(location = 0) in vec4 VertexPosition;             \n"
        "layout(location = 1) in vec3 VertexColor;                \n"
        "uniform float RadianAngle;                                \n"
        "out vec4 TriangleColor;                                   \n"
        "void main() {                                             \n"
        "  mat2 r = mat2(cos(RadianAngle),  sin(RadianAngle),      \n"
        "               -sin(RadianAngle),  cos(RadianAngle));     \n"
        "  vec2 pos = r * VertexPosition.xy;                       \n"
        "  gl_Position = vec4(pos, 0.0, 1.0);                      \n"
        "  TriangleColor = vec4(VertexColor, 1.0);                 \n"
        "}\n";

static const char fragmentShader[] =
        "#version 300 es            \n"
        "precision mediump float;   \n"
        "in  vec4 TriangleColor;    \n"
        "out vec4 FragColor;        \n"
        "void main() {              \n"
        "  FragColor = TriangleColor;\n"
        "}";

// ---------------------------------------------------------------------------
// GL state
// ---------------------------------------------------------------------------

static GLuint programID    = 0;
static GLuint vao          = 0;
static GLuint vboPos       = 0;
static GLuint vboColor     = 0;
static GLint  uRadianAngle = -1;

// ---------------------------------------------------------------------------
// Time / animation state
// ---------------------------------------------------------------------------

static const float kTwoPi            = 6.2831853f;
static const float kRotationSpeed    = 1.0471976f;  // radians per second (60 deg/s)
static const float kMaxDeltaTime     = 0.1f;        // clamp after pause/resume

static float gAngle    = 0.0f;   // radians, kept in [0, 2*pi)
static float gLastTime = 0.0f;   // seconds

static float NowSeconds() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (float)ts.tv_sec + (float)ts.tv_nsec * 1e-9f;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void printGLString(const char* name, GLenum s) {
    LOGI("GL %s = %s", name, (const char*)glGetString(s));
}

static void checkGlError(const char* op) {
    for (GLint err = glGetError(); err; err = glGetError())
        LOGE("after %s() glError (0x%x)", op, err);
}

static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    if (!shader) { LOGE("glCreateShader failed (type=0x%x)", type); return 0; }
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char* buf = new char[len + 1]();
        glGetShaderInfoLog(shader, len, nullptr, buf);
        LOGE("Shader compile error:\n%s", buf);
        delete[] buf;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint createProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vertSrc);
    if (!vs) return 0;
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        char* buf = new char[len + 1]();
        glGetProgramInfoLog(prog, len, nullptr, buf);
        LOGE("Program link error:\n%s", buf);
        delete[] buf;
        glDeleteProgram(prog);
        prog = 0;
    }
    if (prog) {
        glDetachShader(prog, vs);
        glDetachShader(prog, fs);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool GraphicsInit()
{
    printGLString("Version",          GL_VERSION);
    printGLString("Vendor",           GL_VENDOR);
    printGLString("Renderer",         GL_RENDERER);
    printGLString("Extensions",       GL_EXTENSIONS);
    printGLString("Shading Language", GL_SHADING_LANGUAGE_VERSION);

    programID = createProgram(vertexShader, fragmentShader);
    if (!programID) { LOGE("Could not create program."); return false; }

    uRadianAngle = glGetUniformLocation(programID, "RadianAngle");

    // VAO - required in OpenGL ES 3.0 core before setting up vertex attributes
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Position VBO (location = 0, 2 floats per vertex)
    glGenBuffers(1, &vboPos);
    glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gTriangleVertices), gTriangleVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Color VBO (location = 1, 3 floats per vertex)
    glGenBuffers(1, &vboColor);
    glBindBuffer(GL_ARRAY_BUFFER, vboColor);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gTriangleColors), gTriangleColors, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Start the clock here so the first frame's delta time is ~0
    gAngle    = 0.0f;
    gLastTime = NowSeconds();

    checkGlError("GraphicsInit");
    return true;
}

bool GraphicsResize(int width, int height)
{
    LOGI("nativeRenderer %d x %d", width, height);
    glViewport(0, 0, width, height);
    return true;
}

bool GraphicsRender()
{
    static bool first = true;
    if(first)
    {
        LOGI("first frame");
        first = false;
    }
    // --- delta time ---
    float now = NowSeconds();
    float dt  = now - gLastTime;
    gLastTime = now;
    if (dt > kMaxDeltaTime) dt = kMaxDeltaTime;

    // --- update ---
    gAngle = fmodf(gAngle + kRotationSpeed * dt, kTwoPi);

    // --- draw ---
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(programID);
    glUniform1f(uRadianAngle, gAngle);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    return true;
}

// ---------------------------------------------------------------------------
// JNI entry points
// (the "com_example_kopitwin" part must match your Kotlin package)
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_kopitwin_MainActivity_nativeInit(JNIEnv* /*env*/, jobject /*thiz*/)
{
    LOGI("nativeInit called");
    return GraphicsInit() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeResize(
        JNIEnv* /*env*/, jobject /*thiz*/, jint width, jint height)
{
    GraphicsResize(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeRender(JNIEnv* /*env*/, jobject /*thiz*/)
{
    GraphicsRender();
}