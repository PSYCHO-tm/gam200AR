cmake_minimum_required(VERSION 3.14)
project(KopiTwin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enable compilation database generation for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MEM LEAKS, applies to the library AND the tests
if (NOT MSVC)
	add_compile_options(-fsanitize=address -g)
	add_link_options(-fsanitize=address)
endif()

# Linking Shared Code AND Tests
add_subdirectory(Core)

# desktop build
if(WIN32)
    add_subdirectory(Platform/desktop)
endif()

enable_testing()
# Any subdirectory beyond this line are registered with ctest via add_test() / catch_discover_tests() calls 

add_subdirectory(Tests) # defines AllTests and register tests (ctest runs this)
add_subdirectory(Tools/ConsoleSim) # defines ConsoleSim
