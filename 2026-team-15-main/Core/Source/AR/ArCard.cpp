// --------------------------------------------------------------------------------------------------------------------
// ArCard: places the station on the printed marker card (optional module).
//
// Settings come from Assets/Json/AR.json (picture, printed width, FollowTilt) through ArSetMarkerConfig /
// ArSetMarkerImage. ArCardConfigure adds the picture to ARCore's image database. Each tracked frame ArCardUpdate
// finds the card, places the anchor on it once it is close and steady, and keeps moving the anchor to the card's
// averaged pose, but only while the card is clearly uncovered (reads ArCover's results through gAr).
// It writes the card's pose and size to gAr so ArCover can measure how much of it is covered.
// --------------------------------------------------------------------------------------------------------------------

#include "AR/ArModules.hpp"

// Shared AR state this module uses (owned by ArContext, see AR/ArModules.hpp).
static ArSession*&       session             = gAr.session;
static ArAnchor*&        anchor              = gAr.anchor;
static bool&             anchorOnCard        = gAr.anchorOnCard;
static float (&cardMatrix)[16]               = gAr.cardMatrix;
static float&            cardExtentX         = gAr.cardExtentX;
static float&            cardExtentZ         = gAr.cardExtentZ;
static bool&             cardPoseKnown       = gAr.cardPoseKnown;
static bool&             depthEnabled        = gAr.depthEnabled;
static float&            cardCoveredFraction = gAr.cardCoveredFraction;
static bool&             cardCovered         = gAr.cardCovered;
static bool&             cameraBlocked       = gAr.cameraBlocked;

// Private to this module
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
static bool cardInView = false;                         // on-screen debug: the card is fully seen this frame
static float cardDistanceNow = 0.0f;                    // on-screen debug: metres from the phone to the card, last seen
static int cardRefineCount = 0;                         // on-screen debug: how often the anchor followed the card
static float cardViewAngle = 0.0f;                      // on-screen debug: degrees off straight-on the phone sees the card, last seen
static const float cardClearFraction = 0.15f;           // the card counts as uncovered (safe to follow) below this
static const int cardHistorySize = 8;                   // about 0.25 s of card readings, averaged before the anchor moves
static float cardHistory[cardHistorySize][4]{};         // x, y, z, yaw of each reading
static int cardHistoryNext = 0;
static int cardHistoryCount = 0;
static const float cardSteadySpread = 0.015f;           // metres: every reading within this of the average, or do not move
static const float cardSteadyAngle = 0.052f;            // radians (about 3 degrees): same for the turn


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

void ArCardConfigure(ArConfig* config)
{
    if(!cardPixels.empty() && cardDatabase == nullptr)
    {
        ArAugmentedImageDatabase_create(session, &cardDatabase);
        int32_t cardIndex = 0;
        const ArStatus cardStatus = ArAugmentedImageDatabase_addImageWithPhysicalSize(session, cardDatabase, "station",
            cardPixels.data(), cardWidthPixels, cardHeightPixels, cardWidthPixels, cardWidthMetres, &cardIndex);
        if(cardStatus == AR_SUCCESS)
        {
            ArConfig_setAugmentedImageDatabase(session, config, cardDatabase);
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
}

void ArCardResume()
{
    cardSteadyFrames = 0;
    cardInView = false;
    cardHistoryCount = 0;
}

bool ArCardLoaded()
{
    return cardDatabase != nullptr;
}


/**
 * @brief   Places the anchor on the printed card once the camera has seen it fully for framesToHold frames.
 * @details Does nothing without a card or once the anchor is on it. If the station was already
 *          placed on a table, that anchor is replaced (the station snaps onto the card).
 * @param[in] camera  The camera of the current frame (used for the distance in the log).
 */
void ArCardUpdate(ArCamera* camera)
{
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

const char* ArCardStatusPlaced(char* text, std::size_t size)
{
    const int coverPercent = cardCoveredFraction < 0.0f ? -1 : static_cast<int>(cardCoveredFraction * 100.0f + 0.5f);
    if(cardCovered)
    {
        std::snprintf(text, size, "AR: ON CARD - card %d%% covered, station hidden", coverPercent < 0 ? 0 : coverPercent);
    }
    // Live card state: following it, card too far to trust, or out of view (station held where it was).
    else if(!cardInView)
    {
        std::snprintf(text, size, "AR: ON CARD - card out of view (last %.0f deg), held (follows %d)",
                      cardViewAngle, cardRefineCount);
    }
    else if(cardDistanceNow > cardMaxDistance)
    {
        std::snprintf(text, size, "AR: ON CARD - %.2f m, %.0f deg, too far, held (follows %d)", cardDistanceNow,
                      cardViewAngle, cardRefineCount);
    }
    else
    {
        std::snprintf(text, size, "AR: ON CARD - %.2f m, %.0f deg, following, covered %d%% (follows %d)",
                      cardDistanceNow, cardViewAngle, coverPercent < 0 ? 0 : coverPercent, cardRefineCount);
    }
    return text;
}

const char* ArCardStatusScanning(char* text, std::size_t size)
{
    if(cardInView)   // the card is found but not placed yet: say what it is waiting for
    {
        std::snprintf(text, size, "AR: card seen - %.2f m, %.0f deg, %s", cardDistanceNow, cardViewAngle,
                      cardDistanceNow > cardMaxDistance ? "come closer" : "hold steady");
        return text;
    }
    return nullptr; // card not seen: let the caller say what else is going on
}

void ArCardShutdown()
{
    cardSteadyFrames = 0;
    cardInView = false;
    cardRefineCount = 0;
    cardPoseKnown = false;
    cardHistoryCount = 0;

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
}
