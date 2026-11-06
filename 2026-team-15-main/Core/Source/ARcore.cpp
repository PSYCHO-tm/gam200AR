#include "ARcore.hpp"
#include "AR/ArModules.hpp"
#include "arcore_c_api.h"
#include <android/log.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// The one shared AR state (see AR/ArModules.hpp). Every module links to the parts it uses.
ArContext gAr;

// Shared AR state this module uses (owned by ArContext, see AR/ArModules.hpp).
static ArSession*&       session             = gAr.session;
static ArFrame*&         sessionFrame        = gAr.sessionFrame;
static ArTrackableList*& sessionList         = gAr.sessionList;
static ArAnchor*&        anchor              = gAr.anchor;
static bool&             anchorOnCard        = gAr.anchorOnCard;
static bool&             anchorTracking      = gAr.anchorTracking;
static int&              cameraTrackingState = gAr.cameraTrackingState;
static float (&lastAnchorMatrix)[16]         = gAr.lastAnchorMatrix;
static bool&             anchorPoseValid     = gAr.anchorPoseValid;
static float (&lastView)[16]                 = gAr.lastView;
static float (&lastProjection)[16]           = gAr.lastProjection;
static int&              displayWidth        = gAr.displayWidth;
static int&              displayHeight       = gAr.displayHeight;
static GLuint&           cameraTexture       = gAr.cameraTexture;
static bool&             cardCovered         = gAr.cardCovered;
static bool&             cameraBlocked       = gAr.cameraBlocked;

// Private to the core
static ArConfig* sessionConfig = nullptr;
static bool textureBound = false;
static ArPose* anchorPoseHandle = nullptr;
static int displayRotation = 0;
static bool geometryDirty = true;
static bool frameUpdated = false;
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
        ArCardConfigure(sessionConfig);    // the printed card's picture, if Kotlin sent one (ArCard)
        ArDepthConfigure(sessionConfig);   // depth on when the phone supports it (ArDepth)
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
    ArCardResume();
    ArCoverReset();
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
    anchorTracking = false;
    ArCardShutdown();
    ArCoverShutdown();
    ArDepthShutdown();
    ArPlanesShutdown();

    if(anchorPoseHandle != nullptr)
    {
        ArPose_destroy(anchorPoseHandle);
    }
    anchorPoseHandle = nullptr;
    anchorPoseValid = false;
    cameraTrackingState = -1;

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

    ArDepthInitGL();     // the depth texture for this GL context (ArDepth)

    textureBound = false;
    ArCamPictureInit();  // the camera-picture shader (ArCamera)
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
    ArPlanesBeginFrame();   // planes are refilled below, only from a frame that updated successfully

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

    ArPlanesRefresh();   // planes this frame (ArPlanes)
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
        ArCamPictureDraw();
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
        ArCardUpdate(camera);          // the printed card wins: it places the anchor, or moves a table-placed one onto the card
        ArDepthUpdate();                // after the camera matrices above: ArCover projects the card with them
        ArCoverUpdate();                // card covered / camera blocked, from this frame's depth picture
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
            ArPlanesPlace(camera);      // scanning: wait until the middle of the screen steadily points at a table or floor
        }
    }
    else if(framesSinceTracking < 1000)
    {
        ++framesSinceTracking;
    }
    if(trackingState != AR_TRACKING_STATE_TRACKING)
    {
        ArPlanesClearMarker();
        ArDepthInvalidate();     // the last depth picture no longer lines up with the screen
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
        return ArCardStatusPlaced(text, sizeof(text));
    }
    if(cameraTrackingState != static_cast<int>(AR_TRACKING_STATE_TRACKING))
    {
        return "AR: scanning - tracking lost, move slowly";
    }
    if(char const* card = ArCardStatusScanning(text, sizeof(text)))   // the card is found but not placed yet
    {
        return card;
    }
    if(!placeOnTables)
    {
        return ArCardLoaded() ? "AR: looking for the card - point the camera at it"
                              : "AR: no card loaded (check AR.json) and table placement is off";
    }
    return ArPlanesStatus(text, sizeof(text));
}
