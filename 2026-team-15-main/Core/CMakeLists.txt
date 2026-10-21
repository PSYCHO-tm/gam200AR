# Look for .cpp files in Core/Source AND compile into a library
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "Source/*.cpp")

list(FILTER CORE_SOURCES EXCLUDE REGEX "/test\\.cpp$") # in case test.cpp breaks the build

add_library(ci_core STATIC ${CORE_SOURCES})

# Anything linked to ci_core can write #include "[Component_name]/[header].hpp"
target_include_directories(ci_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Header)
