#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "Geometry.hpp"
#include "Input.hpp"
#include "Matrix.hpp"
#include "EquipmentDef.hpp"
#include "Renderer.hpp"

// The equipment you see in AR: loads it, keeps it still in the room, selects it when you tap it, and draws it.
// Everything runs on the GL thread, called from Core (Init / Update / Render), so nothing here needs a lock.
//
// There is exactly one EquipmentScene, reached through Get(). Nobody owns it.
//
// Data in: the pieces come from ECS entities that have MeshRef + Transform3D (Engine.cpp, BuildEquipmentLibrary), their
// .obj meshes from the app (baked in by CMake). Placement in: the anchor pose from ARCore (Engine.cpp, ARTrackingState).
// Out: a list of Renderer::MeshDraw items each frame (the Renderer does the GL work), and the selected piece for the
// action buttons (Engine.cpp, GetActionsText).
//
// Spaces: a piece's mesh is in its own "local" space; Equipment.json places it relative to the anchor; the anchor pose
// places that in the ARCore world. Row vectors, so local * localModel * anchorPose = world (Matrix.hpp).
class EquipmentScene
{
public:
    static EquipmentScene& Get(); // the one scene (created the first time this is called)

    EquipmentScene(EquipmentScene const&)            = delete; // one scene only: no copies
    EquipmentScene& operator=(EquipmentScene const&) = delete;

    // Once per GL context. Takes the equipment list built from the ECS entities, and reads the meshes that CMake baked
    // into the app. Returns false (and logs why) if anything is missing or invalid.
    bool Init(EquipmentLibrary loadedLibrary);

    // Once per frame. 'view' and 'projection' are the camera the player is looking through (the last frame drawn),
    // so a tap selects exactly what is on screen. 'tracking' is false while ARCore cannot place anything.
    void Update(float dt, InputSnapshot const& input, bool tracking, Matrix4x4 const& view, Matrix4x4 const& projection,
                int viewportW, int viewportH);

    // Once per frame, after the camera picture. Does nothing until the equipment has been placed.
    void Draw(Matrix4x4 const& view, Matrix4x4 const& projection);

    // Once per frame, last: the "looking for the card" loader, a ring of see-through dots spinning in the middle of the
    // screen. Draw it whenever the station is not shown. 'seconds' keeps growing (it drives the spin).
    void DrawLoader(float seconds, int viewportW, int viewportH);

    // false = the station is hidden (tracking cannot be trusted, or the card is covered): nothing is drawn or picked.
    void SetStationShown(bool shown) { stationShown = shown; }

    // Called every frame while ARCore tracks, with the anchor's current pose (ARCore keeps correcting it).
    // The first call places the pieces (before it nothing is drawn); later calls move them with the anchor.
    // A desktop build with no ARCore can call it once with a fixed pose (the "fake anchor").
    void SetAnchorPose(Matrix4x4 const& pose);

    // The small debug square on the plane. 0 = hide, 1 = plane not usable (red), 2 = good plane (yellow),
    // 3 = placed, at the anchor (green). 'pose' is where it lies, +y being the plane's normal.
    void SetMarker(int state, Matrix4x4 const& pose);

    // The planes ARCore is tracking right now, each drawn as a see-through rectangle so you can see what it found.
    // 'poses' are the plane centres (+y is the normal) and 'extents' their size along their own x and z, in metres.
    void SetPlanes(std::vector<Matrix4x4> const& poses, std::vector<Vector2D> const& extents);

    // For the buttons and text that will be added later: the equipment, and which piece (if any) is selected.
    EquipmentLibrary const&    GetLibrary() const { return library; }
    std::optional<std::size_t> GetSelected() const { return selected; }

    // ---- Guided lesson (sandbox, Mabel's Figma flow) -------------------------------------------------------------
    // What a step shows after the right tap (Lesson.json "Animation"). Kinds:
    //   Pour     - 'piece' moves over 'into', tips by 'tilt' degrees, a stream runs, 'into' fills to 'level' in 'colour'
    //   Dip      - 'piece' lowers into 'into', brews ('into' turns 'colour'), lifts out and drips, goes back
    //   Sprinkle - 'piece' moves over 'into' and tips; grains in 'colour' fall with gravity and land in it
    //   Place    - 'piece' moves (with a little arc) to 'to' and stays there
    //   Stir     - 'piece' (the spoon) stands up in 'into' and circles; 'into' turns 'colour'
    struct LessonAnimation
    {
        std::string type;
        std::string piece;            // the piece that moves; empty = the step's target
        std::string into;             // the piece acted on
        Vector3D    colour{0.5f, 0.3f, 0.1f};
        float       level   = 0.0f;   // Pour: how full 'into' ends up (0..1 of its inside height)
        float       tilt    = 60.0f;  // degrees the piece tips
        float       seconds = 3.0f;
        Vector3D    to{0.0f, 0.0f, 0.0f}; // Place: the new spot, anchor-local metres
    };

    // One step of the lesson: what to do, the hint under it, and which piece (Equipment.json key) to tap.
    struct LessonStep
    {
        std::string     section; // "BREW" / "PREPARE"
        std::string     title;   // "Add coffee powder"
        std::string     hint;    // the longer help line
        std::string     target;  // equipment id, e.g. "SockFilter"
        bool            hasAnimation = false;
        LessonAnimation animation;
    };

    // The lesson from Assets/Json/Lesson.json (read in Engine.cpp). Starts at step 1.
    void LoadLesson(std::string name, std::string mode, std::vector<LessonStep> steps);

    // Real time in seconds (keeps running while game time is held): drives the arrow's bob.
    void SetClock(float seconds) { clock = seconds; }

    // Everything the Kotlin HUD needs, as tab-separated fields:
    // state (scan / step / done), step number (1-based), step count, section, title, hint, lesson name, mode,
    // and 1 if the last tap hit the wrong piece in the last 1.5 s (else 0).
    std::string LessonState() const;

private:
    static MeshData MakeArrowMesh(); // a small arrow pointing down (-y), tip at the origin

    std::optional<std::size_t> LessonTargetIndex() const; // the current step's piece in library.items, if it exists
    std::optional<std::size_t> PieceIndex(std::string const& id) const;

    // ---- Animation (sandbox) -------------------------------------------------------------------------------------
    static MeshData MakeCylinderMesh(); // radius 1, from y = 0 to y = 1, closed: liquids and the pour stream
    static MeshData MakeGrainMesh();    // a tiny cube, 1 unit wide: powder and sugar grains, drips

    void UpdateAnimation(float dt);     // advances the playing step animation, its particles and liquids
    void FinishAnimation();             // puts everything at rest and moves the lesson on
    void DrawLiquidsAndEffects();       // adds liquids, the stream and the grains to 'draws'

    // A container's inside, from its mesh box: radius, floor height and usable height (piece-local metres).
    void ContainerInside(std::size_t i, float& radius, float& floor, float& height) const;

    struct Liquid
    {
        float    level = 0.0f; // 0..1 of the inside height
        Vector3D colour{0.75f, 0.85f, 0.95f};
    };
    struct Grain
    {
        Vector3D position;  // world metres
        Vector3D velocity;  // metres per second
        Vector3D colour;
        float    floorY;    // world height where it lands and disappears
    };

    std::vector<Vector3D> basePositions; // each piece's resting spot (anchor-local); Place changes it
    std::vector<Vector3D> animOffset;    // this frame's extra move (anchor-local)
    std::vector<float>    animTilt;      // degrees tipped (its local +x side goes down)
    std::vector<float>    animTurn;      // degrees turned about y so its +x side faces where it pours
    std::vector<Liquid>   liquids;       // per piece; only containers ever get a level
    std::vector<Grain>    grains;
    bool                  animating = false;
    float                 animTime  = 0.0f; // seconds into the playing animation (game time: holds while hidden)
    bool                  streamOn  = false;
    Vector3D              streamTop;
    Vector3D              streamBottom;
    Vector3D              streamColour;
    float                 grainSpawn = 0.0f; // fractional grains owed to the spawner
    Liquid                animStartInto;     // the 'into' liquid when the animation started
    Liquid                animStartPiece;    // the moving piece's liquid when it started (a pot that pours)

    std::string             lessonName;
    std::string             lessonMode;
    std::vector<LessonStep> lessonSteps;
    std::size_t             lessonStep = 0;     // 0-based; == size() when the lesson is done
    float                   clock = 0.0f;
    float                   wrongTapAt = -100.0f; // clock time of the last tap on the wrong piece

public:

private:
    EquipmentScene() = default; // only Get() makes one

    static MeshData MakeMarkerMesh(); // a small flat square, lying in the x-z plane (debug square, planes, loader dots)

    void RefreshModels(); // models[i] = localModels[i] * anchorPose

    static char const* FindAsset(char const* path); // an embedded file's text by repo path; nullptr on desktop / missing
    static Vector3D    ColorFor(std::size_t index); // placeholder colour per piece until the data carries colours

    EquipmentLibrary           library;     // the pieces, sorted by id (from the ECS)
    std::vector<MeshData>      meshes;      // same order as library.items
    std::vector<Matrix4x4>     localModels; // local -> anchor-local (scale, yaw, position from the JSON), same order
    std::vector<Matrix4x4>     models;      // local -> ARCore world = localModels * anchorPose, same order
    Matrix4x4                  anchorPose;  // set by SetAnchorPose
    std::optional<std::size_t> selected;    // index into library.items of the tapped piece, if any
    std::vector<Matrix4x4>     planeModels; // each tracked plane as a scaled rectangle, local -> ARCore world
    int                        markerState = 0; // see SetMarker
    Matrix4x4                  markerPose;      // where the debug square lies
    bool                       loaded = false;  // Init succeeded: meshes read and handed to the Renderer
    bool                       placed = false;  // an anchor pose has arrived at least once
    bool                       stationShown = false; // see SetStationShown
    std::vector<Renderer::MeshDraw> draws;           // sandbox optimisation: this frame's draw list, reused (no new heap memory each frame)
};