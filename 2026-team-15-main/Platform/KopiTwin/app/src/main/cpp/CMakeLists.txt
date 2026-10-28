# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

cmake_minimum_required(VERSION 3.22.1)

project("kopitwin")

# Repo root = six folders above this file (Platform/KopiTwin/app/src/main/cpp)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)
set(CORE_DIR ${REPO_ROOT}/Core)

# Precompiled headers caused build errors earlier, so they stay off.
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)

# Downloads SDL3, nlohmann json and ARCore into Extern/ (runs at Sync, before compiling).
# It also defines NLOHMANN_INCLUDE_LIB, ARCORE_INCLUDE_DIR and ARCORE_LIB_DIR.
include(${REPO_ROOT}/Cmake/GetDependencies.cmake)

# Every .cpp under Core/Source is compiled into the library.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS ${CORE_DIR}/Source/*.cpp)

# Two old files must stay out of the library:
#  - interactionEngine.cpp: old Sim:: picking engine (picking.hpp is commented out, equipmentDef.hpp changed), cannot compile.
#  - test-json.cpp: older copy of DeserializeStream/DeserializePath/SerializeObj, which DefinitionLoader.cpp also defines
#    (duplicate symbol at link time). test-json.hpp is still used, since it only declares them.
list(FILTER CORE_SOURCES EXCLUDE REGEX "/(interactionEngine|test-json)\\.cpp$")

# Header folders, listed by hand. A new folder under Core/Header must be added here.
set(CORE_INCLUDE_DIRS
        ${CORE_DIR}/Header
        ${NLOHMANN_INCLUDE_LIB}
		${ARCORE_INCLUDE_DIR})

# The library that goes into the APK. Its name must match System.loadLibrary("kopitwin").
add_library(${CMAKE_PROJECT_NAME} SHARED	
	  JNI-bridge.cpp
        ${CORE_SOURCES})

include(${REPO_ROOT}/Cmake/EmbedShader.cmake)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
embed_shader(${REPO_ROOT}/Assets/Shader/lit.vert gLitVert ${REPO_ROOT}/Core/Header/lit_vert.hpp)
embed_shader(${REPO_ROOT}/Assets/Shader/lit.frag gLitFrag ${REPO_ROOT}/Core/Header/lit_frag.hpp)

include(${REPO_ROOT}/Cmake/EmbedAssets.cmake)
embed_assets(${GEN_DIR}/EmbeddedAssets.hpp
        Assets/Json/Equipment.json
        Assets/Models/kettle.obj
        Assets/Models/sock_filter.obj
        Assets/Models/condensed_milk_tin.obj
        Assets/Models/cup.obj
        Assets/Models/coffee_liquid.obj)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${CORE_INCLUDE_DIRS}
	${GEN_DIR})

target_link_libraries(${CMAKE_PROJECT_NAME}
        android
        GLESv3
        log
        SDL3::SDL3
        ${ARCORE_LIB_DIR}/${ANDROID_ABI}/libarcore_sdk_c.so)