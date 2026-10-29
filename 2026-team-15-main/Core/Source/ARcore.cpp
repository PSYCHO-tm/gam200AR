#include "ARcore.hpp"
#include "arcore_c_api.h"
#include <android/log.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <cstdio>
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
static ArAnchor* anchor = nullptr;
static ArPose* anchorPoseHandle = nullptr;
static ArHitResultList* hitList = nullptr;   // what the middle-of-the-screen ray touched, nearest first
static ArHitResult* hitResult = nullptr;     // one entry of that list
static ArPose* hitPose = nullptr;            // where on the surface it touched
static ArPose* cameraPose = nullptr;         // where the phone is (only used to say how far below it a plane is)
static int validHitFrames = 0;
static float markerMatrix[16]{};             // where the middle-of-the-screen ray touched a plane (OpenGL layout)
static int markerState = 0;                  // 0 = no plane under the ray, 1 = plane but not usable, 2 = usable, holding steady
static float hitDistance = 0.0f;             // metres from the phone to that plane
static float hitBelow = 0.0f;                // metres the plane is below the phone
static const int maxDrawPlanes = 8;
static float drawPlaneMatrix[maxDrawPlanes][16]{};   // centre pose of each plane worth drawing (OpenGL layout)
static float drawPlaneExtent[maxDrawPlanes][2]{};    // its size along its own x and z, in metres
static int drawPlaneCount = 0;                       // how many planes are worth drawing right now
static ArPose* planeCentrePose = nullptr;            // reusable pose object for reading each plane's centre
static int cameraTrackingState = -1;         // the camera's tracking state this frame, for the on-screen status
static int planeCount = 0;                   // how many planes ARCore has right now, for the on-screen status
static const char* placementResult = nullptr; // what the middle-of-the-screen ray touched this frame, for the on-screen status               // how many tracked frames in a row the middle of the screen has pointed at a good plane
static const int framesToHold = 20;          // about 0.7 s at 30 fps: hold steady this long before the anchor is placed
static const float maxPlaceDistance = 3.0f;  // metres: ignore planes further away than this
static float lastAnchorMatrix[16]{};
static bool anchorPoseValid = false;

//AR Camera Draw
static int displayWidth = 1;
static int displayHeight = 1;
static int displayRotation = 0;
static bool geometryDirty = true;

//OpenGL Camera
static void CameraDrawingInit();
static void CameraDraw();
static bool frameUpdated = false;

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

    if(anchor != nullptr)
    {
        ArAnchor_detach(session, anchor);
        ArAnchor_release(anchor);
    }
    anchor = nullptr;

    if(anchorPoseHandle != nullptr)
    {
        ArPose_destroy(anchorPoseHandle);
    }
    anchorPoseHandle = nullptr;
    anchorPoseValid = false;

    if(hitPose != nullptr)
    {
        ArPose_destroy(hitPose);
    }
    hitPose = nullptr;

    if(cameraPose != nullptr)
    {
        ArPose_destroy(cameraPose);
    }
    cameraPose = nullptr;
    validHitFrames = 0;

    if(planeCentrePose != nullptr)
    {
        ArPose_destroy(planeCentrePose);
    }
    planeCentrePose = nullptr;
    drawPlaneCount = 0;
    cameraTrackingState = -1;
    markerState = 0;
    planeCount = 0;
    placementResult = nullptr;

    if(hitResult != nullptr)
    {
        ArHitResult_destroy(hitResult);
    }
    hitResult = nullptr;

    if(hitList != nullptr)
    {
        ArHitResultList_destroy(hitList);
    }
    hitList = nullptr;

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
 * @brief   Runs once per frame: asks ARCore for its newest frame and refreshes the plane list.
 * @details Tells ARCore about the camera texture and screen size if it does not know them yet,
 *          then updates the session. Call this before ArGetCameraMatrices, ArGetAnchorPose,
 *          ArGetPlacementMarker and ArGetPlanes, so they all read the same frame. If the update
 *          fails, no planes are reported and ArCameraDraw draws nothing that frame.
 */
void ArCameraUpdate()
{
    frameUpdated = false;
    drawPlaneCount = 0;   // refilled below, only from a frame that updated successfully

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
        LOGI("ArCameraUpdate alive: %d frames, update status %d", frameCount, static_cast<int>(updateStatus));
    }
    
    if(updateStatus != AR_SUCCESS)
    {
        return;
    }

    frameUpdated = true;

    if(sessionList != nullptr)
    {
        ArSession_getAllTrackables(session, AR_TRACKABLE_PLANE, sessionList);   // fill the list first
        int32_t n{};
        ArTrackableList_getSize(session, sessionList, &n);                      // then count it
        planeCount = n;
        static int32_t lastPlaneCount = -1;
        if(n != lastPlaneCount)
        {
            lastPlaneCount = n;
            LOGI("Planes detected: (%d)", n);
        }

        // About once a second, say what each plane is, so we can compare ARCore's idea of a plane with the real room.
        // y is the height in ARCore's world (0 = where the phone was when the app started, negative = below that).
        static int planeLogFrames = 0;
        const bool logPlanes = (n > 0 && ++planeLogFrames % 60 == 0);

        if(n > 0 && planeCentrePose == nullptr)
        {
            ArPose_create(session, nullptr, &planeCentrePose);
        }

        for(int32_t i = 0; i < n; ++i)
        {
            ArTrackable* trackable = nullptr;
            ArTrackableList_acquireItem(session, sessionList, i, &trackable);
            ArPlane* plane = ArAsPlane(trackable);
            ArPlane* parent = nullptr;
            ArPlane_acquireSubsumedBy(session, plane, &parent);
            ArPlaneType planeType = AR_PLANE_VERTICAL;
            ArPlane_getType(session, plane, &planeType);
            ArTrackingState planeState = AR_TRACKING_STATE_STOPPED;
            ArTrackable_getTrackingState(session, trackable, &planeState);
            float extentX = 0.0f;
            float extentZ = 0.0f;
            ArPlane_getExtentX(session, plane, &extentX);
            ArPlane_getExtentZ(session, plane, &extentZ);
            ArPlane_getCenterPose(session, plane, planeCentrePose);

            // Worth drawing: a floor or table that ARCore is tracking right now, not merged into another, with a real size.
            if(parent == nullptr && planeType == AR_PLANE_HORIZONTAL_UPWARD_FACING && planeState == AR_TRACKING_STATE_TRACKING
               && extentX > 0.02f && extentZ > 0.02f && drawPlaneCount < maxDrawPlanes)
            {
                ArPose_getMatrix(session, planeCentrePose, drawPlaneMatrix[drawPlaneCount]);
                drawPlaneExtent[drawPlaneCount][0] = extentX;
                drawPlaneExtent[drawPlaneCount][1] = extentZ;
                ++drawPlaneCount;
            }

            if(logPlanes)
            {
                float raw[7];
                ArPose_getPoseRaw(session, planeCentrePose, raw);
                LOGI("  Plane %d: %s, %s%s, y %.2f m, size %.2f x %.2f m", i,
                     planeType == AR_PLANE_HORIZONTAL_UPWARD_FACING ? "horizontal, faces up" : planeType == AR_PLANE_HORIZONTAL_DOWNWARD_FACING ? "horizontal, faces down" : "vertical",
                     planeState == AR_TRACKING_STATE_TRACKING ? "tracked" : "not tracked", parent != nullptr ? ", merged into another" : "",
                     raw[5], extentX, extentZ);
            }
            if(parent != nullptr)
            {
                ArTrackable_release(ArAsTrackable(parent));
            }
            ArTrackable_release(trackable);
        }
    }
}

/**
 * @brief   Clears the screen and draws the camera picture.
 * @details This is the back layer, drawn before the equipment by Core::Render. It shows the frame
 *          that ArCameraUpdate fetched this frame. If that update failed, only the clear happens.
 */
void ArCameraDraw()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if(frameUpdated)
    {
        CameraDraw();
    }
}

/**
 * @brief   Remembers the new screen size and rotation for ARCore.
 * @details Called when the phone's width, height or rotation changes.
 *          ARCore is told later, from ArCameraUpdate.
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
 *          used every frame by CameraDraw.
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
static void CameraDraw()
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
 * @brief   Puts the anchor on a table or floor, once the middle of the screen has pointed at one steadily.
 * @details Called every tracked frame while there is no anchor yet (the "scanning" state). A hit test shoots a
 *          ray from the camera through the middle of the screen and lists what ARCore has found along it, nearest
 *          first. The list can start with loose feature points, so it is walked until the first plane. That plane
 *          counts only if it is tracked, not merged into another plane, upward-facing, within maxPlaceDistance,
 *          and the ray lands inside its edge. It must count for framesToHold frames in a row before the anchor is
 *          created there, so a plane the camera only sweeps across by accident is not used.
 * @param[in] camera  The camera of this frame (used to report how far below the phone the plane is).
 */
static void TryPlaceAnchorOnPlane(ArCamera* camera)
{
    if(hitList == nullptr)
    {
        ArHitResultList_create(session, &hitList);
        ArHitResult_create(session, &hitResult);
        ArPose_create(session, nullptr, &hitPose);
        ArPose_create(session, nullptr, &cameraPose);
    }

    ArFrame_hitTest(session, sessionFrame, displayWidth * 0.5f, displayHeight * 0.5f, hitList);
    int32_t count = 0;
    ArHitResultList_getSize(session, hitList, &count);

    const char* result = "no hit";
    float distance = 0.0f;
    float below = 0.0f;       // metres between the phone and the plane (positive = the plane is below the phone)
    float extentX = 0.0f;
    float extentZ = 0.0f;
    bool canPlace = false;
    bool foundPlane = false;

    for(int32_t i = 0; i < count && !foundPlane; ++i)
    {
        ArHitResultList_getItem(session, hitList, i, hitResult);

        ArTrackable* trackable = nullptr;
        ArHitResult_acquireTrackable(session, hitResult, &trackable);
        ArTrackableType type = AR_TRACKABLE_NOT_VALID;
        ArTrackable_getType(session, trackable, &type);

        if(type != AR_TRACKABLE_PLANE)
        {
            result = "only loose points so far, no plane";
            ArTrackable_release(trackable);
            continue;                          // a feature point can sort in front of a plane: keep looking
        }

        foundPlane = true;
        ArPlane* plane = ArAsPlane(trackable);
        ArPlane* parent = nullptr;
        ArPlane_acquireSubsumedBy(session, plane, &parent);
        ArPlaneType planeType = AR_PLANE_VERTICAL;
        ArPlane_getType(session, plane, &planeType);
        ArTrackingState planeState = AR_TRACKING_STATE_STOPPED;
        ArTrackable_getTrackingState(session, trackable, &planeState);
        ArHitResult_getDistance(session, hitResult, &distance);
        ArHitResult_getHitPose(session, hitResult, hitPose);
        int32_t inside = 0;
        ArPlane_isPoseInPolygon(session, plane, hitPose, &inside);
        ArPlane_getExtentX(session, plane, &extentX);
        ArPlane_getExtentZ(session, plane, &extentZ);

        float hitRaw[7];
        ArPose_getPoseRaw(session, hitPose, hitRaw);
        ArCamera_getPose(session, camera, cameraPose);
        float cameraRaw[7];
        ArPose_getPoseRaw(session, cameraPose, cameraRaw);
        below = cameraRaw[5] - hitRaw[5];      // y is up in ARCore's world

        if(parent != nullptr)
        {
            result = "a plane that was merged into another one";
            ArTrackable_release(ArAsTrackable(parent));
        }
        else if(planeType != AR_PLANE_HORIZONTAL_UPWARD_FACING)
        {
            result = "a plane, but not a floor or table";
        }
        else if(planeState != AR_TRACKING_STATE_TRACKING)
        {
            result = "horizontal plane, not tracked right now";
        }
        else if(!inside)
        {
            result = "horizontal plane, outside its edge";
        }
        else if(distance > maxPlaceDistance)
        {
            result = "horizontal plane, too far away";
        }
        else
        {
            result = "horizontal plane, inside its edge";
            canPlace = true;
        }
        ArTrackable_release(trackable);
    }

    markerState = 0;
    if(foundPlane)
    {
        ArPose_getMatrix(session, hitPose, markerMatrix);   // hitPose holds the touch point of the plane found above
        markerState = canPlace ? 2 : 1;
        hitDistance = distance;
        hitBelow = below;
    }

    static const char* lastResult = nullptr;
    if(result != lastResult)
    {
        lastResult = result;
        LOGI("Centre ray: %s", result);
    }
    placementResult = result;

    validHitFrames = canPlace ? validHitFrames + 1 : 0;
    if(validHitFrames < framesToHold)
    {
        return;
    }

    const ArStatus status = ArHitResult_acquireNewAnchor(session, hitResult, &anchor);
    if(status != AR_SUCCESS)
    {
        LOGI("Anchor failed to create (%d)", static_cast<int>(status));
        anchor = nullptr;
        validHitFrames = 0;
    }
    else
    {
        LOGI("Anchor created on a plane: %.2f m away, %.2f m below the phone, plane size %.2f x %.2f m", distance, below, extentX, extentZ);
    }
}

/**
 * @brief   Gives the phone camera's view and projection matrices from the latest frame.
 * @details Call after ArCameraUpdate. Only valid while ARCore is tracking, and for a few
 *          frames after it drops (the last good camera is held, so the equipment does
 *          not blink when tracking flickers).
 * @param[out] view        16 floats, in the layout OpenGL uses (column-major).
 * @param[out] projection  16 floats, same layout.
 * @return  true if both were written, otherwise false (nothing is written).
 */
bool ArGetCameraMatrices(float* view, float* projection)
{
    if(session == nullptr || sessionFrame == nullptr || frameUpdated == false)
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

    cameraTrackingState = static_cast<int>(trackingState);

    static int lastTrackingState = -1;
    if(static_cast<int>(trackingState) != lastTrackingState)
    {
        lastTrackingState = static_cast<int>(trackingState);
        ArTrackingFailureReason failureReason = AR_TRACKING_FAILURE_REASON_NONE;
        ArCamera_getTrackingFailureReason(session, camera, &failureReason);
        LOGI("Tracking state: %s (failure reason %d)", trackingState == AR_TRACKING_STATE_TRACKING ? "TRACKING" : trackingState == AR_TRACKING_STATE_PAUSED   ? "PAUSED" : "STOPPED", static_cast<int>(failureReason));
    }

    if(trackingState == AR_TRACKING_STATE_TRACKING)
    {
        ArCamera_getViewMatrix(session, camera, lastView);
        ArCamera_getProjectionMatrix(session, camera, 0.05f, 100.0f, lastProjection);
        framesSinceTracking = 0;
        if(anchor != nullptr)
        {
            ArTrackingState anchorState = AR_TRACKING_STATE_STOPPED;
            ArAnchor_getTrackingState(session, anchor, &anchorState);
            static int lastAnchorState = -1;
            if(static_cast<int>(anchorState) != lastAnchorState)
            {
                lastAnchorState = static_cast<int>(anchorState);
                LOGI("Anchor state: %s", anchorState == AR_TRACKING_STATE_TRACKING ? "TRACKING" : anchorState == AR_TRACKING_STATE_PAUSED ? "PAUSED" : "STOPPED");
            }
            if(anchorState == AR_TRACKING_STATE_TRACKING)
            {
                if(anchorPoseHandle == nullptr)
                {
                    ArPose_create(session, nullptr, &anchorPoseHandle);   // nullptr = start as the identity pose
                }
                ArAnchor_getPose(session, anchor, anchorPoseHandle);        // fill the handle with the anchor's current pose
                ArPose_getMatrix(session, anchorPoseHandle, lastAnchorMatrix);
                anchorPoseValid = true;
                float raw[7];
                ArPose_getPoseRaw(session, anchorPoseHandle, raw);       // qx qy qz qw tx ty tz
                static int anchorLogCount = 0;
                if(++anchorLogCount % 60 == 0)
                {
                    LOGI("Anchor at (%.3f, %.3f, %.3f)", raw[4], raw[5], raw[6]);
                }
            }
        }
        else
        {
            TryPlaceAnchorOnPlane(camera);   // scanning: wait until the middle of the screen steadily points at a table or floor
        }
    }
    else if(framesSinceTracking < 1000)
    {
        ++framesSinceTracking;
    }
    if(trackingState != AR_TRACKING_STATE_TRACKING)
    {
        markerState = 0;
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

bool ArGetAnchorPose(float* matrix)
{
    if(!anchorPoseValid)
    {
        return false;
    }

    std::memcpy(matrix, lastAnchorMatrix, sizeof(lastAnchorMatrix));
    return true;
}

/**
 * @brief   Says in a few words what the plane placement is doing, for the on-screen text.
 * @details Scanning (no anchor yet), holding steady on a good plane, or placed. Call it on the drawing thread.
 * @return  A short text. It stays valid until the next call.
 */
const char* ArGetPlacementStatus()
{
    static char text[96];

    if(session == nullptr)
    {
        return "AR: no session";
    }
    if(anchor != nullptr)
    {
        return "AR: PLACED on a plane";
    }
    if(cameraTrackingState != static_cast<int>(AR_TRACKING_STATE_TRACKING))
    {
        return "AR: scanning - tracking lost, move slowly";
    }
    if(placementResult == nullptr || std::strcmp(placementResult, "no hit") == 0)
    {
        if(planeCount > 0 && drawPlaneCount == 0)
        {
            std::snprintf(text, sizeof(text), "AR: scanning - %d plane(s) seen but none tracked yet", planeCount);
        }
        else if(drawPlaneCount > 0)
        {
            std::snprintf(text, sizeof(text), "AR: scanning - %d usable plane(s), aim the centre at one", drawPlaneCount);
        }
        else
        {
            std::snprintf(text, sizeof(text), "AR: scanning - NO plane found yet, move sideways slowly");
        }
        return text;
    }
    if(std::strcmp(placementResult, "horizontal plane, inside its edge") == 0)
    {
        std::snprintf(text, sizeof(text), "AR: PLANE FOUND %.1f m away, %.1f m below - hold steady (%d/%d)", hitDistance, hitBelow, validHitFrames, framesToHold);
        return text;
    }
    if(markerState != 0)
    {
        std::snprintf(text, sizeof(text), "AR: scanning - %s (%.1f m away, %.1f m below)", placementResult, hitDistance, hitBelow);
        return text;
    }
    std::snprintf(text, sizeof(text), "AR: scanning - %s", placementResult);
    return text;
}

/**
 * @brief   Gives the spot to draw the small debug square: where the middle of the screen touches a plane.
 * @details While scanning it follows the ray (state 1 = a plane that cannot be used, 2 = a good plane, held steady
 *          before placing). Once the anchor exists it sits on the anchor (state 3), so you can see where the anchor is
 *          compared with the pieces. The square lies flat on the plane: +y is the plane's normal.
 * @param[out] matrix  16 floats, in the layout OpenGL uses (column-major). Not written when 0 is returned.
 * @return  0 = draw nothing, 1 = plane not usable, 2 = good plane, 3 = placed.
 */
int ArGetPlacementMarker(float* matrix)
{
    if(anchor != nullptr)
    {
        if(!anchorPoseValid)
        {
            return 0;
        }
        std::memcpy(matrix, lastAnchorMatrix, sizeof(lastAnchorMatrix));
        return 3;
    }
    if(markerState == 0)
    {
        return 0;
    }
    std::memcpy(matrix, markerMatrix, sizeof(markerMatrix));
    return markerState;
}

/**
 * @brief   Gives the planes worth drawing as a see-through rectangle (floor or table, tracked right now, with a size).
 * @details Each plane comes as its centre pose (+y is the plane's normal) and its size along its own x and z.
 *          Empty when ARCore is not tracking or has found nothing usable.
 * @param[out] matrices  maxCount * 16 floats, each in the layout OpenGL uses (column-major).
 * @param[out] extents   maxCount * 2 floats: the size along the plane's x, then z, in metres.
 * @param[in]  maxCount  How many planes the caller has room for.
 * @return  How many planes were written.
 */
int ArGetPlanes(float* matrices, float* extents, int maxCount)
{
    const int count = drawPlaneCount < maxCount ? drawPlaneCount : maxCount;
    std::memcpy(matrices, drawPlaneMatrix, sizeof(float) * 16 * static_cast<std::size_t>(count));
    std::memcpy(extents, drawPlaneExtent, sizeof(float) * 2 * static_cast<std::size_t>(count));
    return count;
}