// --------------------------------------------------------------------------------------------------------------------
// ArCamera: draws the camera picture as the back layer (always on).
//
// ARCore writes each camera frame into an external OES texture (gAr.cameraTexture, made in ArCameraInit).
// This module owns the tiny shader that draws that texture over the whole screen, with ARCore's own UVs so the
// picture matches the screen's shape and rotation. Called from ArCameraInit (once per GL context) and
// ArCameraDraw (every frame).
// --------------------------------------------------------------------------------------------------------------------

#include "AR/ArModules.hpp"

// Shared AR state this module uses (owned by ArContext, see AR/ArModules.hpp).
static GLuint& cameraTexture = gAr.CameraTexture;
static ArSession*& session = gAr.Session;
static ArFrame*& sessionFrame = gAr.SessionFrame;

// Private to this module
static GLuint cameraProgram = 0;
static GLint cameraSamplerLoc = -1;


////////////////////////////////////////////
//             Camera Shader              //
////////////////////////////////////////////
static const char* cameraFragmentShader = R"(#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision mediump float;
uniform samplerExternalOES uCamera;
in vec2 vUv;
out vec4 fragColor;
void main()
{
    fragColor = texture(uCamera, vUv);
}
)";

static const char* cameraVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
out vec2 vUv;
void main()
{
    vUv = aUv;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

/**
 * @brief   Compiles one shader from its source text.
 * @details Checks the result and prints the driver's error message to Logcat if
 *          the compile fails, so a typo does not just give a black screen.
 * @param[in] type    GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
 * @param[in] source  The shader's source text.
 * @return  The shader's number, or 0 if the compile failed.
 */
static GLuint CompileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);        // reserve a shader number
    glShaderSource(shader, 1, &source, nullptr); // hand over the text
    glCompileShader(shader);                     // the driver compiles it now

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOGE("Shader compile error: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

/**
 * @brief   Builds a program from a vertex shader and a fragment shader.
 * @details Compiles both shaders, links them into one program, then frees the
 *          separate shaders. The reason for any failure is printed to Logcat.
 * @param[in] vertexSource    Source text of the vertex shader.
 * @param[in] fragmentSource  Source text of the fragment shader.
 * @return  The program's number, or 0 if the build failed.
 */
static GLuint BuildProgram(const char* vertexSource, const char* fragmentSource)
{
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertexShader == 0 || fragmentShader == 0)
    {
        if (vertexShader != 0)
        {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0)
        {
            glDeleteShader(fragmentShader);
        }
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        LOGE("Program link error: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

/**
 * @brief   Builds the camera program and finds where its uCamera variable is.
 * @details Called from ArCameraInit. The location is looked up once here and
 *          used every frame by CameraDraw.
 */
void ArCamPictureInit()
{
    cameraProgram = BuildProgram(cameraVertexShader, cameraFragmentShader);
    if (cameraProgram != 0)
    {
        cameraSamplerLoc = glGetUniformLocation(cameraProgram, "uCamera");
    }

    LOGI("Camera program built: %u", cameraProgram);
}

/**
 * @brief   Draws the camera picture over the whole screen.
 * @details Asks ARCore which spot of the picture belongs at each screen corner,
 *          then draws the screen-sized rectangle with the camera program.
 *          This is the back layer, drawn before the equipment.
 */
void ArCamPictureDraw()
{
    if (cameraProgram == 0)
    {
        return;
    }

    static const float corners[8] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};

    float uv[8];
    ArFrame_transformCoordinates2d(session, sessionFrame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES, 4,
                                   corners, AR_COORDINATES_2D_TEXTURE_NORMALIZED, uv);


    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(cameraProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, cameraTexture);
    glUniform1i(cameraSamplerLoc, 0);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, corners);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, uv);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}
