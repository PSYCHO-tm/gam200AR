# platform/desktop/CMakeLists.txt
# Windows dev shell. Pulled in by the root CMakeLists.txt, Windows only.

cmake_minimum_required(VERSION 3.22.1)

# Use dependencies
include(${CMAKE_SOURCE_DIR}/Cmake/GetDependencies.cmake)


# ---- SDL3 (pinned, built as a static lib) ------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)

get_sdl3()

# ---- GLEW (glew-cmake fork, static) -------------------------------------------
get_glew()

# System OpenGL (opengl32 on Windows)
find_package(OpenGL REQUIRED)

# ---- The shell ---------------------------------------------------------------
add_executable(desktop desktop-main.cpp)

target_compile_features(desktop PRIVATE cxx_std_20)

target_link_libraries(desktop PRIVATE
    SDL3::SDL3
    libglew_static
    OpenGL::GL
)

if(MSVC)
    target_compile_options(desktop PRIVATE /W4 /utf-8)
endif()

# ---- Visual Studio conveniences ----------------------------------------------
# Working directory for F5, so relative shader/asset paths resolve.
set_target_properties(desktop PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
# Only affects a generated .sln (harmless in Open Folder mode).
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT desktop)