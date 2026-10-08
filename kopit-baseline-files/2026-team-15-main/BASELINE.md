# KopiTwin baseline (what exists, how to use it)

## Build and test (one command, exits non-zero on any failure)
    python Tools/CI/build.py              # Debug + tests
    python Tools/CI/build.py --all        # Debug and Release
    python Tools/CI/build.py --leak-canary  # must report "tests failed as expected"

Linux/macOS: tests run under AddressSanitizer + LeakSanitizer + UBSan (a leak fails the run).
Windows (MSVC Debug): the CRT debug heap check in Tests/Baseline/TestMain.cpp fails the run on a leak.
Visual Studio 2022: File > Open > CMake... > root CMakeLists.txt.

## Layers (dependencies point down only)
    Platform/*  (SDL2 window, Android shell, ARCore)   <- not written yet
    Core/*      (no GL, no window, no wall clock, no platform events)

## Core modules (Core/Header + Core/Source)
| Folder | What |
|---|---|
| Log | console + file logging (LogInit, LogInfo/LogWarn/LogError) |
| Timing | ScopedTimer + TimingRegistry (per-system ms, last and average) |
| Entity Component System | own ECS (no EnTT): Registry, pools, ForEach; Components; ComponentRegistry; SceneData |
| App | Simulation (Update(dt, InputState)), Systems, InputState, DataWatcher (hot reload polling), Animation |
| Math, Collider, GameObject, Button | existing M1G05 code, unchanged |

## Data (Assets/)
- Assets/Objects/*.json  object definitions: { "type", "components": {...} }
- Assets/Scenes/baseline.json  instances of those types with per-instance "overrides"
- Positions are marker-local meters; rotations are degrees.
- A bad file gives an error like: `scene.instances[1] (kettle): Transform.position: expected an array of 3 numbers`
  and the previous scene stays loaded.

## Animation (pouring, straining)
Clips live in Assets/Animations/*.json. A track animates one property of the entity with that Name:
`position` / `rotationDeg` are offsets from where the entity was when the clip started; `scale` / `color` are absolute.
Keyframes use seconds and ease `smooth` (default) or `linear`.
Assets/Animations/Bindings.json maps interaction actions (the "Action" strings in Equipment.json) to clips:
pour_water, add_grounds, pour_condensed_milk, serve. When the interaction engine accepts an action, call
`simulation.TriggerAction(action)` and the clip plays, advanced only by the fixed timestep (deterministic).
Tuning the look (tilt angle, distances, timing) is a JSON edit, not a code change.
Numbers in the sample clips are placeholders chosen without seeing a render: tune them once the renderer is up.

## How to add a component
1. Add the struct in Components.hpp.
2. Add one Register({...}) block in ComponentRegistry.cpp (load + save).
3. Use it in a system. Nothing else changes.

## How to add a test
Create Tests/Baseline/MyTests.cpp, write KOPIT_TEST(Name) { KOPIT_CHECK(...); }, add the file to
KOPIT_TEST_SOURCES in the root CMakeLists.txt.

## Not done yet (next steps)
- SDL2 desktop shell: fixed-timestep loop, debug overlay, GL 4.3 hello triangle (M1G01)
- Render module (own GL; Mesh upload from Core::MeshData, shaders, markerPose/view/proj inputs)
- Android: wire Platform/KopiTwin CMake to root Core sources, GLES 3.0 shell, ARCore (M1G04)
- Equipment.json/Recipes.json etc. are not loaded by this system yet (EquipmentLoader still separate)
