include(${CMAKE_SOURCE_DIR}/Cmake/GetDependencies.cmake) # Use dependencies macros
get_json()

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "Source/*.cpp")
list(FILTER CORE_SOURCES EXCLUDE REGEX "/test\\.cpp$")     #  exclude placeholder test file
list(FILTER CORE_SOURCES EXCLUDE REGEX "/ARcore\\.cpp$")   #  Android-only, exclude from build test

add_library(ci_core STATIC ${CORE_SOURCES})
target_include_directories(ci_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Header) 
target_link_libraries(ci_core PUBLIC nlohmann_json::nlohmann_json)