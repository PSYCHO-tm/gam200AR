# embed_shader(<input .vert/.frag> <C++ variable name> <output .hpp>)
#
# Copies a GLSL file into a generated C++ header as a string constant, at build time.
# C++ then #includes the header: no file loading at runtime, no Android AssetManager, no Kotlin.
# Editing the shader file makes CMake re-run and regenerate the header on the next build.
function(embed_shader INPUT VAR_NAME OUTPUT)
    file(READ "${INPUT}" SHADER_SOURCE)
    file(WRITE "${OUTPUT}"
        "// Generated from ${INPUT}\n"
        "// Do not edit: edit the shader file instead.\n"
        "#pragma once\n"
        "inline constexpr const char* ${VAR_NAME} = R\"GLSL(${SHADER_SOURCE})GLSL\";\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${INPUT}")
endfunction()
