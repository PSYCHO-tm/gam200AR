cmake_minimum_required(VERSION 3.22)
project(M1G05Tests CXX)

option(M1G05_SANITIZE "Build tests with AddressSanitizer/LeakSanitizer so a leak fails the run" ON)

include(${CMAKE_CURRENT_LIST_DIR}/../Core/M1G05Sources.cmake)
m1g05_add_core_library(m1g05_core)

add_executable(m1g05_tests InteractionTests.cpp)
target_link_libraries(m1g05_tests PRIVATE m1g05_core)
target_compile_definitions(m1g05_tests PRIVATE M1G05_PROJECT_ROOT="${M1G05_ROOT}")

if(M1G05_SANITIZE)
    if(MSVC)
        target_compile_options(m1g05_core PUBLIC /fsanitize=address)
    else()
        target_compile_options(m1g05_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(m1g05_core PUBLIC -fsanitize=address,undefined)
    endif()
endif()

enable_testing()
add_test(NAME m1g05_tests COMMAND m1g05_tests)
