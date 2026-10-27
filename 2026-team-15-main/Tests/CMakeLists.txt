# Use dependencies
include(${CMAKE_SOURCE_DIR}/Cmake/GetDependencies.cmake)

# Install Catch2 if it doesn't exist on device
get_catch2()

# Every .cpp under Tests/ becomes part of one test executable
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(AllTests ${TEST_SOURCES})
target_link_libraries(AllTests PRIVATE ci_core Catch2::Catch2WithMain)

# runs test
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(AllTests)
