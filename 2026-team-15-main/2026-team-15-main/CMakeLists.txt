cmake_minimum_required(VERSION 3.14)
project(KopiTwin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MEM LEAKS, applies to the library AND the tests
add_compile_options(-fsanitize=address -g)
add_link_options(-fsanitize=address)

# Linking Shared Code AND Tests
add_subdirectory(Core)

enable_testing()
add_subdirectory(Tests) # Cmake opens Tests/CMakeLists.txt
