#include "ARcore.hpp"
#include "arcore_c_api.h"
#include <android/log.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <cstring>

#define LOG_TAG "kopitwin"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

//OpenGL Camera
static GLuint cameraTexture = 0;
static GLuint cameraProgram = 0;
static GLint cameraSamplerLoc = -1;

//AR Session
static ArSession* session = nullptr;
static ArConfig* sessionConfig = nullptr;
static ArFrame* sessionFrame = nullptr;
static bool textureBound = false;
static ArTrackableList* sessionList = nullptr;

//AR Camera Draw
static int displayWidth = 1;
static int displayHeight = 1;
static int displayRotation = 0;
static bool geometryDirty = true;

//OpenGL Camera
static void CameraDrawingInit();
static void CameraDrawUpdate();

//Camera for drawing (read by ArGetCameraMatrices)
static float lastView[16]{};
static float lastProjection[16]{};
static int framesSinceTracking = 1000;      // 1000 means "never tracked yet"
static const int holdFrames = 15;           // keep using the last good camera for this many frames after tracking drops

/**
 * @brief   Checks that the phone supports ARCore and that it is installed.
 * @details If the phone supports ARCore but it is missing or out of date, this
 *          asks Google Play to install or update it. While that screen is up
 *          the app pauses, so the next call (from onResume) tries again.
 * @param[in] env       The JNI environment (JNIEnv*), passed as void*.
 * @param[in] activity  The Android activity (jobject), passed as void*.
 * @return  true if ARCore is ready and a session can be created, otherwise
 *          false (call again later).
 */
bool ArInitialise(void *env, void *activity)
{
    // Check if mobile phone has been installed
    static bool installRequested = false;
    
    ArAvailability availability;
    ArCoreApk_checkAvailability(env, activity, &availability);

    if(availability == AR_AVAILABILITY_UNSUPPORTED_DEVICE_NOT_CAPABLE)
    {
        LOGI("AR not supported by this device");
        return false;
    }
    else if(availability == AR_AVAILABILITY_SUPPORTED_NOT_INSTALLED || availability == AR_AVAILABILITY_SUPPORTED_APK_TOO_OLD)
    {
        LOGI("AR supported but not installed or updated");
        ArInstallStatus installStatus;
        const ArStatus status = ArCoreApk_requestInstall(env, activity, !installRequested, &installStatus);
        installRequested = true;
        if(status == AR_SUCCESS && installStatus == AR_INSTALL_STATUS_INSTALLED)
        {
            return true;
        }
        return false;
    }
    else if(availability == AR_AVAILABILITY_SUPPORTED_INSTALLED)
    {
        LOGI("AR is installed");
        return true;
    }
    else
    {
        LOGI("AR availability not known yet (%d), try again", static_cast<int>(availability));
        return false;
    }
}

/**
 * @brief   Creates the ARCore session, its settings and the frame object.
 * @details Each of the three is only created if it does not exist yet, so it
 *          is safe to call twice. The session is created here but not started;
 *          ArSessionResume starts it.
 * @param[in] env       The JNI environment (JNIEnv*), passed as void*.
 * @param[in] activity  The Android activity (jobject), passed as void*.
 * @return  true if the session, settings and frame all exist afterwards,
 *          otherwise false.
 */
bool ArSessionCreate(void *env, void *activity)
{
    if(session == nullptr)
    {
        const ArStatus status = ArSession_create(env, activity, &session);
        if(status != AR_SUCCESS)
        {
            LOGI("AR session failed to create (%d)", static_cast<int>(status));
            session = nullptr;
            return false;
        }
    }

    if(sessionConfig == nullptr)
    {
        ArConfig_create(session, &sessionConfig);
        ArConfig_setPlaneFindingMode(session, sessionConfig, AR_PLANE_FINDING_MODE_HORIZONTAL);
        const ArStatus status = ArSession_configure(session, sessionConfig);
        if(status != AR_SUCCESS)
        {
            LOGI("AR session failed to configure (%d)", static_cast<int>(status));
            ArConfig_destroy(sessionConfig);
            sessionConfig = nullptr;
            return false;
        }
    }

    if(sessionList == nullptr)
    {
        ArTrackableList_create(session, &sessionList);
    }

    if(sessionFrame == nullptr)
    {
        ArFrame_create(session, &sessionFrame);
    }
    return true;
}

/**
 * @brief   Starts or resumes the ARCore session.
 * @details Turns the camera on and starts tracking. Called when the app comes
 *          to the front, once the camera permission is granted.
 * @return  true if the session is running, otherwise false (for example,
 *          error -9 means the camera permission was not granted).
 */
bool ArSessionResume()
{
    if(session == nullptr)
    {
        LOGI("There's no session running.");
        return false;
    }

    ArStatus status = ArSession_resume(session);

    if(status != AR_SUCCESS)
    {
        LOGI("AR Session failed to Start or Resume. Error: (%d)", static_cast<int>(status));
        return false;
    }
    return true;
}

/**
 * @brief   Pauses the ARCore session.
 * @details Turns the camera off when the app goes to the background. The
 *          session and its objects are kept, so ArSessionResume can start it
 *          again.
 * @return  true if the session was paused, otherwise false.
 */
bool ArSessionPause()
{
    if(session == nullptr)
    {
        LOGI("There's no session running.");
        return false;
    }

    ArStatus status = ArSession_pause(session);

    if(status != AR_SUCCESS)
    {
        LOGI("AR Session failed to Pause. Error: (%d)", static_cast<int>(status));
        return false;
    }
    return true;
}

/**
 * @brief   Destroys the ARCore session and its objects.
 * @details Called when the app is closed. The frame, settings and session are
 *          freed in that order (the reverse of creation), and the flags that
 *          tell ARCore about the texture and screen are reset.
 */
void ArSessionDestroy()
{
    if(sessionFrame != nullptr)
    {
        ArFrame_destroy(sessionFrame);
    }
    sessionFrame = nullptr;
    
    if(sessionConfig != nullptr)
    {
        ArConfig_destroy(sessionConfig);
    }
    sessionConfig = nullptr;

    if(sessionList != nullptr)
    {
        ArTrackableList_destroy(sessionList);
    }
    sessionList = nullptr;

    if(session != nullptr)
    {
        ArSession_destroy(session);
    }
    session = nullptr;

    textureBound = false;
    geometryDirty = true;
    framesSinceTracking = 1000;
}

/**
 * @brief   Creates the camera texture and builds the camera program.
 * @details Runs on the drawing thread, once for each new OpenGL context. The
 *          texture is where ARCore writes each camera picture.
 */
void ArCameraInit()
{
    glGenTextures(1, &cameraTexture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, cameraTexture);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    LOGI("Camera texture created: %u", cameraTexture);

    textureBound = false;
    CameraDrawingInit();
}

/**
 * @brief   Runs once per frame: updates ARCore, then draws the camera picture.
 * @details Tells ARCore about the camera texture and screen size if it does not
 *          know them yet, asks ARCore for the newest frame, then draws the camera
 *          picture. The equipment is drawn after this by Core::Render, using the
 *          matrices from ArGetCameraMatrices. If the update fails, nothing is
 *          drawn that frame.
 */
void ArCameraDraw()
{
    if(session == nullptr || sessionFrame == nullptr || cameraTexture == 0)
    {
        return;
    }

    if(!textureBound)
    {
        ArSession_setCameraTextureName(session, cameraTexture);
        textureBound = true;
        LOGI("Camera texture (%u) given to ARCore", cameraTexture);
    }

    if(geometryDirty)
    {
        ArSession_setDisplayGeometry(session, displayRotation, displayWidth, displayHeight);
        geometryDirty = false;
        LOGI("Display geometry given to ARCore: %d x %d, rotation %d", displayWidth, displayHeight, displayRotation);
    }

    static int lastUpdateStatus = 12345;
    const ArStatus updateStatus = ArSession_update(session, sessionFrame);
    if(static_cast<int>(updateStatus) != lastUpdateStatus)
    {
        lastUpdateStatus = static_cast<int>(updateStatus);
        LOGI("ArSession_update status: (%d)", lastUpdateStatus);
    }

    static int frameCount = 0;
    ++frameCount;
    if(frameCount % 60 == 0)
    {
        LOGI("ArCameraDraw alive: %d frames, update status %d", frameCount, static_cast<int>(updateStatus));
    }
    
    if(updateStatus != AR_SUCCESS)
    {
        return;
    }

    CameraDrawUpdate();

    if(sessionList != nullptr)
    {
        int32_t n{};
        ArTrackableList_getSize(session, sessionList, &n);
        ArSession_getAllTrackables(session, AR_TRACKABLE_PLANE, sessionList);
        LOGI("The size count is (%d)", n);
    }
}

/**
 * @brief   Remembers the new screen size and rotation for ARCore.
 * @details Called when the phone's width, height or rotation changes.
 *          ARCore is told later, from ArCameraDraw.
 * @param[in] width     Screen width in pixels.
 * @param[in] height    Screen height in pixels.
 * @param[in] rotation  Screen rotation: 0, 1, 2 or 3 (Android Surface.ROTATION_*).
 */
void ArCameraResize(int width, int height, int rotation)
{
    displayWidth = width;
    displayHeight = height;
    displayRotation = rotation;
    geometryDirty = true;
}

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
    GLuint shader = glCreateShader(type);          // reserve a shader number
    glShaderSource(shader, 1, &source, nullptr);   // hand over the text
    glCompileShader(shader);                       // the driver compiles it now
 
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(ok == GL_FALSE)
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
    if(vertexShader == 0 || fragmentShader == 0)
    {
        if(vertexShader != 0)
        {
            glDeleteShader(vertexShader);
        }
        if(fragmentShader != 0)
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
    if(ok == GL_FALSE)
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
 *          used every frame by CameraDrawUpdate.
 */
static void CameraDrawingInit()
{
    cameraProgram = BuildProgram(cameraVertexShader, cameraFragmentShader);
    if(cameraProgram != 0)
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
static void CameraDrawUpdate()
{
    if(cameraProgram == 0)
    {
        return;
    }

    static const float corners[8] = { -1.0f, -1.0f,    1.0f, -1.0f,    -1.0f, 1.0f,    1.0f, 1.0f };

    float uv[8];
    ArFrame_transformCoordinates2d(session, sessionFrame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES, 4, corners, AR_COORDINATES_2D_TEXTURE_NORMALIZED, uv);


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

/**
 * @brief   Gives the phone camera's view and projection matrices from the latest frame.
 * @details Call after ArCameraDraw. Only valid while ARCore is tracking, and for a few
 *          frames after it drops (the last good camera is held, so the equipment does
 *          not blink when tracking flickers).
 * @param[out] view        16 floats, in the layout OpenGL uses (column-major).
 * @param[out] projection  16 floats, same layout.
 * @return  true if both were written, otherwise false (nothing is written).
 */
bool ArGetCameraMatrices(float* view, float* projection)
{
    if(session == nullptr || sessionFrame == nullptr)
    {
        return false;
    }

    ArCamera* camera = nullptr;
    ArFrame_acquireCamera(session, sessionFrame, &camera);
    if(camera == nullptr)
    {
        return false;
    }

    ArTrackingState trackingState = AR_TRACKING_STATE_STOPPED;
    ArCamera_getTrackingState(session, camera, &trackingState);

    static int lastTrackingState = -1;
    if(static_cast<int>(trackingState) != lastTrackingState)
    {
        lastTrackingState = static_cast<int>(trackingState);
        LOGI("Tracking state: %s", trackingState == AR_TRACKING_STATE_TRACKING ? "TRACKING" : trackingState == AR_TRACKING_STATE_PAUSED   ? "PAUSED" : "STOPPED");
    }

    if(trackingState == AR_TRACKING_STATE_TRACKING)
    {
        ArCamera_getViewMatrix(session, camera, lastView);
        ArCamera_getProjectionMatrix(session, camera, 0.05f, 100.0f, lastProjection);
        framesSinceTracking = 0;
    }
    else if(framesSinceTracking < 1000)
    {
        ++framesSinceTracking;
    }
    ArCamera_release(camera);

    if(framesSinceTracking > holdFrames)
    {
        return false;
    }

    std::memcpy(view, lastView, sizeof(lastView));
    std::memcpy(projection, lastProjection, sizeof(lastProjection));
    return true;
}