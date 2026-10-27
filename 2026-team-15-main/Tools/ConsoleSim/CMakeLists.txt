# Stand-alone build of the console demo for the coffee simulation (M1G06).
# It does NOT use the big Core library, so the ECS code cannot break it.
#
#   cmake -S Tools/ConsoleSim -B build-sim
#   cmake --build build-sim
#   ./build-sim/console_sim            (Visual Studio generator: build-sim/Debug/console_sim.exe)

cmake_minimum_required(VERSION 3.14)
project(KopiConsoleSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SIM_SRC ${REPO_ROOT}/Core/Source/Simulation)

add_executable(console_sim
    main.cpp
    guided.cpp
    ${SIM_SRC}/simData.cpp
    ${SIM_SRC}/orderSystem.cpp
    ${SIM_SRC}/inventory.cpp
    ${SIM_SRC}/prepTracker.cpp
    ${SIM_SRC}/simulation.cpp)

find_package(Threads REQUIRED) # guided mode reads the keyboard on its own thread
target_link_libraries(console_sim PRIVATE Threads::Threads)

# Core/Header      -> #include "Simulation/simulation.hpp"
# Assets/Json      -> only needed while json.hpp has not been moved to Core/Header/nlohmann/
target_include_directories(console_sim PRIVATE ${REPO_ROOT}/Core/Header ${REPO_ROOT}/Assets/Json)

# Where the demo looks for the JSON files when you do not pass --assets
target_compile_definitions(console_sim PRIVATE KOPI_ASSETS_DIR="${REPO_ROOT}/Assets/Json")

if(MSVC)
    target_compile_options(console_sim PRIVATE /W4 /utf-8)
else()
    target_compile_options(console_sim PRIVATE -Wall -Wextra)
endif()
