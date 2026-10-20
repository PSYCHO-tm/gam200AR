cmake_minimum_required(VERSION 3.22)
project(M1G05Tests CXX)

# Builds and runs the M1G05 host tests (no window, no GL). Standalone: no vcpkg needed.
#   cmake -S Tests -B build-tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(M1G05_SANITIZE "Build with AddressSanitizer/UBSan so a leak fails the test run" ON)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_library(m1g05_sim STATIC
    ${REPO_ROOT}/source/sim/math/mat4.cpp
    ${REPO_ROOT}/source/sim/collider/aabb.cpp
    ${REPO_ROOT}/source/sim/collider/picking.cpp
    ${REPO_ROOT}/source/sim/equipment/equipmentDef.cpp
    ${REPO_ROOT}/source/sim/equipment/objLoader.cpp
    ${REPO_ROOT}/source/sim/interaction/interactionEngine.cpp)
# Headers are included as "sim/...", so the include root is source/. json.hpp is the team's, in Assets/Json.
target_include_directories(m1g05_sim PUBLIC ${REPO_ROOT}/source ${REPO_ROOT}/Assets/Json)

function(M1G05Strict target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
    if(M1G05_SANITIZE)
        if(MSVC)
            target_compile_options(${target} PRIVATE /fsanitize=address)
        else()
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()
M1G05Strict(m1g05_sim)

enable_testing()
file(GLOB M1G05_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/test_*.cpp)
foreach(testFile ${M1G05_TEST_SOURCES})
    get_filename_component(testName ${testFile} NAME_WE)
    add_executable(${testName} ${testFile})
    target_link_libraries(${testName} PRIVATE m1g05_sim)
    M1G05Strict(${testName})
    add_test(NAME ${testName} COMMAND ${testName})
endforeach()
