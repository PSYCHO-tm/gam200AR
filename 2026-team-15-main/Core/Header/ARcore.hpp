#pragma once

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
bool ArInitialise(void *env, void *activity);

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
bool ArSessionCreate(void *env, void *activity);

/**
 * @brief   Starts or resumes the ARCore session.
 * @details Turns the camera on and starts tracking. Called when the app comes
 *          to the front, once the camera permission is granted.
 * @return  true if the session is running, otherwise false (for example,
 *          error -9 means the camera permission was not granted).
 */
bool ArSessionResume();

/**
 * @brief   Pauses the ARCore session.
 * @details Turns the camera off when the app goes to the background. The
 *          session and its objects are kept, so ArSessionResume can start it
 *          again.
 * @return  true if the session was paused, otherwise false.
 */
bool ArSessionPause();

/**
 * @brief   Destroys the ARCore session and its objects.
 * @details Called when the app is closed. The frame, settings and session are
 *          freed in that order (the reverse of creation), and the flags that
 *          tell ARCore about the texture and screen are reset.
 */
void ArSessionDestroy();

/**
 * @brief   Creates the camera texture and builds the camera program.
 * @details Runs on the drawing thread, once for each new OpenGL context. The
 *          texture is where ARCore writes each camera picture.
 */
void ArCameraInit();

/**
 * @brief   Runs once per frame: asks ARCore for its newest frame and refreshes the plane list.
 * @details Tells ARCore about the camera texture and screen size if it does not know them yet,
 *          then updates the session. Call this before ArGetCameraMatrices, ArGetAnchorPose,
 *          ArGetPlacementMarker and ArGetPlanes, so they all read the same frame. If the update
 *          fails, no planes are reported and ArCameraDraw draws nothing that frame.
 */
void ArCameraUpdate();

/**
 * @brief   Clears the screen and draws the camera picture.
 * @details This is the back layer, drawn before the equipment by Core::Render. It shows the frame
 *          that ArCameraUpdate fetched this frame. If that update failed, only the clear happens.
 */
void ArCameraDraw();

/**
 * @brief   Remembers the new screen size and rotation for ARCore.
 * @details Called when the phone's width, height or rotation changes.
 *          ARCore is told later, from ArCameraUpdate.
 * @param[in] width     Screen width in pixels.
 * @param[in] height    Screen height in pixels.
 * @param[in] rotation  Screen rotation: 0, 1, 2 or 3 (Android Surface.ROTATION_*).
 */
void ArCameraResize(int width, int height, int rotation);

/**
 * @brief   Gives the phone camera's view and projection matrices from the latest frame.
 * @details Call after ArCameraUpdate. Only valid while ARCore is tracking, and for a
 *          few frames after it drops, because only then can the phone's position
 *          be trusted.
 * @param[out] view        16 floats, in the layout OpenGL uses (column-major).
 * @param[out] projection  16 floats, same layout.
 * @return  true if both were written, otherwise false (nothing is written).
 */
bool ArGetCameraMatrices(float* view, float* projection);

/**
 * @brief   Gives the anchor's latest pose as a 4x4 matrix.
 * @details ARCore keeps correcting the anchor as it learns the room, so call this every
 *          frame after ArGetCameraMatrices. The last known pose is kept while tracking
 *          flickers. Nothing is valid before the anchor exists or after the session is destroyed.
 * @param[out] matrix  16 floats, in the layout OpenGL uses (column-major).
 * @return  true if the matrix was written, otherwise false (nothing is written).
 */
bool ArGetAnchorPose(float* matrix);

/**
 * @brief   Says in a few words what the plane placement is doing, for the on-screen text.
 * @details Scanning (no anchor yet), holding steady on a good plane, or placed. Call it on the drawing thread.
 * @return  A short text. It stays valid until the next call.
 */
const char* ArGetPlacementStatus();

/**
 * @brief   Gives the spot to draw the small debug square: where the middle of the screen touches a plane.
 * @details While scanning it follows the ray (state 1 = a plane that cannot be used, 2 = a good plane, held steady
 *          before placing). Once the anchor exists it sits on the anchor (state 3), so you can see where the anchor is
 *          compared with the pieces. The square lies flat on the plane: +y is the plane's normal.
 * @param[out] matrix  16 floats, in the layout OpenGL uses (column-major). Not written when 0 is returned.
 * @return  0 = draw nothing, 1 = plane not usable, 2 = good plane, 3 = placed.
 */
int ArGetPlacementMarker(float* matrix);

/**
 * @brief   Gives the planes worth drawing as a see-through rectangle (floor or table, tracked right now, with a size).
 * @details Each plane comes as its centre pose (+y is the plane's normal) and its size along its own x and z.
 *          Empty when ARCore is not tracking or has found nothing usable.
 * @param[out] matrices  maxCount * 16 floats, each in the layout OpenGL uses (column-major).
 * @param[out] extents   maxCount * 2 floats: the size along the plane's x, then z, in metres.
 * @param[in]  maxCount  How many planes the caller has room for.
 * @return  How many planes were written.
 */
int ArGetPlanes(float* matrices, float* extents, int maxCount);