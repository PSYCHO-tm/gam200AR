#include "ARcore.hpp"
#include "arcore_c_api.h"
#include "Profiler.hpp"
#include <android/log.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

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
static const bool placeOnTables = false;      // false = only the printed card places the station (planes are still found and drawn)
static const int framesToHold = 20;         // about 0.7 s at 30 fps: hold steady this long before the anchor is placed
static const float maxPlaceDistance = 3.0f;  // metres: ignore planes further away than this
static float lastAnchorMatrix[16]{};
static bool anchorPoseValid = false;

//Printed card (ARCore Augmented Image): places the anchor, or moves a table-placed anchor onto the card
static std::vector<uint8_t> cardPixels;                 // grayscale, from Kotlin; freed once the session is configured
static int cardWidthPixels = 0;
static int cardHeightPixels = 0;
static float cardWidthMetres = 0.0f;                    // the printed width: ARCore uses it for the card's real size
static ArAugmentedImageDatabase* cardDatabase = nullptr;
static ArTrackableList* cardList = nullptr;             // own list: sessionList is used for planes
static ArPose* cardPose = nullptr;
static ArPose* cardCameraPose = nullptr;                // where the phone is, to know how far away the card is
static ArPose* cardAnchorPose = nullptr;                // where the current anchor is, to compare with the card
static int cardSteadyFrames = 0;                        // tracked frames in a row the card has been fully in view
static const float cardMaxDistance = 0.6f;              // metres: further away, ARCore's guess of the card's depth is too rough to use
static const float cardRefineDistance = 0.01f;          // metres: move the anchor when the card's latest pose is this far from it
static const float cardRefineAngle = 0.035f;            // radians (about 2 degrees): or when it is turned this much
static bool cardFollowsTilt = false;                    // true = the station tilts with the card (wall poster), false = always upright (counter); from AR.json
static bool anchorOnCard = false;                       // true once the anchor sits on the card: no more snapping
static bool cardInView = false;                         // on-screen debug: the card is fully seen this frame
static float cardDistanceNow = 0.0f;                    // on-screen debug: metres from the phone to the card, last seen
static int cardRefineCount = 0;                         // on-screen debug: how often the anchor followed the card
static float cardViewAngle = 0.0f;                      // on-screen debug: degrees off straight-on the phone sees the card, last seen
static char cameraModeText[128] = "default (not chosen)";   // sandbox: which camera config was picked, logged again later
static bool anchorTracking = false;                    // the anchor's own tracking state this frame (paused = do not trust it)

// Depth (M2G05 extra): ARCore's depth picture hides the pieces behind real things (a hand), and says when the card is covered.
static bool depthEnabled = false;                       // the phone supports depth and the session has it on
static int depthTextureW = 0;                           // size depthTexture was allocated at (0 = not yet)
static int depthTextureH = 0;
static GLuint depthTexture = 0;                         // this frame's depth picture: millimetres, low byte in R, high byte in G
static bool depthValid = false;                         // depthTexture holds this frame's picture
static float depthUvTransform[9]{};                     // screen (0..1) -> depth picture (0..1), column-major 3x3
static float cardMatrix[16]{};                          // the card's latest pose (OpenGL layout), +y out of the card
static float cardExtentX = 0.0f;                        // the card's size in metres, as ARCore sees it
static float cardExtentZ = 0.0f;
static bool cardPoseKnown = false;
static float cardCoveredFraction = -1.0f;               // 0..1 of the card hidden by something closer, -1 = cannot tell
static int cardCoverFrames = 0;                         // frames in a row past the threshold that would flip cardCovered
static bool cardCovered = false;                        // true = hide the station and show the loader
static const float coverHide = 0.75f;                   // hide the station once this much of the card is covered
static const float coverShow = 0.6f;                    // show it again once less than this is covered (no flicker in between)
static const int coverHoldFrames = 6;                   // about 0.2 s: one noisy depth frame does not flip it
static const float coverMargin = 0.03f;                 // metres: something this much closer than the card is covering it
static const float cardClearFraction = 0.15f;           // the card counts as uncovered (safe to follow) below this
static const int cardHistorySize = 8;                   // about 0.25 s of card readings, averaged before the anchor moves
static float cardHistory[cardHistorySize][4]{};         // x, y, z, yaw of each reading
static int cardHistoryNext = 0;
static int cardHistoryCount = 0;
static const float cardSteadySpread = 0.015f;           // metres: every reading within this of the average, or do not move
static const float cardSteadyAngle = 0.052f;            // radians (about 3 degrees): same for the turn
static bool cameraBlocked = false;                      // most of the camera is covered (hand on the lens): hide the station
static int cameraBlockedFrames = 0;
static const float blockedFraction = 0.7f;              // this much of the depth picture unknown or very close = blocked
static const int clearHoldFrames = 15;                  // about 0.5 s of a clear camera before a blocked one counts as clear
static const float blockedNear = 0.2f;                 // metres: closer than this counts as "on the lens"

//AR Camera Draw
static int displayWidth = 1;
static int displayHeight = 1;
static int displayRotation = 0;
static bool geometryDirty = true;

//OpenGL Camera
static void CameraDrawingInit();
static void CameraDraw();
static void TryPlaceAnchorOnCard(ArCamera* camera);
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
 * @brief   Keeps a copy of the printed card's picture until the session is configured.
 * @details Call before ArSessionCreate. The copy is freed once ArSessionCreate has
 *          added it to ARCore.
 * @param[in] grayscale    width * height bytes, one per pixel, rows top to bottom.
 * @param[in] width        The image width in pixels.
 * @param[in] height       The image height in pixels.
 */
void ArSetMarkerImage(const uint8_t* grayscale, int width, int height)
{
    cardPixels.assign(grayscale, grayscale + static_cast<size_t>(width) * static_cast<size_t>(height));
    cardWidthPixels = width;
    cardHeightPixels = height;
}

/**
 * @brief   The printed card's settings, read from AR.json by the JNI bridge.
 * @param[in] widthMetres  The printed card's real width in metres.
 * @param[in] followTilt   true = the station tilts with the card, false = it always stands upright.
 */
void ArSetMarkerConfig(float widthMetres, bool followTilt)
{
    cardWidthMetres = widthMetres;
    cardFollowsTilt = followTilt;
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
        if(!cardPixels.empty() && cardDatabase == nullptr)
        {
            ArAugmentedImageDatabase_create(session, &cardDatabase);
            int32_t cardIndex = 0;
            const ArStatus cardStatus = ArAugmentedImageDatabase_addImageWithPhysicalSize(session, cardDatabase, "station",
                cardPixels.data(), cardWidthPixels, cardHeightPixels, cardWidthPixels, cardWidthMetres, &cardIndex);
            if(cardStatus == AR_SUCCESS)
            {
                ArConfig_setAugmentedImageDatabase(session, sessionConfig, cardDatabase);
                LOGI("Marker card added (%dx%d px, %.3f m wide)", cardWidthPixels, cardHeightPixels, cardWidthMetres);
            }
            else
            {
                LOGE("Marker card rejected by ARCore (%d): placing on tables only", static_cast<int>(cardStatus));
                ArAugmentedImageDatabase_destroy(cardDatabase);
                cardDatabase = nullptr;
            }
            cardPixels.clear();
            cardPixels.shrink_to_fit();
        }
        // Sandbox optimisation: the app is camera-bound (measured: ~31.7 of ~32 ms per frame is waiting for the
        // camera, our own work is ~0.7 ms), so the only way to a smoother picture is a faster camera. Ask ARCore for
        // a 60 fps camera; phones without one keep the default 30 fps. Must happen before the session first resumes.
        {
            ArCameraConfigFilter* fpsFilter = nullptr;
            ArCameraConfigList* fpsConfigs = nullptr;
            ArCameraConfigFilter_create(session, &fpsFilter);
            ArCameraConfigFilter_setTargetFps(session, fpsFilter, AR_CAMERA_CONFIG_TARGET_FPS_60);
            ArCameraConfigList_create(session, &fpsConfigs);
            ArSession_getSupportedCameraConfigsWithFilter(session, fpsFilter, fpsConfigs);
            int32_t fpsCount = 0;
            ArCameraConfigList_getSize(session, fpsConfigs, &fpsCount);
            if(fpsCount > 0)
            {
                ArCameraConfig* fastCamera = nullptr;
                ArCameraConfig_create(session, &fastCamera);
                ArCameraConfigList_getItem(session, fpsConfigs, 0, fastCamera);   // ARCore lists the best one first
                int32_t minFps = 0;
                int32_t maxFps = 0;
                int32_t imageW = 0;
                int32_t imageH = 0;
                ArCameraConfig_getFpsRange(session, fastCamera, &minFps, &maxFps);
                ArCameraConfig_getImageDimensions(session, fastCamera, &imageW, &imageH);
                const ArStatus fpsStatus = ArSession_setCameraConfig(session, fastCamera);
                std::snprintf(cameraModeText, sizeof(cameraModeText), "%d 60 fps configs; using %dx%d at %d-%d fps (status %d)",
                              fpsCount, imageW, imageH, minFps, maxFps, static_cast<int>(fpsStatus));
                LOGI("Camera: %s", cameraModeText);
                ArCameraConfig_destroy(fastCamera);
            }
            else
            {
                std::snprintf(cameraModeText, sizeof(cameraModeText), "no 60 fps config on this phone, staying at 30 fps");
                LOGI("Camera: %s", cameraModeText);
            }
            ArCameraConfigList_destroy(fpsConfigs);
            ArCameraConfigFilter_destroy(fpsFilter);
        }

        int32_t depthSupported = 0;
        ArSession_isDepthModeSupported(session, AR_DEPTH_MODE_AUTOMATIC, &depthSupported);
        depthEnabled = depthSupported != 0;
        ArConfig_setDepthMode(session, sessionConfig, depthEnabled ? AR_DEPTH_MODE_AUTOMATIC : AR_DEPTH_MODE_DISABLED);
        LOGI("Depth: %s", depthEnabled ? "on (real things hide the pieces)" : "not supported on this phone");
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

    // Tracking was off, so the room may not line up any more: let the card snap the station again.
    // The current anchor stays until then, so the station is still shown if the card is out of view.
    anchorOnCard = false;
    cardSteadyFrames = 0;
    cardInView = false;
    cardCovered = false;
    cardCoverFrames = 0;
    cardHistoryCount = 0;
    cameraBlocked = false;
    cameraBlockedFrames = 0;
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
    anchorOnCard = false;
    cardSteadyFrames = 0;
    cardInView = false;
    cardRefineCount = 0;
    anchorTracking = false;
    cardPoseKnown = false;
    cardCovered = false;
    cardCoverFrames = 0;
    cardCoveredFraction = -1.0f;
    depthValid = false;
    depthEnabled = false;
    cardHistoryCount = 0;
    cameraBlocked = false;
    cameraBlockedFrames = 0;

    if(cardList != nullptr)
    {
        ArTrackableList_destroy(cardList);
    }
    cardList = nullptr;

    if(cardPose != nullptr)
    {
        ArPose_destroy(cardPose);
        ArPose_destroy(cardCameraPose);
        ArPose_destroy(cardAnchorPose);
    }
    cardPose = nullptr;
    cardCameraPose = nullptr;
    cardAnchorPose = nullptr;

    if(cardDatabase != nullptr)
    {
        ArAugmentedImageDatabase_destroy(cardDatabase);
    }
    cardDatabase = nullptr;

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

    // The depth picture: NEAREST, because each pixel is two bytes of one number and must not be blended with its neighbours.
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    depthValid = false;
    depthTextureW = 0;   // a new texture: the first upload must allocate it
    depthTextureH = 0;

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
    ArStatus updateStatus;
    {
        PROFILE_SCOPE("AR wait");   // blocks until the camera's next frame: waiting, not work
        updateStatus = ArSession_update(session, sessionFrame);
    }
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

    // Sandbox optimisation: card-only placement never uses the planes, so do not walk (and log) them every frame.
    // The plane logging was also flooding Logcat, which then dropped important lines.
    if(sessionList != nullptr && placeOnTables)
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
    PROFILE_SCOPE("Camera draw");
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
 * @brief   Places the anchor on the printed card once the camera has seen it fully for framesToHold frames.
 * @details Does nothing without a card or once the anchor is on it. If the station was already
 *          placed on a table, that anchor is replaced (the station snaps onto the card).
 * @param[in] camera  The camera of the current frame (used for the distance in the log).
 */
static void TryPlaceAnchorOnCard(ArCamera* camera)
{
    PROFILE_SCOPE("AR card");
    if(cardDatabase == nullptr)
    {
        return;
    }
    if(cardList == nullptr)
    {
        ArTrackableList_create(session, &cardList);
        ArPose_create(session, nullptr, &cardPose);
        ArPose_create(session, nullptr, &cardCameraPose);
        ArPose_create(session, nullptr, &cardAnchorPose);
    }

    ArSession_getAllTrackables(session, AR_TRACKABLE_AUGMENTED_IMAGE, cardList);
    int32_t count = 0;
    ArTrackableList_getSize(session, cardList, &count);

    bool fullyInView = false;
    for(int32_t i = 0; i < count; ++i)
    {
        ArTrackable* trackable = nullptr;
        ArTrackableList_acquireItem(session, cardList, i, &trackable);

        ArTrackingState state = AR_TRACKING_STATE_STOPPED;
        ArTrackable_getTrackingState(session, trackable, &state);
        ArAugmentedImageTrackingMethod method = AR_AUGMENTED_IMAGE_TRACKING_METHOD_NOT_TRACKING;
        ArAugmentedImage_getTrackingMethod(session, ArAsAugmentedImage(trackable), &method);

        // Seen now or last known: where the card is, so the depth check can tell when a hand covers it.
        if(state == AR_TRACKING_STATE_TRACKING)
        {
            ArAugmentedImage_getCenterPose(session, ArAsAugmentedImage(trackable), cardPose);
            ArPose_getMatrix(session, cardPose, cardMatrix);
            ArAugmentedImage_getExtentX(session, ArAsAugmentedImage(trackable), &cardExtentX);
            ArAugmentedImage_getExtentZ(session, ArAsAugmentedImage(trackable), &cardExtentZ);
            cardPoseKnown = true;
        }

        // FULL_TRACKING = the camera sees the card right now (LAST_KNOWN_POSE would be a guess)
        if(state == AR_TRACKING_STATE_TRACKING && method == AR_AUGMENTED_IMAGE_TRACKING_METHOD_FULL_TRACKING)
        {
            fullyInView = true;
            ++cardSteadyFrames;

            // ARCore's guess of the card's depth gets better up close and as the phone moves around it,
            // so the anchor follows the card's latest pose while it is in view and close enough.
            float cardRaw[7];
            float cameraRaw[7];
            ArAugmentedImage_getCenterPose(session, ArAsAugmentedImage(trackable), cardPose);   // +y points out of the card
            ArPose_getPoseRaw(session, cardPose, cardRaw);       // qx qy qz qw tx ty tz
            ArCamera_getPose(session, camera, cardCameraPose);
            ArPose_getPoseRaw(session, cardCameraPose, cameraRaw);
            const float cx = cardRaw[4] - cameraRaw[4];
            const float cy = cardRaw[5] - cameraRaw[5];
            const float cz = cardRaw[6] - cameraRaw[6];
            const float cardDistance = std::sqrt(cx * cx + cy * cy + cz * cz);
            cardDistanceNow = cardDistance;

            // On-screen debug: the viewing angle, between the card's face direction (+y) and the line to the phone.
            // 0 = straight on, 90 = edge on.
            {
                const float px = cardRaw[0], py = cardRaw[1], pz = cardRaw[2], pw = cardRaw[3];
                const float faceX = 2.0f * (px * py - pw * pz);
                const float faceY = 1.0f - 2.0f * (px * px + pz * pz);
                const float faceZ = 2.0f * (py * pz + pw * px);
                const float toPhone = -(faceX * cx + faceY * cy + faceZ * cz) / (cardDistance > 0.001f ? cardDistance : 0.001f);
                cardViewAngle = std::acos(toPhone < -1.0f ? -1.0f : (toPhone > 1.0f ? 1.0f : toPhone)) * 57.2958f;
            }

            // The station stands upright whatever the card's tilt: keep the card's centre and its turn around the
            // vertical, but use the real world's up (+y), so a card on a tilted or upright screen still works.
            const float qx = cardRaw[0], qy = cardRaw[1], qz = cardRaw[2], qw = cardRaw[3];
            const float cardXx = 1.0f - 2.0f * (qy * qy + qz * qz);   // the card's +x axis in the world, x and z parts
            const float cardXz = 2.0f * (qx * qz - qw * qy);
            const float cardZx = 2.0f * (qx * qz + qw * qy);          // the card's +z axis, for a card turned so x is vertical
            const float cardZz = 1.0f - 2.0f * (qx * qx + qy * qy);
            const float yaw = (cardXx * cardXx + cardXz * cardXz > 0.01f) ? std::atan2(-cardXz, cardXx)
                                                                           : std::atan2(cardZx, cardZz);
            float uprightRaw[7] = {0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f),
                                   cardRaw[4], cardRaw[5], cardRaw[6]};
            if(cardFollowsTilt)
            {
                std::memcpy(uprightRaw, cardRaw, sizeof(uprightRaw));   // the card's full pose: +y out of the card
            }

            // Smoothing and a sanity check: keep the last cardHistorySize readings and use their average, but only
            // when they all agree. A hand sliding over the card makes ARCore's readings jump around (15-24 cm seen),
            // and those must never move the station.
            cardHistory[cardHistoryNext][0] = uprightRaw[4];
            cardHistory[cardHistoryNext][1] = uprightRaw[5];
            cardHistory[cardHistoryNext][2] = uprightRaw[6];
            cardHistory[cardHistoryNext][3] = yaw;
            cardHistoryNext = (cardHistoryNext + 1) % cardHistorySize;
            if(cardHistoryCount < cardHistorySize)
            {
                ++cardHistoryCount;
            }
            bool cardSteady = false;
            if(cardHistoryCount == cardHistorySize)
            {
                float avg[3] = {0.0f, 0.0f, 0.0f};
                float yawSin = 0.0f;
                float yawCos = 0.0f;
                for(int h = 0; h < cardHistorySize; ++h)
                {
                    avg[0] += cardHistory[h][0] / cardHistorySize;
                    avg[1] += cardHistory[h][1] / cardHistorySize;
                    avg[2] += cardHistory[h][2] / cardHistorySize;
                    yawSin += std::sin(cardHistory[h][3]);
                    yawCos += std::cos(cardHistory[h][3]);
                }
                const float avgYaw = std::atan2(yawSin, yawCos);
                float spread = 0.0f;       // the reading furthest from the average, in metres
                float yawSpread = 0.0f;    // and in radians
                for(int h = 0; h < cardHistorySize; ++h)
                {
                    const float dx = cardHistory[h][0] - avg[0];
                    const float dy = cardHistory[h][1] - avg[1];
                    const float dz = cardHistory[h][2] - avg[2];
                    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                    const float a = std::fabs(std::remainder(cardHistory[h][3] - avgYaw, 6.2831853f));
                    spread = d > spread ? d : spread;
                    yawSpread = a > yawSpread ? a : yawSpread;
                }
                cardSteady = spread <= cardSteadySpread && yawSpread <= cardSteadyAngle;
                if(cardSteady)   // move to the average, not the latest (noisy) reading
                {
                    uprightRaw[4] = avg[0];
                    uprightRaw[5] = avg[1];
                    uprightRaw[6] = avg[2];
                    if(!cardFollowsTilt)
                    {
                        uprightRaw[1] = std::sin(avgYaw * 0.5f);
                        uprightRaw[3] = std::cos(avgYaw * 0.5f);
                    }
                }
            }
            // With depth, only trust the card when it is clearly uncovered ("cannot tell" is not trusted either:
            // a hand right at the lens reads as unknown depth). Without depth on this phone, the steadiness check alone.
            const bool cardClear = !depthEnabled || (cardCoveredFraction >= 0.0f && cardCoveredFraction < cardClearFraction);
            if(anchorOnCard && (!cardClear || cardCovered || cameraBlocked))
            {
                // Readings taken with a hand over the card can agree with each other and still be wrong (10 cm seen
                // right after uncovering): forget them, so the average restarts from clean readings only.
                cardHistoryCount = 0;
                cardSteady = false;
            }

            bool moveAnchor = false;
            if(!anchorOnCard)
            {
                moveAnchor = cardSteadyFrames >= framesToHold && cardDistance <= cardMaxDistance && cardSteady;
            }
            else if(cardDistance <= cardMaxDistance && anchor != nullptr && cardSteady && cardClear && !cardCovered &&
                    !cameraBlocked)
            {
                float anchorRaw[7];
                ArAnchor_getPose(session, anchor, cardAnchorPose);
                ArPose_getPoseRaw(session, cardAnchorPose, anchorRaw);
                const float ax = uprightRaw[4] - anchorRaw[4];
                const float ay = uprightRaw[5] - anchorRaw[5];
                const float az = uprightRaw[6] - anchorRaw[6];
                const float shift = std::sqrt(ax * ax + ay * ay + az * az);
                const float dot = std::fabs(uprightRaw[0] * anchorRaw[0] + uprightRaw[1] * anchorRaw[1] +
                                            uprightRaw[2] * anchorRaw[2] + uprightRaw[3] * anchorRaw[3]);
                const float turn = 2.0f * std::acos(dot < 1.0f ? dot : 1.0f);
                if(shift > cardRefineDistance || turn > cardRefineAngle)
                {
                    moveAnchor = true;
                    ++cardRefineCount;
                    LOGI("Marker card: refined %.1f cm, %.1f deg, %.2f m away (dx %.3f dy %.3f dz %.3f)", shift * 100.0f,
                         turn * 57.2958f, cardDistance, ax, ay, az);
                }
            }

            if(moveAnchor)
            {
                ArAnchor* cardAnchor = nullptr;
                ArPose* uprightPose = nullptr;
                ArPose_create(session, uprightRaw, &uprightPose);
                // A world anchor, not one tied to the card: a card-tied anchor copies every small wobble in ARCore's guess
                // of the card's tilt, so the "upright" station tipped a few degrees and was re-placed almost every frame.
                // The refinement above still moves it whenever the card really moves (1 cm / 2 degrees).
                const ArStatus anchorStatus = ArSession_acquireNewAnchor(session, uprightPose, &cardAnchor);
                ArPose_destroy(uprightPose);
                if(anchorStatus == AR_SUCCESS)
                {
                    const bool firstOnCard = !anchorOnCard;
                    if(anchor != nullptr)   // a table anchor, an old card anchor after Resume, or a refinement
                    {
                        ArAnchor_detach(session, anchor);
                        ArAnchor_release(anchor);
                    }
                    if(firstOnCard)
                    {
                        LOGI("Marker card found: station %s the card, %.2f m away",
                             anchor != nullptr ? "moved onto" : "placed on", cardDistance);
                    }
                    anchor = cardAnchor;
                    anchorOnCard = true;
                }
            }
        }
        ArTrackable_release(trackable);
    }

    cardInView = fullyInView;
    if(!fullyInView)
    {
        cardSteadyFrames = 0;
        cardHistoryCount = 0;   // start the average again from fresh readings
    }
}

/**
 * @brief   How much of the card something real (a hand, a cup) is in front of, from this frame's depth picture.
 * @details Checks an 8 x 8 grid of points on the card. A point counts when it is on screen and ARCore knows the depth
 *          there; it is covered when the real surface is more than coverMargin closer to the phone than the card is.
 *          Sets cardCoveredFraction to 0..1, or -1 when it cannot tell (card mostly off screen, or depth unknown).
 * @param[in] depth   The depth picture's bytes: 16-bit millimetres per pixel, rows rowStride bytes apart.
 */
static void MeasureCardCover(const uint8_t* depth, int width, int height, int rowStride)
{
    cardCoveredFraction = -1.0f;
    if(!anchorOnCard || !cardPoseKnown || cardExtentX <= 0.0f || cardExtentZ <= 0.0f)
    {
        return;
    }

    const int grid = 8;
    float ndc[grid * grid * 2];
    float expected[grid * grid];   // how far in front of the phone each card point is, in metres
    int onScreen = 0;
    for(int gz = 0; gz < grid; ++gz)
    {
        for(int gx = 0; gx < grid; ++gx)
        {
            // 90% of the card: its very edges pick up the table's depth too
            const float x = ((gx + 0.5f) / grid - 0.5f) * cardExtentX * 0.9f;
            const float z = ((gz + 0.5f) / grid - 0.5f) * cardExtentZ * 0.9f;
            const float* m = cardMatrix;   // column-major: card -> world
            const float world[3] = {m[0] * x + m[8] * z + m[12], m[1] * x + m[9] * z + m[13], m[2] * x + m[10] * z + m[14]};
            float viewPoint[4];
            for(int r = 0; r < 4; ++r)
            {
                viewPoint[r] = lastView[r] * world[0] + lastView[4 + r] * world[1] + lastView[8 + r] * world[2] + lastView[12 + r];
            }
            float clip[4];
            for(int r = 0; r < 4; ++r)
            {
                clip[r] = lastProjection[r] * viewPoint[0] + lastProjection[4 + r] * viewPoint[1] +
                          lastProjection[8 + r] * viewPoint[2] + lastProjection[12 + r] * viewPoint[3];
            }
            if(clip[3] <= 0.05f)
            {
                continue;   // behind the phone
            }
            const float nx = clip[0] / clip[3];
            const float ny = clip[1] / clip[3];
            if(nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f)
            {
                continue;   // off screen: the player is not looking at that part
            }
            ndc[onScreen * 2] = nx;
            ndc[onScreen * 2 + 1] = ny;
            expected[onScreen] = clip[3];   // a perspective camera's w is the distance in front of it
            ++onScreen;
        }
    }
    if(onScreen < grid * grid / 4)
    {
        return;   // less than a quarter of the card on screen: looking elsewhere, not covered
    }

    float uv[grid * grid * 2];
    ArFrame_transformCoordinates2d(session, sessionFrame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES, onScreen,
                                   ndc, AR_COORDINATES_2D_IMAGE_NORMALIZED, uv);
    int known = 0;
    int covered = 0;
    for(int i = 0; i < onScreen; ++i)
    {
        int px = static_cast<int>(uv[i * 2] * width);
        int py = static_cast<int>(uv[i * 2 + 1] * height);
        px = px < 0 ? 0 : (px >= width ? width - 1 : px);
        py = py < 0 ? 0 : (py >= height ? height - 1 : py);
        const uint8_t* pixel = depth + py * rowStride + px * 2;
        const int millimetres = pixel[0] | (pixel[1] << 8);
        if(millimetres == 0)
        {
            continue;   // ARCore does not know the depth here
        }
        ++known;
        if(millimetres * 0.001f < expected[i] - coverMargin)
        {
            ++covered;
        }
    }
    if(known < onScreen / 2)
    {
        return;   // too few known points to judge
    }
    cardCoveredFraction = static_cast<float>(covered) / static_cast<float>(known);
}

/**
 * @brief   Hides the station once the card has been mostly covered for a moment, and shows it again once it is uncovered.
 * @details Two thresholds (coverHide, coverShow) and coverHoldFrames in a row, so depth noise does not make it flicker.
 */
static void UpdateCardCovered()
{
    const bool flip = cardCovered ? cardCoveredFraction < coverShow : cardCoveredFraction >= coverHide;
    cardCoverFrames = flip ? cardCoverFrames + 1 : 0;
    if(cardCoverFrames >= coverHoldFrames)
    {
        cardCovered = !cardCovered;
        cardCoverFrames = 0;
        LOGI("Marker card %s (%.0f%% covered)", cardCovered ? "covered: station hidden" : "uncovered: station shown",
             cardCoveredFraction < 0.0f ? 0.0f : cardCoveredFraction * 100.0f);
    }
}

/**
 * @brief   Uploads this frame's depth picture for the lit shader and measures how much of the card is covered.
 * @details Call on the GL thread after lastView and lastProjection hold this frame's camera. Without depth (phone does
 *          not support it, or the first frames before ARCore has a picture) depthValid is false and nothing is hidden.
 */
static void UpdateDepth()
{
    PROFILE_SCOPE("AR depth");
    depthValid = false;
    cardCoveredFraction = -1.0f;
    if(anchor == nullptr)
    {
        cameraBlocked = false;
        cameraBlockedFrames = 0;
        return;   // sandbox optimisation: nothing to hide or check until the station is placed
    }
    if(depthEnabled && depthTexture != 0)
    {
        ArImage* depthImage = nullptr;
        if(ArFrame_acquireDepthImage16Bits(session, sessionFrame, &depthImage) == AR_SUCCESS && depthImage != nullptr)
        {
            int32_t width = 0;
            int32_t height = 0;
            int32_t rowStride = 0;
            int32_t length = 0;
            const uint8_t* data = nullptr;
            ArImage_getWidth(session, depthImage, &width);
            ArImage_getHeight(session, depthImage, &height);
            ArImage_getPlaneRowStride(session, depthImage, 0, &rowStride);
            ArImage_getPlaneData(session, depthImage, 0, &data, &length);
            if(data != nullptr && width > 0 && height > 0 && rowStride >= width * 2)
            {
                glBindTexture(GL_TEXTURE_2D, depthTexture);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, rowStride / 2);   // in pixels: rows may be padded
                // Sandbox optimisation: allocate the texture once (or when the size changes), then only update the
                // pixels, instead of making the driver re-create the texture every frame.
                if(width != depthTextureW || height != depthTextureH)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, data);
                    depthTextureW = width;
                    depthTextureH = height;
                }
                else
                {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RG, GL_UNSIGNED_BYTE, data);
                }
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glBindTexture(GL_TEXTURE_2D, 0);

                // Screen corners -> where they fall in the depth picture. The mapping is a straight-line one, so three
                // corners give it: column 0 = along screen x, column 1 = along screen y, column 2 = the bottom-left corner.
                const float ndc[6] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f};
                float uv[6];
                ArFrame_transformCoordinates2d(session, sessionFrame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES,
                                               3, ndc, AR_COORDINATES_2D_IMAGE_NORMALIZED, uv);
                const float transform[9] = {uv[2] - uv[0], uv[3] - uv[1], 0.0f,
                                            uv[4] - uv[0], uv[5] - uv[1], 0.0f,
                                            uv[0],         uv[1],         1.0f};
                std::memcpy(depthUvTransform, transform, sizeof(transform));
                depthValid = true;

                MeasureCardCover(data, width, height, rowStride);

                // Camera blocked: a hand on or right in front of the lens makes most of the depth picture unknown or
                // very close. ARCore may keep "tracking" from the last few features for a moment, so do not wait for it.
                int blocked = 0;
                int samples = 0;
                for(int sy = 0; sy < 9; ++sy)
                {
                    for(int sx = 0; sx < 16; ++sx)
                    {
                        const int px = (sx * 2 + 1) * width / 32;
                        const int py = (sy * 2 + 1) * height / 18;
                        const uint8_t* pixel = data + py * rowStride + px * 2;
                        const int millimetres = pixel[0] | (pixel[1] << 8);
                        blocked += (millimetres == 0 || millimetres * 0.001f < blockedNear) ? 1 : 0;
                        ++samples;
                    }
                }
                const bool looksBlocked = blocked >= static_cast<int>(blockedFraction * samples);
                cameraBlockedFrames = (looksBlocked != cameraBlocked) ? cameraBlockedFrames + 1 : 0;
                // Hide fast, show slowly: a palm near the lens gives a few frames of believable depth now and then,
                // so "clear" must last about half a second before the station comes back.
                if(cameraBlockedFrames >= (cameraBlocked ? clearHoldFrames : coverHoldFrames))
                {
                    cameraBlocked = looksBlocked;
                    cameraBlockedFrames = 0;
                    LOGI("Camera %s (%d%% of the depth picture unknown or under %.0f cm)",
                         cameraBlocked ? "blocked: station hidden" : "clear again", blocked * 100 / samples, blockedNear * 100.0f);
                }

                static bool loggedSize = false;
                if(!loggedSize)
                {
                    loggedSize = true;
                    LOGI("Depth picture: %d x %d; camera mode: %s; depth %s", width, height, cameraModeText,
                         depthEnabled ? "on" : "off");
                }
            }
            ArImage_release(depthImage);
        }
    }
    UpdateCardCovered();
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
        TryPlaceAnchorOnCard(camera);   // the printed card wins: it places the anchor, or moves a table-placed one onto the card
        UpdateDepth();                  // after the camera matrices above: the cover check projects the card with them
        anchorTracking = false;
        if(anchor != nullptr)
        {
            ArTrackingState anchorState = AR_TRACKING_STATE_STOPPED;
            ArAnchor_getTrackingState(session, anchor, &anchorState);
            anchorTracking = anchorState == AR_TRACKING_STATE_TRACKING;
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
        else if(placeOnTables)
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
        depthValid = false;      // the last depth picture no longer lines up with the screen
        anchorTracking = false;
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

bool ArIsStationShown()
{
    return anchor != nullptr && anchorTracking && cameraTrackingState == static_cast<int>(AR_TRACKING_STATE_TRACKING) &&
           !cardCovered && !cameraBlocked;
}

bool ArGetDepthTexture(unsigned int* texture, float* uvTransform)
{
    if(!depthValid || depthTexture == 0)
    {
        return false;
    }
    *texture = depthTexture;
    std::memcpy(uvTransform, depthUvTransform, sizeof(depthUvTransform));
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
        if(cameraTrackingState != static_cast<int>(AR_TRACKING_STATE_TRACKING))
        {
            return "AR: tracking lost - station hidden, move slowly";
        }
        if(!anchorTracking)
        {
            return "AR: anchor paused - station hidden, look at the card";
        }
        if(cameraBlocked)
        {
            return "AR: camera blocked - station hidden, uncover the lens";
        }
        if(!anchorOnCard)
        {
            return "AR: PLACED on a plane";
        }
        const int coverPercent = cardCoveredFraction < 0.0f ? -1 : static_cast<int>(cardCoveredFraction * 100.0f + 0.5f);
        if(cardCovered)
        {
            std::snprintf(text, sizeof(text), "AR: ON CARD - card %d%% covered, station hidden", coverPercent < 0 ? 0 : coverPercent);
        }
        // Live card state: following it, card too far to trust, or out of view (station held where it was).
        else if(!cardInView)
        {
            std::snprintf(text, sizeof(text), "AR: ON CARD - card out of view (last %.0f deg), held (follows %d)",
                          cardViewAngle, cardRefineCount);
        }
        else if(cardDistanceNow > cardMaxDistance)
        {
            std::snprintf(text, sizeof(text), "AR: ON CARD - %.2f m, %.0f deg, too far, held (follows %d)", cardDistanceNow,
                          cardViewAngle, cardRefineCount);
        }
        else
        {
            std::snprintf(text, sizeof(text), "AR: ON CARD - %.2f m, %.0f deg, following, covered %d%% (follows %d)",
                          cardDistanceNow, cardViewAngle, coverPercent < 0 ? 0 : coverPercent, cardRefineCount);
        }
        return text;
    }
    if(cameraTrackingState != static_cast<int>(AR_TRACKING_STATE_TRACKING))
    {
        return "AR: scanning - tracking lost, move slowly";
    }
    if(cardInView)   // the card is found but not placed yet: say what it is waiting for
    {
        std::snprintf(text, sizeof(text), "AR: card seen - %.2f m, %.0f deg, %s", cardDistanceNow, cardViewAngle,
                      cardDistanceNow > cardMaxDistance ? "come closer" : "hold steady");
        return text;
    }
    if(!placeOnTables)
    {
        return cardDatabase != nullptr ? "AR: looking for the card - point the camera at it"
                                       : "AR: no card loaded (check AR.json) and table placement is off";
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
    if(!placeOnTables)
    {
        return 0;   // card-only placement: the planes are not used, and drawn they just cover the camera picture
    }
    const int count = drawPlaneCount < maxCount ? drawPlaneCount : maxCount;
    std::memcpy(matrices, drawPlaneMatrix, sizeof(float) * 16 * static_cast<std::size_t>(count));
    std::memcpy(extents, drawPlaneExtent, sizeof(float) * 2 * static_cast<std::size_t>(count));
    return count;
}