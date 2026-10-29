include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON) # FetchContent will not run git fetch, checkout, stash. Prevent hidden stashing

# Define REPO_ROOT, Android build does this automatically but desktop doesn't
if (NOT DEFINED REPO_ROOT)
    get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE) # Takes folder of GetDependencies, resolves .. to get repo root, stores in REPO_ROOT
endif()

# --- SDL 3 ---
macro(get_sdl3)
    # Linux/WSL ONLY (the CI build). Android must keep SDL as a shared lib for SDLActivity.
    if(UNIX AND NOT ANDROID)
        set(SDL_UNIX_CONSOLE_BUILD ON  CACHE BOOL "" FORCE) # no X11/Wayland dev libs needed
        set(SDL_TESTS              OFF CACHE BOOL "" FORCE) # faster build, doesn't build SDL own test
        set(SDL_EXAMPLES           OFF CACHE BOOL "" FORCE) # faster build, doesn't build SDL own sample
        set(SDL_TEST_LIBRARY       OFF CACHE BOOL "" FORCE) # faster build, doesn't build SDL helper library
    endif()

    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.4.18
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES ""
        SOURCE_DIR     ${REPO_ROOT}/Extern/SDL3
    )
    FetchContent_MakeAvailable(SDL3)
endmacro()

# --- Nlohmann Json ---
macro(get_json)
    FetchContent_Declare(
        NlohmannJson
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.12.0
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES ""
        SOURCE_DIR     ${REPO_ROOT}/Extern/NlohmannJson
    )
    FetchContent_MakeAvailable(NlohmannJson)
    set(NLOHMANN_ROOT        ${REPO_ROOT}/Extern/NlohmannJson)
    set(NLOHMANN_INCLUDE_LIB ${NLOHMANN_ROOT}/include)
endmacro()

# --- AR Core ---
macro(get_arcore)
    set(ARCORE_VERSION     1.56.0)
    set(ARCORE_ROOT        ${REPO_ROOT}/Extern/ARCore-natives/${ARCORE_VERSION})
    set(ARCORE_INCLUDE_DIR ${ARCORE_ROOT}/include)
    set(ARCORE_LIB_DIR     ${ARCORE_ROOT}/jni)
    set(ARCORE_HEADER      ${ARCORE_INCLUDE_DIR}/arcore_c_api.h)

    if(NOT EXISTS ${ARCORE_HEADER})
        message(STATUS "ARCore ${ARCORE_VERSION}: downloading arcore_c_api.h")
        file(DOWNLOAD
            https://raw.githubusercontent.com/google-ar/arcore-android-sdk/refs/tags/v${ARCORE_VERSION}/libraries/include/arcore_c_api.h
            ${ARCORE_HEADER}
            STATUS ARCORE_DL)
        list(GET ARCORE_DL 0 ARCORE_DL_CODE)
        if(NOT ARCORE_DL_CODE EQUAL 0)
            file(REMOVE ${ARCORE_HEADER})
            message(FATAL_ERROR "Could not download arcore_c_api.h: ${ARCORE_DL}")
        endif()
    endif()
endmacro()

# --- Catch 2 ---
macro(get_catch2)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.0
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES ""
        SOURCE_DIR     ${REPO_ROOT}/Extern/Catch2
    )
    FetchContent_MakeAvailable(Catch2)
endmacro()

# --- GLEW (glew-cmake fork, static) ---
macro(get_glew)
    set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(glew-cmake_BUILD_STATIC ON  CACHE BOOL "" FORCE)
    set(ONLY_LIBS ON CACHE BOOL "" FORCE)   # skip glewinfo/visualinfo executables

    FetchContent_Declare(
        glew
        GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
        GIT_TAG        glew-cmake-2.2.0
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES ""
        SOURCE_DIR     ${REPO_ROOT}/Extern/glew
    )

    set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # Updated to CMake 3.5 due to compatibility with versions < 3.5 being removed
    FetchContent_MakeAvailable(glew)
endmacro()


# --- Android Build ---
if (ANDROID)
    get_json()
    get_sdl3()
    get_arcore()
endif()