// --------------------------------------------------------------------------------------------------------------------
// ArCover: hides the station when the card or the lens is covered (optional module, needs ArDepth + ArCard).
//
// Each tracked frame ArCoverUpdate checks an 8 x 8 grid of points on the card against the depth picture: the card
// counts as covered when something real is closer than it. It also checks the whole picture: mostly unknown or
// very close means a hand is on the lens. Both use hold-off frames so noise does not flicker. Results go to gAr
// (cardCovered, cameraBlocked, cardCoveredFraction): ArIsStationShown and ArCard read them.
// --------------------------------------------------------------------------------------------------------------------

#include "AR/ArModules.hpp"

// Shared AR state this module uses (owned by ArContext, see AR/ArModules.hpp).
static ArSession*& session = gAr.Session;
static ArFrame*& sessionFrame = gAr.SessionFrame;
static bool& anchorOnCard = gAr.AnchorOnCard;
static float (&cardMatrix)[16] = gAr.CardMatrix;
static float& cardExtentX = gAr.CardExtentX;
static float& cardExtentZ = gAr.CardExtentZ;
static bool& cardPoseKnown = gAr.CardPoseKnown;
static float& cardCoveredFraction = gAr.CardCoveredFraction;
static bool& cardCovered = gAr.CardCovered;
static bool& cameraBlocked = gAr.CameraBlocked;
static float (&lastView)[16] = gAr.LastView;
static float (&lastProjection)[16] = gAr.LastProjection;

// Private to this module
static int cardCoverFrames = 0;         // frames in a row past the threshold that would flip cardCovered
static const float coverHide = 0.75f;   // hide the station once this much of the card is covered
static const float coverShow = 0.6f;    // show it again once less than this is covered (no flicker in between)
static const int coverHoldFrames = 6;   // about 0.2 s: one noisy depth frame does not flip it
static const float coverMargin = 0.03f; // metres: something this much closer than the card is covering it
static int cameraBlockedFrames = 0;
static const float blockedFraction = 0.7f; // this much of the depth picture unknown or very close = blocked
static const int clearHoldFrames = 15;     // about 0.5 s of a clear camera before a blocked one counts as clear
static const float blockedNear = 0.2f;     // metres: closer than this counts as "on the lens"


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
    if (!anchorOnCard || !cardPoseKnown || cardExtentX <= 0.0f || cardExtentZ <= 0.0f)
    {
        return;
    }

    const int grid = 8;
    float ndc[grid * grid * 2];
    float expected[grid * grid]; // how far in front of the phone each card point is, in metres
    int onScreen = 0;
    for (int gz = 0; gz < grid; ++gz)
    {
        for (int gx = 0; gx < grid; ++gx)
        {
            // 90% of the card: its very edges pick up the table's depth too
            const float x = ((gx + 0.5f) / grid - 0.5f) * cardExtentX * 0.9f;
            const float z = ((gz + 0.5f) / grid - 0.5f) * cardExtentZ * 0.9f;
            const float* m = cardMatrix; // column-major: card -> world
            const float world[3] = {m[0] * x + m[8] * z + m[12], m[1] * x + m[9] * z + m[13],
                                    m[2] * x + m[10] * z + m[14]};
            float viewPoint[4];
            for (int r = 0; r < 4; ++r)
            {
                viewPoint[r] =
                    lastView[r] * world[0] + lastView[4 + r] * world[1] + lastView[8 + r] * world[2] + lastView[12 + r];
            }
            float clip[4];
            for (int r = 0; r < 4; ++r)
            {
                clip[r] = lastProjection[r] * viewPoint[0] + lastProjection[4 + r] * viewPoint[1] +
                          lastProjection[8 + r] * viewPoint[2] + lastProjection[12 + r] * viewPoint[3];
            }
            if (clip[3] <= 0.05f)
            {
                continue; // behind the phone
            }
            const float nx = clip[0] / clip[3];
            const float ny = clip[1] / clip[3];
            if (nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f)
            {
                continue; // off screen: the player is not looking at that part
            }
            ndc[onScreen * 2] = nx;
            ndc[onScreen * 2 + 1] = ny;
            expected[onScreen] = clip[3]; // a perspective camera's w is the distance in front of it
            ++onScreen;
        }
    }
    if (onScreen < grid * grid / 4)
    {
        return; // less than a quarter of the card on screen: looking elsewhere, not covered
    }

    float uv[grid * grid * 2];
    ArFrame_transformCoordinates2d(session, sessionFrame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES,
                                   onScreen, ndc, AR_COORDINATES_2D_IMAGE_NORMALIZED, uv);
    int known = 0;
    int covered = 0;
    for (int i = 0; i < onScreen; ++i)
    {
        int px = static_cast<int>(uv[i * 2] * width);
        int py = static_cast<int>(uv[i * 2 + 1] * height);
        px = px < 0 ? 0 : (px >= width ? width - 1 : px);
        py = py < 0 ? 0 : (py >= height ? height - 1 : py);
        const uint8_t* pixel = depth + py * rowStride + px * 2;
        const int millimetres = pixel[0] | (pixel[1] << 8);
        if (millimetres == 0)
        {
            continue; // ARCore does not know the depth here
        }
        ++known;
        if (millimetres * 0.001f < expected[i] - coverMargin)
        {
            ++covered;
        }
    }
    if (known < onScreen / 2)
    {
        return; // too few known points to judge
    }
    cardCoveredFraction = static_cast<float>(covered) / static_cast<float>(known);
}

/**
 * @brief   Hides the station once the card has been mostly covered for a moment, and shows it again once it is
 * uncovered.
 * @details Two thresholds (coverHide, coverShow) and coverHoldFrames in a row, so depth noise does not make it flicker.
 */
static void UpdateCardCovered()
{
    const bool flip = cardCovered ? cardCoveredFraction < coverShow : cardCoveredFraction >= coverHide;
    cardCoverFrames = flip ? cardCoverFrames + 1 : 0;
    if (cardCoverFrames >= coverHoldFrames)
    {
        cardCovered = !cardCovered;
        cardCoverFrames = 0;
        LOGI("Marker card %s (%.0f%% covered)", cardCovered ? "covered: station hidden" : "uncovered: station shown",
             cardCoveredFraction < 0.0f ? 0.0f : cardCoveredFraction * 100.0f);
    }
}

void ArCoverUpdate()
{
    cardCoveredFraction = -1.0f;
    const uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    int rowStride = 0;
    if (ArDepthPicture(data, width, height, rowStride))
    {
        MeasureCardCover(data, width, height, rowStride);

        // Camera blocked: a hand on or right in front of the lens makes most of the depth picture unknown or
        // very close. ARCore may keep "tracking" from the last few features for a moment, so do not wait for it.
        int blocked = 0;
        int samples = 0;
        for (int sy = 0; sy < 9; ++sy)
        {
            for (int sx = 0; sx < 16; ++sx)
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
        if (cameraBlockedFrames >= (cameraBlocked ? clearHoldFrames : coverHoldFrames))
        {
            cameraBlocked = looksBlocked;
            cameraBlockedFrames = 0;
            LOGI("Camera %s (%d%% of the depth picture unknown or under %.0f cm)",
                 cameraBlocked ? "blocked: station hidden" : "clear again", blocked * 100 / samples,
                 blockedNear * 100.0f);
        }
    }
    UpdateCardCovered();
}

void ArCoverReset()
{
    cardCovered = false;
    cardCoverFrames = 0;
    cameraBlocked = false;
    cameraBlockedFrames = 0;
}

void ArCoverShutdown()
{
    ArCoverReset();
    cardCoveredFraction = -1.0f;
}
