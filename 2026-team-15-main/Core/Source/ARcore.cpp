#include "ARcore.hpp"
#include "arcore_c_api.h"
#include <android/log.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <vector>
#include <cmath>

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

//AR Camera Draw
static int displayWidth = 1;
static int displayHeight = 1;
static int displayRotation = 0;
static bool geometryDirty = true;

//OpenGL Camera
static void CameraDrawingInit();
static void CameraDrawUpdate();

//Sphere Draw
static GLuint sphereProgram = 0;
static GLint sphereModelLoc = -1;
static GLint sphereViewLoc = -1;
static GLint sphereProjectionLoc = -1;
static GLint sphereColourLoc = -1;
static const float pi = 3.14159265f;
static const int sphereRings = 12;
static const int sphereSlices = 24;
static const float sphereRadius = 0.1f;
static std::vector<float> sphereVertices;
static std::vector<unsigned short> sphereIndices;

//OpenGL Object
static void SphereDrawingInit();
static void SphereDrawUpdate();
static void BuildSphere(int rings, int slices, float radius);

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
        const ArStatus status = ArSession_configure(session, sessionConfig);
        if(status != AR_SUCCESS)
        {
            LOGI("AR session failed to configure (%d)", static_cast<int>(status));
            ArConfig_destroy(sessionConfig);
            sessionConfig = nullptr;
            return false;
        }
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

    if(session != nullptr)
    {
        ArSession_destroy(session);
    }
    session = nullptr;

    textureBound = false;
    geometryDirty = true;
}

/**
 * @brief   Creates the camera texture and builds the camera and sphere programs.
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
    SphereDrawingInit();
}

/**
 * @brief   Runs once per frame: updates ARCore, then draws the camera and sphere.
 * @details Tells ARCore about the camera texture and screen size if it does not
 *          know them yet, asks ARCore for the newest frame, then draws the camera
 *          picture first and the sphere on top. If the update fails, nothing is
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
    SphereDrawUpdate();
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
 *          This is the back layer, drawn before the sphere.
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

///////////////////////////////////////
//          Sphere Shader            //
///////////////////////////////////////
static const char* sphereVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPosition;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vNormal;
void main()
{
    vNormal = mat3(uModel) * aPosition;
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
)";

static const char* sphereFragmentShader = R"(#version 300 es
precision mediump float;
uniform vec3 uColour;
in vec3 vNormal;
out vec4 fragColor;
void main()
{
    vec3 lightDir = normalize(vec3(0.3, 0.8, 0.5));
    float brightness = max(dot(normalize(vNormal), lightDir), 0.0);
    float light = 0.3 + 0.7 * brightness;
    fragColor = vec4(uColour * light, 1.0);
}
)";

/**
 * @brief   Builds the sphere program and its points and triangles.
 * @details Builds the sphere's vertex and fragment shaders into a program, finds
 *          where its variables are (uModel, uView, uProjection, uColour), then
 *          calls BuildSphere. Called from ArCameraInit.
 */
static void SphereDrawingInit()
{
    sphereProgram = BuildProgram(sphereVertexShader, sphereFragmentShader);
    if(sphereProgram != 0)
    {
        sphereModelLoc = glGetUniformLocation(sphereProgram, "uModel");
        sphereViewLoc = glGetUniformLocation(sphereProgram, "uView");
        sphereProjectionLoc = glGetUniformLocation(sphereProgram, "uProjection");
        sphereColourLoc = glGetUniformLocation(sphereProgram, "uColour");
    }

    BuildSphere(sphereRings, sphereSlices, sphereRadius);
    LOGI("Sphere program built: %u, %zu points", sphereProgram, sphereVertices.size() / 3);
}

/**
 * @brief   Builds the sphere's points and triangles.
 * @details Fills sphereVertices with x, y, z for each point, ring by ring, and
 *          sphereIndices with the point numbers of each triangle.
 * @param[in] rings   Number of rings, top to bottom.
 * @param[in] slices  Number of slices, around the sphere.
 * @param[in] radius  Radius of the sphere in metres.
 * @note    The sphere is still hard-coded.
 */
static void BuildSphere(int rings, int slices, float radius)
{
    sphereVertices.clear();

    const float perPhi = static_cast<float>(pi) / static_cast<float>(rings);
    const float perTheta = 2.0f * static_cast<float>(pi) / static_cast<float>(slices);

    for(int i = 0; i <= rings; ++i)
    {
        const float phi = perPhi * static_cast<float>(i);
        const float sinPhi = sinf(phi);
        const float cosPhi = cosf(phi);

        for(int j = 0; j <= slices; ++j)
        {
            const float theta = perTheta * static_cast<float>(j);
            sphereVertices.push_back(radius * sinPhi * cosf(theta));   // x
            sphereVertices.push_back(radius * cosPhi);                 // y
            sphereVertices.push_back(radius * sinPhi * sinf(theta));   // z
        }
    }

    sphereIndices.clear();

    for(int i = 0; i < rings; ++i)
    {
        for(int j = 0; j < slices; ++j)
        {
            const unsigned short a = static_cast<unsigned short>(i * (slices + 1) + j);
            const unsigned short b = static_cast<unsigned short>((i + 1) * (slices + 1) + j);
            const unsigned short c = static_cast<unsigned short>(i * (slices + 1) + (j + 1));
            const unsigned short d = static_cast<unsigned short>((i + 1) * (slices + 1) + (j + 1));

            sphereIndices.push_back(a);
            sphereIndices.push_back(b);
            sphereIndices.push_back(c);

            sphereIndices.push_back(c);
            sphereIndices.push_back(b);
            sphereIndices.push_back(d);
        }
    }
}

/**
 * @brief   Draws the sphere on top of the camera picture.
 * @details Only draws while ARCore is tracking, because only then can the
 *          phone's position be trusted. Sends the view and projection matrices
 *          from ARCore, the model matrix and the colour to the GPU, then draws
 *          the triangles.
 * @note    The sphere's position is still hard-coded and is not fixed to a plane.
 */
static void SphereDrawUpdate()
{
    static bool drawLogged = false;
    if(!drawLogged)
    {
        LOGI("Sphere draw started");
        drawLogged = true;
    }

    if(sphereProgram == 0)
    {
        return;
    }

    ArCamera* camera = nullptr;
    ArFrame_acquireCamera(session, sessionFrame, &camera);

    ArTrackingState trackingState = AR_TRACKING_STATE_STOPPED;
    ArCamera_getTrackingState(session, camera, &trackingState);

    static int lastTrackingState = -1;
    if(static_cast<int>(trackingState) != lastTrackingState)
    {
        lastTrackingState = static_cast<int>(trackingState);
        LOGI("Tracking state: %s", trackingState == AR_TRACKING_STATE_TRACKING ? "TRACKING" : trackingState == AR_TRACKING_STATE_PAUSED   ? "PAUSED" : "STOPPED");
    }

    static float lastView[16];
    static float lastProjection[16];
    static int framesSinceTracking = 1000;      // 1000 means "never tracked yet"
    static const int holdFrames = 15;

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

    const bool drawBall = (framesSinceTracking <= holdFrames);

    static int statFrames = 0;
    static int statDrawn = 0;
    ++statFrames;
    if(drawBall)
    {
        ++statDrawn;
    }
    if(statFrames == 60)
    {
        LOGI("Ball drawn in %d of 60 frames", statDrawn);
        statFrames = 0;
        statDrawn = 0;
    }

    if(drawBall)
    {
        const float model[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
                                  0.0f, 1.0f, 0.0f, 0.0f,
                                  0.0f, 0.0f, 1.0f, 0.0f,
                                  0.0f, -0.3f, -1.0f, 1.0f };
        const float colour[3] = { 1.0f, 0.45f, 0.0f };

        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glUseProgram(sphereProgram);
        glUniformMatrix4fv(sphereModelLoc, 1, GL_FALSE, model);
        glUniformMatrix4fv(sphereViewLoc, 1, GL_FALSE, lastView);
        glUniformMatrix4fv(sphereProjectionLoc, 1, GL_FALSE, lastProjection);
        glUniform3fv(sphereColourLoc, 1, colour);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, sphereVertices.data());

        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sphereIndices.size()), GL_UNSIGNED_SHORT, sphereIndices.data());

        glDisableVertexAttribArray(0);
        glDisable(GL_DEPTH_TEST);
    }

    ArCamera_release(camera);
}