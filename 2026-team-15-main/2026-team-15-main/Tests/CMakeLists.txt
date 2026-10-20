# Install Catch2 if it doesn't exist on device
include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.0)
FetchContent_MakeAvailable(Catch2)

# Every .cpp under Tests/ becomes part of one test executable
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(AllTests ${TEST_SOURCES})
target_link_libraries(AllTests PRIVATE ci_core Catch2::Catch2WithMain)

# runs test
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(AllTests)
