cmake_minimum_required(VERSION 3.22)
project(KopiTwin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KOPIT_BUILD_TESTS "Build host-side tests"                      ON)
option(KOPIT_SANITIZE    "AddressSanitizer + LeakSanitizer in tests"  ON)
option(KOPIT_LEAK_CANARY "Add a deliberately leaking test (must FAIL)" OFF)
option(KOPIT_WARNINGS_AS_ERRORS "Treat warnings as errors"            ON)

# Existing M1G05 sources + nlohmann_json (FetchContent) come from here.
include(${CMAKE_CURRENT_LIST_DIR}/Core/M1G05Sources.cmake)

function(KopitSetWarnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(KOPIT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
        if(KOPIT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---- kopitCore: everything in Core/, no GL, no window, no platform headers ----
set(KOPIT_CORE_SOURCES
    ${M1G05_CORE_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/Core/Source/Log/Log.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Core/Source/Timing/Timing.cpp
    "${CMAKE_CURRENT_LIST_DIR}/Core/Source/Entity Component System/ComponentRegistry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Core/Source/Entity Component System/SceneData.cpp"
    ${CMAKE_CURRENT_LIST_DIR}/Core/Source/App/Animation.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Core/Source/App/Systems.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Core/Source/App/Simulation.cpp)

add_library(kopitCore STATIC ${KOPIT_CORE_SOURCES})
target_include_directories(kopitCore PUBLIC ${CMAKE_CURRENT_LIST_DIR}/Core/Header)
target_link_libraries(kopitCore PUBLIC nlohmann_json::nlohmann_json)
KopitSetWarnings(kopitCore)

# ---- tests (host only, never in the APK) ----
if(KOPIT_BUILD_TESTS AND NOT ANDROID)
    enable_testing()

    set(KOPIT_TEST_SOURCES
        Tests/Baseline/TestMain.cpp
        Tests/Baseline/EcsTests.cpp
        Tests/Baseline/SimulationTests.cpp
        Tests/Baseline/SceneDataTests.cpp
        Tests/Baseline/AnimationTests.cpp
        Tests/Baseline/LogTests.cpp)
    if(KOPIT_LEAK_CANARY)
        list(APPEND KOPIT_TEST_SOURCES Tests/Baseline/LeakCanary.cpp)
    endif()

    add_executable(kopitTests ${KOPIT_TEST_SOURCES})
    target_link_libraries(kopitTests PRIVATE kopitCore)
    target_compile_definitions(kopitTests PRIVATE KOPIT_DATA_DIR="${CMAKE_CURRENT_LIST_DIR}/Assets")
    KopitSetWarnings(kopitTests)
    add_test(NAME kopitTests COMMAND kopitTests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # Existing M1G05 interaction tests, built from the same core.
    add_executable(m1g05Tests Tests/InteractionTests.cpp)
    target_link_libraries(m1g05Tests PRIVATE kopitCore)
    target_compile_definitions(m1g05Tests PRIVATE M1G05_PROJECT_ROOT="${M1G05_ROOT}")
    add_test(NAME m1g05Tests COMMAND m1g05Tests)

    if(KOPIT_SANITIZE)
        if(MSVC)
            # MSVC: ASan is available but has no leak detector; the debug heap check in TestMain covers leaks.
            target_compile_options(kopitCore PUBLIC /fsanitize=address)
        else()
            target_compile_options(kopitCore PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(kopitCore PUBLIC -fsanitize=address,undefined)
        endif()
    endif()
endif()
