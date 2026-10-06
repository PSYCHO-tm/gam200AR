cmake_minimum_required(VERSION 3.22)
project(M1G05Visualizer CXX)

# Desktop debug tool for M1G05: SDL2 window + GL 4.3 core + Dear ImGui (both approved for tools by the professor).
# It runs the same Core/ interaction engine as the phone build, with a fake anchor pose and a mouse instead of a touch screen.

include(${CMAKE_CURRENT_LIST_DIR}/../../Core/M1G05Sources.cmake)
m1g05_add_core_library(m1g05_core)

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

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
if(TARGET SDL2::SDL2)
    target_link_libraries(imgui_lib PUBLIC SDL2::SDL2)
else()
    target_include_directories(imgui_lib PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(imgui_lib PUBLIC ${SDL2_LIBRARIES})
endif()

add_executable(m1g05_visualizer main.cpp)
target_link_libraries(m1g05_visualizer PRIVATE m1g05_core imgui_lib OpenGL::GL)
if(TARGET SDL2::SDL2)
    target_link_libraries(m1g05_visualizer PRIVATE SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(m1g05_visualizer PRIVATE SDL2::SDL2main)
    endif()
else()
    target_link_libraries(m1g05_visualizer PRIVATE ${SDL2_LIBRARIES})
    target_include_directories(m1g05_visualizer PRIVATE ${SDL2_INCLUDE_DIRS})
endif()
