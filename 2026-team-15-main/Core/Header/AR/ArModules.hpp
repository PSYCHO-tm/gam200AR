#pragma once
// ---------------------------------------------------------------------------------------------------------------------
// AR modules: the inside of the AR layer, split into parts you can switch on and off.
//
// Teammates outside graphics/AR only ever use ARcore.hpp (the front door). It did not change when the AR code was
// split, so Engine.cpp and JNI-bridge.cpp need no changes. This header is for working INSIDE the AR layer.
//
//   ARcore.cpp   (core, always)  session life cycle, per-frame order, anchor pose, status text
//   AR/ArCamera  (always)        draws the camera picture as the back layer
//   AR/ArPlanes  (optional)      finds tables and floors; can place the station on one (placeOnTables)
//   AR/ArCard    (optional)      places the station on the printed marker card and keeps it there
//   AR/ArDepth   (optional)      ARCore's depth picture: hides pieces behind real things (lit.frag)
//   AR/ArCover   (optional)      card covered / hand on the lens -> hide the station (needs ArDepth + ArCard)
//
// Per frame, ArGetCameraMatrices in ARcore.cpp calls the modules in this order (the "menu"):
//   ArCardUpdate -> ArDepthUpdate -> ArCoverUpdate -> (no anchor and placeOnTables) ArPlanesPlace
// To switch a module off, stop calling it there (its getters then report "nothing"). See README "Graphics & AR".
//
// Shared state lives in ONE place, gAr (ArContext below). Each module's .cpp starts with a list of named links to
// the parts of gAr it uses, e.g. `static ArAnchor*& anchor = gAr.anchor;`, so its first lines say what it depends on.
// Everything else a module keeps is private to its .cpp. Android only: the desktop build leaves Core/Source/AR out.
// ---------------------------------------------------------------------------------------------------------------------

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <GLES3/gl3.h> // first: gl2ext.h uses its types
#include <android/log.h>

// Kept in its own block so clang-format's include sorting can never move it above gl3.h.
#include <GLES2/gl2ext.h> // GL_TEXTURE_EXTERNAL_OES, for the camera picture

#include "arcore_c_api.h"

#define LOG_TAG "kopitwin"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Placement settings shared by ArPlanes and ArCard.
inline constexpr bool placeOnTables = false; // false = only the printed card places the station (planes still found)
inline constexpr int framesToHold = 20;      // about 0.7 s at 30 fps: hold steady this long before the anchor is placed

// Everything more than one module reads or writes. One instance, gAr, defined in ARcore.cpp.
struct ArContext
{
    // Session (core)
    ArSession* Session = nullptr;
    ArFrame* SessionFrame = nullptr;        // this frame, filled by ArCameraUpdate
    ArTrackableList* SessionList = nullptr; // reused list for planes (ArPlanes)
    GLuint CameraTexture = 0;               // where ARCore writes the camera picture (drawn by ArCamera)
    int DisplayWidth = 1;                   // screen size in pixels (ArPlanes casts its ray from the middle)
    int DisplayHeight = 1;

    // Camera this frame (core)
    int CameraTrackingState = -1; // ArTrackingState as an int, -1 = no frame yet
    float LastView[16]{};         // OpenGL layout (column-major)
    float LastProjection[16]{};

    // The station's anchor (placed by ArCard or ArPlanes, read by the core)
    ArAnchor* Anchor = nullptr;
    bool AnchorOnCard = false;   // the anchor sits on the card (ArCard keeps it there)
    bool AnchorTracking = false; // the anchor's own tracking state this frame
    float LastAnchorMatrix[16]{};
    bool AnchorPoseValid = false;

    // The card (written by ArCard, read by ArCover)
    float CardMatrix[16]{};   // the card's latest pose, +y out of the card
    float CardExtentX = 0.0f; // its size in metres, as ARCore sees it
    float CardExtentZ = 0.0f;
    bool CardPoseKnown = false;

    // Depth and cover (ArDepth / ArCover, read by ArCard and the core)
    bool DepthEnabled = false;         // the phone supports depth and the session has it on
    float CardCoveredFraction = -1.0f; // 0..1 of the card behind something real, -1 = cannot tell
    bool CardCovered = false;          // hide the station: the card is mostly covered
    bool CameraBlocked = false;        // hide the station: a hand is on the lens
};
extern ArContext gAr;

// ---- ArCamera -------------------------------------------------------------------------------------------------------
void ArCamPictureInit(); // builds the camera-picture shader (once per GL context)
void ArCamPictureDraw(); // draws this frame's camera picture over the whole screen

// ---- ArPlanes -------------------------------------------------------------------------------------------------------
void ArPlanesBeginFrame();            // forget last frame's drawable planes
void ArPlanesRefresh();               // list this frame's planes (for drawing and the status line)
void ArPlanesPlace(ArCamera* camera); // try to place the anchor on the plane under the middle of the screen
void ArPlanesClearMarker();           // no debug square this frame (tracking lost)
const char* ArPlanesStatus(char* text, std::size_t size); // scanning text; writes into 'text'
void ArPlanesShutdown();                                  // frees everything (session destroy)

// ---- ArCard ---------------------------------------------------------------------------------------------------------
void ArCardConfigure(ArConfig* config); // adds the card picture to ARCore (before the session is configured)
void ArCardResume();                    // after a pause: look for the card again
bool ArCardLoaded();                    // ARCore accepted the card picture
void ArCardUpdate(ArCamera* camera);    // every tracked frame: find the card, place / refine the anchor
const char* ArCardStatusPlaced(char* text, std::size_t size);   // status once the station is on the card
const char* ArCardStatusScanning(char* text, std::size_t size); // "card seen ..." or nullptr when not seen
void ArCardShutdown();

// ---- ArDepth --------------------------------------------------------------------------------------------------------
void ArDepthConfigure(ArConfig* config); // depth on when the phone supports it (before the session is configured)
void ArDepthInitGL();                    // makes the depth texture (once per GL context)
void ArDepthUpdate();                    // every tracked frame: upload the depth picture
void ArDepthInvalidate();                // tracking lost: the old picture no longer lines up
bool ArDepthPicture(const uint8_t*& data, int& width, int& height, int& rowStride); // this frame's CPU copy
void ArDepthShutdown();

// ---- ArCover --------------------------------------------------------------------------------------------------------
void ArCoverUpdate(); // every tracked frame, after ArDepthUpdate
void ArCoverReset();  // after a pause
void ArCoverShutdown();
