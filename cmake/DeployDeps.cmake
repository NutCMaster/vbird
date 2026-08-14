# Copies every non-system DLL the deployed tree transitively needs.
#
# windeployqt only knows about Qt's own libraries and the compiler runtime. A
# Qt built by a distro like MSYS2 also links third-party libraries -- ICU,
# PCRE2, zlib, zstd, double-conversion, libb2 -- and windeployqt leaves all of
# them behind. The result starts on the build machine, where they happen to be
# on PATH, and fails on every other machine.
#
# This walks the import table of everything already in the deploy directory,
# copies any non-system DLL it finds, and repeats until the set closes over.
#
# Invoked by the `deploy` target:
#   cmake -DVBIRD_DIR=<deploy dir> -DVBIRD_SEARCH_DIRS=<a;b;c> -P DeployDeps.cmake

cmake_minimum_required(VERSION 3.21)
include("${CMAKE_CURRENT_LIST_DIR}/SystemDlls.cmake")

if(NOT VBIRD_DIR OR NOT IS_DIRECTORY "${VBIRD_DIR}")
    message(FATAL_ERROR "DeployDeps.cmake: pass -DVBIRD_DIR=<deploy dir>")
endif()
if(NOT VBIRD_SEARCH_DIRS)
    message(FATAL_ERROR "DeployDeps.cmake: pass -DVBIRD_SEARCH_DIRS=<dirs to copy from>")
endif()

# windeployqt sometimes copies DLLs defensively that nothing in the deployed
# tree actually loads -- e.g. dxcompiler.dll (DirectX Shader Compiler),
# apparently bundled because Qt6Gui *can* use QRhi's D3D/DXC shader backend,
# even though vbird only uses QVulkanWindow and never touches QRhi/QShader.
# Confirmed by scanning every deployed file's import table: nothing, not even
# vbird.exe, references it. It's also MSVC-compiled (needs MSVCP140.dll /
# VCRUNTIME140[_1].dll, which a MinGW build never otherwise needs), so leaving
# it in place would demand bundling the VC++ redistributable for a file
# nothing calls. Pruned before the transitive walk below so its own unused
# dependencies never get demanded.
set(VBIRD_ORPHAN_DLL_NAMES dxcompiler.dll)
foreach(_orphan IN LISTS VBIRD_ORPHAN_DLL_NAMES)
    if(EXISTS "${VBIRD_DIR}/${_orphan}")
        file(REMOVE "${VBIRD_DIR}/${_orphan}")
        message(STATUS "Pruned unused ${_orphan} (windeployqt copies it defensively; nothing in vbird references it)")
    endif()
endforeach()

# Everything already deployed, including plugins in subdirectories.
file(GLOB_RECURSE _seed_files "${VBIRD_DIR}/*.dll" "${VBIRD_DIR}/*.exe")

set(_pending  ${_seed_files})
set(_examined "")
set(_copied   "")
set(_missing  "")

while(_pending)
    list(POP_FRONT _pending _current)

    get_filename_component(_current_name "${_current}" NAME)
    string(TOLOWER "${_current_name}" _current_key)
    if(_current_key IN_LIST _examined)
        continue()
    endif()
    list(APPEND _examined "${_current_key}")

    vbird_get_imports("${_current}" _imports)

    foreach(_dll IN LISTS _imports)
        vbird_is_system_dll("${_dll}" _is_system)
        if(_is_system)
            continue()
        endif()

        # Windows resolves from the exe's directory, so everything lands flat at
        # the top level even when the importer is a plugin in a subdirectory.
        set(_dest "${VBIRD_DIR}/${_dll}")
        if(EXISTS "${_dest}")
            list(APPEND _pending "${_dest}")
            continue()
        endif()

        set(_found "")
        foreach(_dir IN LISTS VBIRD_SEARCH_DIRS)
            if(EXISTS "${_dir}/${_dll}")
                set(_found "${_dir}/${_dll}")
                break()
            endif()
        endforeach()

        if(_found)
            file(COPY "${_found}" DESTINATION "${VBIRD_DIR}")
            list(APPEND _copied  "${_dll}")
            list(APPEND _pending "${_dest}")
        else()
            list(APPEND _missing "${_dll}")
        endif()
    endforeach()
endwhile()

list(REMOVE_DUPLICATES _copied)
list(LENGTH _copied _n_copied)
if(_n_copied GREATER 0)
    list(SORT _copied)
    message(STATUS "Copied ${_n_copied} transitive dependencies windeployqt missed:")
    foreach(_dll IN LISTS _copied)
        message(STATUS "    ${_dll}")
    endforeach()
else()
    message(STATUS "No additional dependencies needed.")
endif()

if(_missing)
    list(REMOVE_DUPLICATES _missing)
    list(SORT _missing)
    message(STATUS "")
    foreach(_dll IN LISTS _missing)
        message(STATUS "    MISSING: ${_dll}")
    endforeach()
    message(FATAL_ERROR
        "Could not locate the DLLs listed above in VBIRD_SEARCH_DIRS. The "
        "deployment is incomplete and will fail on machines that do not "
        "already have them installed.")
endif()
