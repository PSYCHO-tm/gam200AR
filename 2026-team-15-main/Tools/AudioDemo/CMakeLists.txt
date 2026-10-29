# Stand-alone console demo for the AudioSystem (Core/Source/Audio).
#
#   cmake -S Tools/AudioDemo -B build-audio
#   cmake --build build-audio
#   build-audio\Debug\audio_demo.exe        (Windows, Visual Studio generator)
#   ./build-audio/audio_demo                 (Linux / Mac)
#
# SDL3 is looked for in this order:
#   1. Extern/SDL3 (include/ + the library file in lib/, lib/x64 or bin/)
#   2. An SDL3 installed on the computer (find_package), e.g. -DCMAKE_PREFIX_PATH=C:/SDL3

cmake_minimum_required(VERSION 3.16)
project(KopiAudioDemo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SDL3_ROOT ${REPO_ROOT}/Extern/SDL3)

# ---- Find SDL3 ----------------------------------------------------------------------
find_library(SDL3_LIBRARY NAMES SDL3
    PATHS ${SDL3_ROOT}/lib/x64 ${SDL3_ROOT}/lib ${SDL3_ROOT}/bin
    NO_DEFAULT_PATH)

if(SDL3_LIBRARY AND EXISTS ${SDL3_ROOT}/include/SDL3/SDL.h)
    message(STATUS "Using SDL3 from Extern/SDL3: ${SDL3_LIBRARY}")
    add_library(SDL3::SDL3 UNKNOWN IMPORTED)
    set_target_properties(SDL3::SDL3 PROPERTIES
        IMPORTED_LOCATION ${SDL3_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${SDL3_ROOT}/include)
    find_file(SDL3_DLL NAMES SDL3.dll
        PATHS ${SDL3_ROOT}/lib/x64 ${SDL3_ROOT}/lib ${SDL3_ROOT}/bin
        NO_DEFAULT_PATH)
else()
    find_package(SDL3 CONFIG QUIET)
    if(NOT SDL3_FOUND)
        message(FATAL_ERROR
            "SDL3 library not found.\n"
            "Extern/SDL3 needs the library file, not only the headers.\n"
            "Windows: download SDL3-devel-<version>-VC.zip from https://github.com/libsdl-org/SDL/releases\n"
            "and copy its include/ and lib/ folders into Extern/SDL3 (see Tools/AudioDemo/ReadMe.txt).")
    endif()
    message(STATUS "Using installed SDL3 ${SDL3_VERSION}")
endif()

# ---- The demo ----------------------------------------------------------------------
add_executable(audio_demo
    main.cpp
    ${REPO_ROOT}/Core/Source/Audio/audioSystem.cpp)

# Core/Header                  -> #include "Audio/audioSystem.hpp"
# Extern/NlohmannJson/include  -> #include <nlohmann/json.hpp>
target_include_directories(audio_demo PRIVATE ${REPO_ROOT}/Core/Header ${REPO_ROOT}/Extern/NlohmannJson/include)
target_link_libraries(audio_demo PRIVATE SDL3::SDL3)
target_compile_definitions(audio_demo PRIVATE KOPI_ASSETS_ROOT="${REPO_ROOT}/Assets")

if(MSVC)
    target_compile_options(audio_demo PRIVATE /W4 /utf-8)
else()
    target_compile_options(audio_demo PRIVATE -Wall -Wextra)
endif()

# Windows needs SDL3.dll next to the .exe, so copy it there after every build
if(WIN32)
    if(SDL3_DLL)
        add_custom_command(TARGET audio_demo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL3_DLL} $<TARGET_FILE_DIR:audio_demo>)
    elseif(TARGET SDL3::SDL3-shared)
        add_custom_command(TARGET audio_demo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:audio_demo>)
    endif()
endif()