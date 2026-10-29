include(${CMAKE_SOURCE_DIR}/Cmake/GetDependencies.cmake) # Use dependencies macros
get_json()
get_sdl3()

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "Source/*.cpp")
list(FILTER CORE_SOURCES EXCLUDE REGEX "/test\\.cpp$")     #  exclude placeholder test file
list(FILTER CORE_SOURCES EXCLUDE REGEX "/ARcore\\.cpp$")   #  Android-only, exclude from build test
if(NOT WIN32)   # Linux/WSL console build has no OpenGL (GlApi.hpp), so leave the GPU sources out
    list(FILTER CORE_SOURCES EXCLUDE REGEX "/(Mesh|Shader|Renderer|EquipmentScene)\\.cpp$")
endif()

add_library(ci_core STATIC ${CORE_SOURCES})

# Desktop OpenGL loader (GlApi.hpp includes GL/glew.h on Windows)
if(WIN32)
    get_glew()
    target_link_libraries(ci_core PUBLIC libglew_static)
endif()

option(ENABLE_PROFILER "Enable scoped profiler" ON)
if(ENABLE_PROFILER)
    target_compile_definitions(ci_core PUBLIC PROFILER_ENABLED)
endif()

target_include_directories(ci_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Header) 
target_link_libraries(ci_core PUBLIC nlohmann_json::nlohmann_json SDL3::SDL3)

target_compile_definitions(ci_core PUBLIC KOPI_LOG_DIR="${CMAKE_BINARY_DIR}/logs")
target_compile_definitions(ci_core PUBLIC PROFILER_ENABLED)
