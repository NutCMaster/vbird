# Compiles the GLSL shader sources under res/shaders/ to SPIR-V with glslc.
# Invoked as the COMMAND of an add_custom_command() whose OUTPUT is exactly the
# .spv files produced here, so Ninja treats this as an ordinary build step:
# reruns only when a shader source changes, and vbird's resource-compile step
# waits on it via add_dependencies(vbird shaders).
#
#   cmake -DVBIRD_GLSLC=<path to glslc> \
#         "-DVBIRD_SHADER_SOURCES=<a.vert;b.frag;...>" \
#         -DVBIRD_SHADER_OUTDIR=<dir> \
#         -P cmake/CompileShaders.cmake

cmake_minimum_required(VERSION 3.21)

if(NOT VBIRD_GLSLC)
    message(FATAL_ERROR "CompileShaders.cmake: pass -DVBIRD_GLSLC=<path to glslc>")
endif()
if(NOT VBIRD_SHADER_SOURCES)
    message(FATAL_ERROR "CompileShaders.cmake: pass -DVBIRD_SHADER_SOURCES=<a;b;...>")
endif()
if(NOT VBIRD_SHADER_OUTDIR)
    message(FATAL_ERROR "CompileShaders.cmake: pass -DVBIRD_SHADER_OUTDIR=<dir>")
endif()

file(MAKE_DIRECTORY "${VBIRD_SHADER_OUTDIR}")

foreach(_src IN LISTS VBIRD_SHADER_SOURCES)
    get_filename_component(_name "${_src}" NAME)
    set(_out "${VBIRD_SHADER_OUTDIR}/${_name}.spv")
    message(STATUS "Compiling ${_name} -> ${_name}.spv")
    execute_process(
        COMMAND "${VBIRD_GLSLC}" -o "${_out}" "${_src}"
        RESULT_VARIABLE _rc
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "glslc failed compiling ${_src}")
    endif()
endforeach()
