# Answers one question: will this build start on a Windows machine that has
# never had Qt or MinGW installed?
#
# It walks the whole transitive import graph from vbird.exe and sorts every DLL
# into system (ships with Windows), bundled (present in the deploy folder) or
# missing. Anything missing is a hard failure: the build works here only because
# the DLL happens to be on this machine's PATH.
#
# Invoked by the `check-deps` target:
#   cmake -DVBIRD_EXE=<path to vbird.exe> -P cmake/CheckDeps.cmake

cmake_minimum_required(VERSION 3.21)
include("${CMAKE_CURRENT_LIST_DIR}/SystemDlls.cmake")

if(NOT VBIRD_EXE)
    message(FATAL_ERROR "CheckDeps.cmake: pass -DVBIRD_EXE=<path to vbird.exe>")
endif()
if(NOT EXISTS "${VBIRD_EXE}")
    message(FATAL_ERROR "CheckDeps.cmake: no such file: ${VBIRD_EXE}")
endif()

get_filename_component(_dir "${VBIRD_EXE}" DIRECTORY)

# ---------------------------------------------------------------- subsystem
find_program(OBJDUMP NAMES objdump llvm-objdump REQUIRED)
execute_process(COMMAND "${OBJDUMP}" -p "${VBIRD_EXE}" OUTPUT_VARIABLE _dump)

if(_dump MATCHES "Subsystem[ \t]+0*([0-9]+)")
    if(CMAKE_MATCH_1 STREQUAL "2")
        message(STATUS "Subsystem : GUI (no console window)")
    else()
        message(STATUS "Subsystem : ${CMAKE_MATCH_1} -- expected 2 (GUI); a console window will appear")
    endif()
endif()

# ------------------------------------------------------- transitive walk
set(_pending  "${VBIRD_EXE}")
set(_examined "")
set(_bundled  "")
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

        if(EXISTS "${_dir}/${_dll}")
            list(APPEND _bundled "${_dll}")
            list(APPEND _pending "${_dir}/${_dll}")
        else()
            list(APPEND _missing "${_dll}")
        endif()
    endforeach()
endwhile()

if(_bundled)
    list(REMOVE_DUPLICATES _bundled)
    list(SORT _bundled)
endif()
if(_missing)
    list(REMOVE_DUPLICATES _missing)
    list(SORT _missing)
endif()

# ------------------------------------------------------------------ report
list(LENGTH _bundled _n_bundled)
list(LENGTH _missing _n_missing)

message(STATUS "")
if(_n_missing GREATER 0)
    message(STATUS "${_n_missing} required DLL(s) are NOT in the deploy folder:")
    foreach(_dll IN LISTS _missing)
        message(STATUS "    ${_dll}")
    endforeach()
    message(STATUS "")
    message(FATAL_ERROR
        "This build will fail to start on any machine that does not already "
        "have the DLLs above installed. It appears to work here only because "
        "they are on this machine's PATH. Run the `deploy` target.")
endif()

if(_n_bundled EQUAL 0)
    message(STATUS "Self-contained single file: every import ships with Windows.")
    message(STATUS "Ship: just vbird.exe")
else()
    message(STATUS "Self-contained folder: ${_n_bundled} bundled DLL(s), all present.")
    foreach(_dll IN LISTS _bundled)
        message(STATUS "    ${_dll}")
    endforeach()
    message(STATUS "")
    message(STATUS "Ship: the whole folder, not vbird.exe on its own.")
endif()

# Plugins are loaded by filename at runtime, so they never appear in an import
# table. Missing qwindows.dll aborts startup with "could not find or load the
# Qt platform plugin windows".
if(EXISTS "${_dir}/platforms/qwindows.dll")
    message(STATUS "Platform plugin: platforms/qwindows.dll present.")
elseif(_n_bundled GREATER 0)
    message(STATUS "")
    message(FATAL_ERROR
        "platforms/qwindows.dll is missing. A shared-Qt build cannot start "
        "without it. Run the `deploy` target.")
endif()
