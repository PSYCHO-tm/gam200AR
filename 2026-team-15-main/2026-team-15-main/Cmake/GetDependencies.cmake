include(FetchContent)

# --- SDL 3 ---
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.18
    GIT_SUBMODULES ""
    SOURCE_DIR     ${REPO_ROOT}/Extern/SDL3
)
FetchContent_MakeAvailable(SDL3)

# --- Nlohmann Json ---
FetchContent_Declare(
    NlohmannJson
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SUBMODULES ""
    SOURCE_DIR     ${REPO_ROOT}/Extern/NlohmannJson
)
FetchContent_MakeAvailable(NlohmannJson)
set(NLOHMANN_ROOT        ${REPO_ROOT}/Extern/NlohmannJson)
set(NLOHMANN_INCLUDE_LIB ${NLOHMANN_ROOT}/include)

# --- AR Core ---
FetchContent_Declare(
    ARCoreSDK
    GIT_REPOSITORY https://github.com/google-ar/arcore-android-sdk.git
    GIT_TAG        1.56.0
    GIT_SUBMODULES ""
    SOURCE_DIR     ${REPO_ROOT}/Extern/ARCore
)
FetchContent_MakeAvailable(ARCoreSDK)

set(ARCORE_VERSION     1.56.0)
set(ARCORE_ROOT        ${REPO_ROOT}/Extern/ARCore/${ARCORE_VERSION})
set(ARCORE_INCLUDE_DIR ${ARCORE_ROOT}/include)
set(ARCORE_LIB_DIR     ${ARCORE_ROOT}/jni)
set(ARCORE_HEADER      ${ARCORE_INCLUDE_DIR}/arcore_c_api.h)

set(ARCORE_AAR ${ARCORE_ROOT}/core-${ARCORE_VERSION}.aar)
if(NOT EXISTS ${ARCORE_HEADER})
    message(STATUS "ARCore ${ARCORE_VERSION}: downloading arcore_c_api.h")
    file(DOWNLOAD
        https://raw.githubusercontent.com/google-ar/arcore-android-sdk/${ARCORE_VERSION}/libraries/include/arcore_c_api.h
        ${ARCORE_HEADER}
        STATUS ARCORE_DL)
    list(GET ARCORE_DL 0 ARCORE_DL_CODE)
    if(NOT ARCORE_DL_CODE EQUAL 0)
        file(REMOVE ${ARCORE_HEADER})
        message(FATAL_ERROR "Could not download arcore_c_api.h: ${ARCORE_DL}")
    endif()
endif()

# --- Catch 2 ---
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.0
    GIT_SUBMODULES ""
    SOURCE_DIR     ${REPO_ROOT}/Extern/Catch2
)
FetchContent_MakeAvailable(Catch2)