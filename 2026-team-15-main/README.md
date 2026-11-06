# KopiTwin

An AR training app (Team 15, GovCat): point a phone at the printed KopiTwin card and a virtual kopi station appears on
it, so new F&B staff can practise making traditional kopi with step-by-step guidance and feedback.

**Contents**
1. [Repository structure](#repository-structure)
2. [Branching and workflow](#branching-and-workflow)
3. [Code style](#code-style)
4. [Tests and CI](#tests-and-ci)
5. [Graphics and AR](#graphics-and-ar)

---

## Repository structure

```text
root/
├── Assets/             Audio, JSON data, 3D models, shaders, the marker card picture
├── Cmake/              CMake helpers (GetDependencies, EmbedShader, EmbedAssets)
├── Core/               The engine, shared by every platform
│   ├── Header/         .hpp files (AR/ = the AR modules)
│   ├── Source/         .cpp files (AR/ = the AR modules, Simulation/, Audio/)
│   └── CMakeLists.txt
├── Extern/             External libraries (not in git: fetched by the build)
├── Platform/
│   ├── desktop/        Windows development shell
│   └── KopiTwin/       Android app (Kotlin + JNI)
├── Tests/              Unit and integration tests (Catch2)
├── Tools/              Build, CI and test tools (Tools/CI/)
├── CMakeLists.txt      Root CMake file for the whole project
├── .clang-format       C/C++ formatting rules
├── .clang-tidy         C/C++ naming rules and static analysis
└── .gitignore          Build artifacts and temporary files
```

---

## Branching and workflow

We use feature branches:

- **`main`**: production-ready. Fix merge conflicts on your own branch before merging into it.
- **`JIRAkey-Milestone-Subcategory-Task`**: branch off `main` (from JIRA) for each task or bug fix.
- **Pull requests**: need at least one approving code review before merging.

---

## Code style

`.clang-format` (formatting) and `.clang-tidy` (naming rules and static analysis) define the team conventions. One
command checks every source file and reports each one that does not follow them.

### Setup on Windows

Install [MSYS2](https://www.msys2.org), open **MSYS2 UCRT64** from the Start menu and run:

```bash
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-clang-tools-extra mingw-w64-ucrt-x86_64-gcc
```

Use the UCRT64 terminal for every command below. In Git Bash instead, add the tools to your `PATH` first:

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"
```

Notes:
- `pacman` may print many mirror 404 / timeout errors. That is normal as long as the install finishes.
- The `.clang-tidy` config targets LLVM 18; the MSYS2 package is 17.x. If clang-tidy reports an unknown check or
  option, tell the team.

### Setup on Linux / WSL

Ubuntu 24.04 (the team server and CI) ships LLVM 18, which matches `.clang-tidy`:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git clang-format clang-tidy
```

### Check the setup

```bash
cmake --version && ninja --version && clang-format --version && clang-tidy --version
```

### Run the style check

Open a bash shell at the repo root (a Linux/WSL terminal, or Git Bash / MSYS2 UCRT64 on Windows).

1. **Generate the compilation database** (clang-tidy needs it). Do this once per fresh clone, and again whenever you
   add or remove source files. The first run downloads every dependency and takes a while; later runs are fast.

   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
   ```

2. **Run the check:**

   ```bash
   bash Tools/CI/check_clang.sh
   ```

The script scans `Core/` and `Tests/` (`Extern/` and the platform shells are left out). It prints a `[FAIL]` line for
each file that breaks the rules and exits with a non-zero code if anything fails, so CI or a pre-commit hook can use it.

Notes:
- If `build/compile_commands.json` is missing, the script fails and tells you to run step 1.
- Run the check before every commit and before opening a PR.
- Some existing code may still fail the check. That is expected until a cleanup commit.

### Fix style problems

`clang-format` fixes formatting by itself. From the repo root:

```bash
find Core Tests \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) | xargs clang-format -i
```

Naming problems reported by `clang-tidy` have to be fixed by hand. The rules are in `.clang-tidy`.

---

## Tests and CI

To build and run the unit tests (Linux / WSL), see `Tools/CI/ReadMe.txt`.

---

## Graphics and AR

How the phone shows the kopi station on the printed card, and where to change things. Owner: John (graphics and AR).

### The one rule

From outside graphics/AR, use only the front doors:

- **`Core/Header/ARcore.hpp`** for AR
- **`Core/Header/Renderer.hpp`** and **`Core/Header/EquipmentScene.hpp`** for drawing

Everything else in this section sits behind those doors, so it can change without breaking your code.

### One frame, start to finish

About 30 times a second (the camera sets the pace), on the GL thread:

```text
JNI update()
├─ Core::Update -> ARTrackingState()                Engine.cpp
│   ├─ ArCameraUpdate()                             ARcore.cpp       ARCore takes the new camera frame
│   └─ ArGetCameraMatrices()                        ARcore.cpp       camera view/projection, then the AR "menu":
│        ├─ ArCardUpdate()                          AR/ArCard        find the card, place / follow the anchor
│        ├─ ArDepthUpdate()                         AR/ArDepth       depth picture -> texture
│        ├─ ArCoverUpdate()                         AR/ArCover       card covered? hand on the lens?
│        └─ ArPlanesPlace()   (placeOnTables only)  AR/ArPlanes      place on a table instead
└─ Core::Render()                                   Engine.cpp
    ├─ ArCameraDraw() -> ArCamPictureDraw()         AR/ArCamera      1. the camera picture (back layer)
    └─ EquipmentScene::Draw()                       EquipmentScene   2. the station: what to draw and where
         └─ Renderer::DrawMeshes()                  Renderer            how: shader, colours, depth hiding
```

### AR modules

Android only: they live in `Core/Source/AR/`, and the desktop build leaves that folder out.

| Module | Job | Can switch off? |
|---|---|---|
| `ARcore.cpp` | Session life cycle, the per-frame menu above, anchor pose, status line | No (core) |
| `AR/ArCamera` | Draws the camera picture | No |
| `AR/ArCard` | Places the station on the printed card and keeps it there (averages readings, ignores the card while a hand covers it) | Yes |
| `AR/ArDepth` | ARCore depth (worked out from camera motion, no depth sensor needed): real hands hide the pieces | Yes |
| `AR/ArCover` | Card covered or hand on the lens: hide the station and hold game time (needs ArDepth and ArCard) | Yes |
| `AR/ArPlanes` | Finds tables; can place the station on one (`placeOnTables`, off by default) | Yes |

- **`Core/Header/AR/ArModules.hpp`** lists every module's functions and the shared state (`ArContext gAr`).
- Each module's `.cpp` starts with the parts of `gAr` it uses, so its first lines show what it depends on.
- **To switch a module off**, remove its call from the menu in `ArGetCameraMatrices` (`ARcore.cpp`). Its getters then
  report "nothing" (no depth texture, not covered, ...) and everything else keeps working.
- A phone without depth support switches ArDepth off by itself (log: `Depth: not supported on this phone`).

### Render files

| File | Job |
|---|---|
| `Renderer.*` | How things are drawn: picks each item's shader, sets colours and matrices, depth test, blending |
| `Shader.*` | One shader program |
| `Mesh.*` | One mesh on the GPU |
| `Geometry.*` | Mesh data on the CPU (`Vertex`, `MeshData`) |
| `objLoader.*` | Reads `.obj` meshes, and builds each piece's material `{Shader, Colour}` (`MakeMaterial`) |
| `EquipmentScene.*` | The station: pieces from the ECS, tap picking (ray against triangles), the draw list, the loader |
| `Assets/Shader/lit.vert`, `lit.frag` | The lit shader. `Core/Header/lit_*.hpp` are generated from them: never edit those |

### Change it from data (no C++)

| To change | Edit |
|---|---|
| A piece's model, shader or colour | `Assets/Json/Equipment.json`, its `"MeshRef": { "Path", "Shader": "Lit", "Colour": [r, g, b] }` |
| A piece's position | `Assets/Json/Equipment.json`, its `"Transform3D"` (metres; the card's centre is 0, 0, 0) |
| Which pieces appear | `Assets/Json/Scenes.json`, one `{ "kind": "EQUIPMENT", "def": "<name>" }` per piece |
| The card picture, printed width, tilt mode | `Assets/Json/AR.json`, `AR.MarkerCard` |

The tuning numbers (card lock-on distance, cover thresholds, hold-off frames) are named constants at the top of each
module's `.cpp`, each with a comment saying what it does.

Data and models are baked into the app, so rebuild and reinstall after changing them.

### How to...

- **Add a model**
  1. Put the `.obj` in `Assets/Models/`.
  2. Add it to the `embed_assets(...)` list in `Platform/KopiTwin/app/src/main/cpp/CMakeLists.txt`.
  3. Point an `Equipment.json` entry's `MeshRef` at it.
- **Add a shader**
  1. Add `Assets/Shader/<name>.vert` and `.frag`, and an `embed_shader(...)` line in the same CMakeLists.
  2. Add a `ShaderKind` and its name in `objLoader.hpp` / `objLoader.cpp` (`ShaderKindFromName`).
  3. Add a case for it in `Renderer::ShaderFor`.
- **Add a component:** follow the 3 steps at the top of `Core/Header/RegisterComponents.hpp` (struct, JSON macro,
  `COMPONENT_LIST`), plus an optional `Rules<T>::Check`.
- **Add an AR module**
  1. Write `Core/Source/AR/ArX.cpp` and declare its functions in `ArModules.hpp`.
  2. Put any value other modules need in `ArContext`.
  3. Call it from the menu in `ArGetCameraMatrices`.

### Debugging on the phone

- **Logcat:** filter `tag:kopitwin`. Useful lines: `Marker card found`, `Marker card: refined`, `Marker card covered`,
  `Camera blocked`, `Session time held` / `running again`, `Depth picture: 160 x 90`.
- **Missing log lines:** the phone drops lines when an app logs a lot, often the startup ones. Record before launching:
  `adb logcat -v time -s kopitwin > log.txt`
- **The status line** (first line of the on-screen overlay), for example:

  ```text
  AR: ON CARD - 0.45 m, 33 deg, following, covered 12% (follows 3)
  ```

  | Part | Meaning |
  |---|---|
  | `0.45 m` | Distance from the phone to the card |
  | `33 deg` | Viewing angle (0 = straight on, 90 = edge on) |
  | `following` | Also `too far, held` or `card out of view, held` |
  | `covered 12%` | How much of the card something real is in front of; 75% or more hides the station |
  | `follows 3` | How many times the station has moved to follow the card |

- **Something looks wrong?** Switch modules off one at a time in the menu. The one whose removal fixes it is the culprit.
