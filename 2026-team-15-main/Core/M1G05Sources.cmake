# Core sources for M1G05 (interaction engine). Included by Tests/ and Tools/Visualizer/ so both build the same code.
get_filename_component(M1G05_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(M1G05_CORE_SOURCES
    ${M1G05_ROOT}/Core/Source/Math/Mat4.cpp
    ${M1G05_ROOT}/Core/Source/Collider/Aabb.cpp
    ${M1G05_ROOT}/Core/Source/Collider/Picking.cpp
    ${M1G05_ROOT}/Core/Source/GameObject/EquipmentLoader.cpp
    ${M1G05_ROOT}/Core/Source/GameObject/ObjLoader.cpp
    ${M1G05_ROOT}/Core/Source/GameObject/InteractionEngine.cpp)

include(FetchContent)
if(NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

function(m1g05_add_core_library target)
    add_library(${target} STATIC ${M1G05_CORE_SOURCES})
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_include_directories(${target} PUBLIC ${M1G05_ROOT}/Core/Header)
    target_link_libraries(${target} PUBLIC nlohmann_json::nlohmann_json)
endfunction()
