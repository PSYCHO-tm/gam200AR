// --------------------------------------------------------------------------------------------------------------------
// ArPlanes: finds tables and floors, and can place the station on one (optional module).
//
// Each frame ArPlanesRefresh lists the planes ARCore knows (for drawing and the status line). While the station
// is not placed, ArPlanesPlace casts a ray from the middle of the screen and places the anchor on a good plane
// held steady for framesToHold frames. Off by default: placeOnTables (AR/ArModules.hpp) is false, so only the
// printed card places the station; the planes are still found because ARCore tracks better with them.
// --------------------------------------------------------------------------------------------------------------------

#include "AR/ArModules.hpp"

// Shared AR state this module uses (owned by ArContext, see AR/ArModules.hpp).
static ArSession*& session = gAr.Session;
static ArFrame*& sessionFrame = gAr.SessionFrame;
static ArTrackableList*& sessionList = gAr.SessionList;
static ArAnchor*& anchor = gAr.Anchor;
static int& displayWidth = gAr.DisplayWidth;
static int& displayHeight = gAr.DisplayHeight;
static float (&lastAnchorMatrix)[16] = gAr.LastAnchorMatrix;
static bool& anchorPoseValid = gAr.AnchorPoseValid;

// Private to this module
static ArHitResultList* hitList = nullptr; // what the middle-of-the-screen ray touched, nearest first
static ArHitResult* hitResult = nullptr;   // one entry of that list
static ArPose* hitPose = nullptr;          // where on the surface it touched
static ArPose* cameraPose = nullptr;       // where the phone is (only used to say how far below it a plane is)
static int validHitFrames = 0;
static float markerMatrix[16]{}; // where the middle-of-the-screen ray touched a plane (OpenGL layout)
static int markerState = 0;      // 0 = no plane under the ray, 1 = plane but not usable, 2 = usable, holding steady
static float hitDistance = 0.0f; // metres from the phone to that plane
static float hitBelow = 0.0f;    // metres the plane is below the phone
static const int maxDrawPlanes = 8;
static float drawPlaneMatrix[maxDrawPlanes][16]{}; // centre pose of each plane worth drawing (OpenGL layout)
static float drawPlaneExtent[maxDrawPlanes][2]{};  // its size along its own x and z, in metres
static int drawPlaneCount = 0;                     // how many planes are worth drawing right now
static ArPose* planeCentrePose = nullptr;          // reusable pose object for reading each plane's centre
static int planeCount = 0;                         // how many planes ARCore has right now, for the on-screen status
static const char* placementResult =
    nullptr; // what the middle-of-the-screen ray touched this frame, for the on-screen status               // how many
             // tracked frames in a row the middle of the screen has pointed at a good plane
static const float maxPlaceDistance = 3.0f; // metres: ignore planes further away than this

void ArPlanesBeginFrame()
{
    drawPlaneCount = 0; // refilled by ArPlanesRefresh, only from a frame that updated
}

void ArPlanesClearMarker() { markerState = 0; }

void ArPlanesRefresh()
{
    if (sessionList != nullptr)
    {
        ArSession_getAllTrackables(session, AR_TRACKABLE_PLANE, sessionList); // fill the list first
        int32_t n{};
        ArTrackableList_getSize(session, sessionList, &n); // then count it
        planeCount = n;
        static int32_t lastPlaneCount = -1;
        if (n != lastPlaneCount)
        {
            lastPlaneCount = n;
            LOGI("Planes detected: (%d)", n);
        }

        // About once a second, say what each plane is, so we can compare ARCore's idea of a plane with the real room.
        // y is the height in ARCore's world (0 = where the phone was when the app started, negative = below that).
        static int planeLogFrames = 0;
        const bool logPlanes = (n > 0 && ++planeLogFrames % 60 == 0);

        if (n > 0 && planeCentrePose == nullptr)
        {
            ArPose_create(session, nullptr, &planeCentrePose);
        }

        for (int32_t i = 0; i < n; ++i)
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

            // Worth drawing: a floor or table that ARCore is tracking right now, not merged into another, with a real
            // size.
            if (parent == nullptr && planeType == AR_PLANE_HORIZONTAL_UPWARD_FACING &&
                planeState == AR_TRACKING_STATE_TRACKING && extentX > 0.02f && extentZ > 0.02f &&
                drawPlaneCount < maxDrawPlanes)
            {
                ArPose_getMatrix(session, planeCentrePose, drawPlaneMatrix[drawPlaneCount]);
                drawPlaneExtent[drawPlaneCount][0] = extentX;
                drawPlaneExtent[drawPlaneCount][1] = extentZ;
                ++drawPlaneCount;
            }

            if (logPlanes)
            {
                float raw[7];
                ArPose_getPoseRaw(session, planeCentrePose, raw);
                LOGI("  Plane %d: %s, %s%s, y %.2f m, size %.2f x %.2f m", i,
                     planeType == AR_PLANE_HORIZONTAL_UPWARD_FACING     ? "horizontal, faces up"
                     : planeType == AR_PLANE_HORIZONTAL_DOWNWARD_FACING ? "horizontal, faces down"
                                                                        : "vertical",
                     planeState == AR_TRACKING_STATE_TRACKING ? "tracked" : "not tracked",
                     parent != nullptr ? ", merged into another" : "", raw[5], extentX, extentZ);
            }
            if (parent != nullptr)
            {
                ArTrackable_release(ArAsTrackable(parent));
            }
            ArTrackable_release(trackable);
        }
    }
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
void ArPlanesPlace(ArCamera* camera)
{
    if (hitList == nullptr)
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
    float below = 0.0f; // metres between the phone and the plane (positive = the plane is below the phone)
    float extentX = 0.0f;
    float extentZ = 0.0f;
    bool canPlace = false;
    bool foundPlane = false;

    for (int32_t i = 0; i < count && !foundPlane; ++i)
    {
        ArHitResultList_getItem(session, hitList, i, hitResult);

        ArTrackable* trackable = nullptr;
        ArHitResult_acquireTrackable(session, hitResult, &trackable);
        ArTrackableType type = AR_TRACKABLE_NOT_VALID;
        ArTrackable_getType(session, trackable, &type);

        if (type != AR_TRACKABLE_PLANE)
        {
            result = "only loose points so far, no plane";
            ArTrackable_release(trackable);
            continue; // a feature point can sort in front of a plane: keep looking
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
        below = cameraRaw[5] - hitRaw[5]; // y is up in ARCore's world

        if (parent != nullptr)
        {
            result = "a plane that was merged into another one";
            ArTrackable_release(ArAsTrackable(parent));
        }
        else if (planeType != AR_PLANE_HORIZONTAL_UPWARD_FACING)
        {
            result = "a plane, but not a floor or table";
        }
        else if (planeState != AR_TRACKING_STATE_TRACKING)
        {
            result = "horizontal plane, not tracked right now";
        }
        else if (!inside)
        {
            result = "horizontal plane, outside its edge";
        }
        else if (distance > maxPlaceDistance)
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
    if (foundPlane)
    {
        ArPose_getMatrix(session, hitPose, markerMatrix); // hitPose holds the touch point of the plane found above
        markerState = canPlace ? 2 : 1;
        hitDistance = distance;
        hitBelow = below;
    }

    static const char* lastResult = nullptr;
    if (result != lastResult)
    {
        lastResult = result;
        LOGI("Centre ray: %s", result);
    }
    placementResult = result;

    validHitFrames = canPlace ? validHitFrames + 1 : 0;
    if (validHitFrames < framesToHold)
    {
        return;
    }

    const ArStatus status = ArHitResult_acquireNewAnchor(session, hitResult, &anchor);
    if (status != AR_SUCCESS)
    {
        LOGI("Anchor failed to create (%d)", static_cast<int>(status));
        anchor = nullptr;
        validHitFrames = 0;
    }
    else
    {
        LOGI("Anchor created on a plane: %.2f m away, %.2f m below the phone, plane size %.2f x %.2f m", distance,
             below, extentX, extentZ);
    }
}

const char* ArPlanesStatus(char* text, std::size_t size)
{
    (void)size; // the texts below were sized for the 96-char buffer the caller passes
    if (placementResult == nullptr || std::strcmp(placementResult, "no hit") == 0)
    {
        if (planeCount > 0 && drawPlaneCount == 0)
        {
            std::snprintf(text, size, "AR: scanning - %d plane(s) seen but none tracked yet", planeCount);
        }
        else if (drawPlaneCount > 0)
        {
            std::snprintf(text, size, "AR: scanning - %d usable plane(s), aim the centre at one", drawPlaneCount);
        }
        else
        {
            std::snprintf(text, size, "AR: scanning - NO plane found yet, move sideways slowly");
        }
        return text;
    }
    if (std::strcmp(placementResult, "horizontal plane, inside its edge") == 0)
    {
        std::snprintf(text, size, "AR: PLANE FOUND %.1f m away, %.1f m below - hold steady (%d/%d)", hitDistance,
                      hitBelow, validHitFrames, framesToHold);
        return text;
    }
    if (markerState != 0)
    {
        std::snprintf(text, size, "AR: scanning - %s (%.1f m away, %.1f m below)", placementResult, hitDistance,
                      hitBelow);
        return text;
    }
    std::snprintf(text, size, "AR: scanning - %s", placementResult);
    return text;
}

void ArPlanesShutdown()
{
    if (hitPose != nullptr)
    {
        ArPose_destroy(hitPose);
    }
    hitPose = nullptr;

    if (cameraPose != nullptr)
    {
        ArPose_destroy(cameraPose);
    }
    cameraPose = nullptr;
    validHitFrames = 0;

    if (planeCentrePose != nullptr)
    {
        ArPose_destroy(planeCentrePose);
    }
    planeCentrePose = nullptr;
    drawPlaneCount = 0;
    markerState = 0;
    planeCount = 0;
    placementResult = nullptr;

    if (hitResult != nullptr)
    {
        ArHitResult_destroy(hitResult);
    }
    hitResult = nullptr;

    if (hitList != nullptr)
    {
        ArHitResultList_destroy(hitList);
    }
    hitList = nullptr;
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
    if (anchor != nullptr)
    {
        if (!anchorPoseValid)
        {
            return 0;
        }
        std::memcpy(matrix, lastAnchorMatrix, sizeof(lastAnchorMatrix));
        return 3;
    }
    if (markerState == 0)
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
    if (!placeOnTables)
    {
        return 0; // card-only placement: the planes are not used, and drawn they just cover the camera picture
    }
    const int count = drawPlaneCount < maxCount ? drawPlaneCount : maxCount;
    std::memcpy(matrices, drawPlaneMatrix, sizeof(float) * 16 * static_cast<std::size_t>(count));
    std::memcpy(extents, drawPlaneExtent, sizeof(float) * 2 * static_cast<std::size_t>(count));
    return count;
}
