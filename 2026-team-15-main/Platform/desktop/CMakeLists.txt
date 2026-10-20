cmake_minimum_required(VERSION 3.22)
project(KopiTwinDesktop LANGUAGES C CXX)

# Desktop shell for the shared C++ in source/. It only creates the window and GL context, reads input and owns the loop.
#   Needs SDL2:  vcpkg install sdl2   (or any SDL2 that find_package can see)
#   cmake -S Platform/desktop -B build-desktop [-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake]
#   cmake --build build-desktop
#   run:  build-desktop/kopitwin_desktop            (add --selftest shot.ppm for a no-input check that writes a frame)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SRC ${REPO_ROOT}/source)

find_package(SDL2 CONFIG REQUIRED)

add_executable(kopitwin_desktop
    main.cpp
    third_party/glad/src/gl.c # GL 4.3 core loader (glad2), vendored so there is nothing else to install
    ${SRC}/sim/math/mat4.cpp
    ${SRC}/sim/collider/aabb.cpp
    ${SRC}/sim/collider/picking.cpp
    ${SRC}/sim/equipment/equipmentDef.cpp
    ${SRC}/sim/equipment/objLoader.cpp
    ${SRC}/sim/interaction/interactionEngine.cpp
    ${SRC}/render/equipmentRenderer.cpp
    ${SRC}/render/kopiScene.cpp)
target_include_directories(kopitwin_desktop PRIVATE ${SRC} ${REPO_ROOT}/Assets/Json third_party/glad/include)
# Data files name their files from the repo root ("Assets/Models/kettle.obj"); the shell resolves them against this.
target_compile_definitions(kopitwin_desktop PRIVATE KOPI_REPO_ROOT="${REPO_ROOT}")
target_link_libraries(kopitwin_desktop PRIVATE SDL2::SDL2)
if(UNIX AND NOT APPLE)
    target_link_libraries(kopitwin_desktop PRIVATE pthread dl)
endif()
