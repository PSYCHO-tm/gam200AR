cmake_minimum_required(VERSION 3.22)
project(KopiSceneViewer CXX)

# Desktop viewer for the KopiTwin ECS scene and its animation clips.
# Open THIS folder in Visual Studio (File > Open > Folder...), not the repo root.
# It reuses the root project's kopitCore library (Simulation, ECS, animation) and adds SDL2 + GL 4.3 + Dear ImGui.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(KOPIT_BUILD_TESTS OFF CACHE BOOL "The viewer does not need the test programs" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../.. kopit)

include(FetchContent)

find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    message(STATUS "SDL2 not installed, fetching it (first configure is slow)")
    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.8
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL2)
endif()
find_package(OpenGL REQUIRED)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)

add_library(sceneviewer_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(sceneviewer_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
if(TARGET SDL2::SDL2)
    target_link_libraries(sceneviewer_imgui PUBLIC SDL2::SDL2)
else()
    target_include_directories(sceneviewer_imgui PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(sceneviewer_imgui PUBLIC ${SDL2_LIBRARIES})
endif()

add_executable(kopit_scene_viewer main.cpp)
target_link_libraries(kopit_scene_viewer PRIVATE kopitCore sceneviewer_imgui OpenGL::GL)
if(TARGET SDL2::SDL2main)
    target_link_libraries(kopit_scene_viewer PRIVATE SDL2::SDL2main)
endif()
if(MSVC)
    target_compile_options(kopit_scene_viewer PRIVATE /W4)
else()
    target_compile_options(kopit_scene_viewer PRIVATE -Wall -Wextra)
endif()
