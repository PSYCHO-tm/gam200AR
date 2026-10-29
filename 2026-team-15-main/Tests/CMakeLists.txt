# Use dependencies
include(${CMAKE_SOURCE_DIR}/Cmake/GetDependencies.cmake)

# Install Catch2 if it doesn't exist on device
get_catch2()

# Every .cpp under Tests/ becomes part of one test executable
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(AllTests ${TEST_SOURCES})
target_link_libraries(AllTests PRIVATE ci_core Catch2::Catch2WithMain)
target_compile_definitions(AllTests PRIVATE KOPI_ASSETS_DIR="${CMAKE_SOURCE_DIR}/Assets/Json") 


# runs test
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(AllTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Assets) # Engine
add_test(NAME console_sim_bot COMMAND console_sim --speed 0 --seed 1 --assets ${CMAKE_SOURCE_DIR}/Assets/Json) # ConsoleSim program runs separately

