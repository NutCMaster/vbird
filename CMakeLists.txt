cmake_minimum_required(VERSION 3.21)

project(vbird
    VERSION     0.1.0
    DESCRIPTION "World editor and creator for the Vortex game platform"
    LANGUAGES   CXX
)

# vBird ships as a single self-contained .exe, so the default build is Release.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

# Vulkan headers/loader/glslc all live in the same MSYS2 mingw64 prefix as Qt,
# so CMAKE_PREFIX_PATH (already pointed at $env{QT_DIR}/$env{QT_STATIC_DIR} by
# the presets) resolves this with no extra configuration. glslc is picked up
# automatically as Vulkan::glslc -- FindVulkan.cmake always searches for it.
find_package(Vulkan REQUIRED)

# Turns on AUTOMOC/AUTOUIC/AUTORCC. Without this, adding a Q_OBJECT class fails
# at link time with missing-vtable errors rather than anything self-explanatory.
qt_standard_project_setup()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Whether Qt itself is static decides whether the output is one file or a
# folder. Everything below keys off this rather than off a hand-set option, so
# pointing CMAKE_PREFIX_PATH at a different Qt is all it takes to switch.
get_target_property(_qt_core_type Qt6::Core TYPE)
if(_qt_core_type STREQUAL "STATIC_LIBRARY")
    set(VBIRD_QT_STATIC TRUE)
else()
    set(VBIRD_QT_STATIC FALSE)
endif()

# WIN32 selects the GUI subsystem (-mwindows). Without it Windows opens a
# console window behind the app.
qt_add_executable(vbird WIN32
    src/app.cpp
    src/recent_projects.cpp
    src/recent_projects.h
    src/ui/startup_dialog.cpp
    src/ui/startup_dialog.h
    src/ui/main_window.cpp
    src/ui/main_window.h
    src/engine/vulkan_viewport.cpp
    src/engine/vulkan_viewport.h
    src/engine/scene_renderer.cpp
    src/engine/scene_renderer.h
    src/engine/scene.cpp
    src/engine/scene.h
    src/engine/camera.cpp
    src/engine/camera.h
    res/app.rc
)

# res/app.rc says ICON "vbird_logo.ico" relatively, so windres needs res/ on its
# include path. CMake feeds target include dirs to the RC compiler as -I.
target_include_directories(vbird PRIVATE res)

target_link_libraries(vbird PRIVATE Qt6::Widgets Vulkan::Vulkan)

# Validation is expensive and noisy -- only worth paying for in Debug, where
# main() reads this macro to request the VK_LAYER_KHRONOS_validation layer.
target_compile_definitions(vbird PRIVATE $<$<CONFIG:Debug>:VBIRD_VULKAN_VALIDATION>)

# Compiles GLSL -> SPIR-V at build time and embeds the result via Qt's
# resource system, so the .spv ships inside vbird's Qt resources rather than
# as loose files the deploy/package scripts would need to know about.
set(VBIRD_SHADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/mesh.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/mesh.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/skybox.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/skybox.frag"
)
set(VBIRD_SHADER_OUTDIR "${CMAKE_BINARY_DIR}/shaders")
set(VBIRD_SHADER_SPV
    "${VBIRD_SHADER_OUTDIR}/mesh.vert.spv"
    "${VBIRD_SHADER_OUTDIR}/mesh.frag.spv"
    "${VBIRD_SHADER_OUTDIR}/skybox.vert.spv"
    "${VBIRD_SHADER_OUTDIR}/skybox.frag.spv"
)

add_custom_command(
    OUTPUT ${VBIRD_SHADER_SPV}
    COMMAND ${CMAKE_COMMAND}
            -DVBIRD_GLSLC=$<TARGET_FILE:Vulkan::glslc>
            "-DVBIRD_SHADER_SOURCES=${VBIRD_SHADER_SOURCES}"
            -DVBIRD_SHADER_OUTDIR=${VBIRD_SHADER_OUTDIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileShaders.cmake"
    DEPENDS ${VBIRD_SHADER_SOURCES}
    COMMENT "Compiling GLSL shaders to SPIR-V"
    VERBATIM
)
add_custom_target(shaders DEPENDS ${VBIRD_SHADER_SPV})
add_dependencies(vbird shaders)

qt_add_resources(vbird "shaders"
    PREFIX "/shaders"
    BASE ${VBIRD_SHADER_OUTDIR}
    FILES ${VBIRD_SHADER_SPV}
)

# Toolbar icons, embedded the same way the compiled shaders are above: inside
# vbird's Qt resources, not as loose files the deploy/package scripts need to
# know about. No compile step needed for these (unlike the shaders), so this
# just lists the files directly instead of routing through a custom command.
set(VBIRD_ICON_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/select.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/move.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/rotate.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/scale.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/part.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/spawn.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/copy.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/paste.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/duplicate.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/workspace.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/texture.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/light.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/play.webp"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/stop.webp"
)
qt_add_resources(vbird "icons"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/res/icons"
    FILES ${VBIRD_ICON_FILES}
)

# Day skybox (Kenney, CC0 -- see res/skybox/License.txt). Same embedding
# reasoning as the icons above.
qt_add_resources(vbird "skybox"
    PREFIX "/skybox"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/res/skybox"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/res/skybox/skybox-day.png"
)

target_compile_options(vbird PRIVATE
    -Wall -Wextra
    $<$<CONFIG:Release>:-ffunction-sections -fdata-sections>
)

if(MINGW)
    target_link_options(vbird PRIVATE $<$<CONFIG:Release>:-Wl,--gc-sections -s>)

    # Only fold the GCC runtime into the exe when Qt is static too. This
    # toolchain is a posix-threads build, so plain -static-libgcc/-libstdc++
    # would still leave a libwinpthread import behind; -static covers all three.
    #
    # Against a shared Qt this must NOT be done: those Qt DLLs are themselves
    # linked against the shared libstdc++-6.dll, so a statically linked exe puts
    # two C++ runtimes in one process with separate allocator and exception
    # state. The DLLs have to ship regardless because Qt needs them, so there is
    # nothing to gain by it either.
    if(VBIRD_QT_STATIC)
        target_link_options(vbird PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_why)
    if(_ipo_ok)
        set_property(TARGET vbird PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "vbird: LTO unavailable, continuing without it (${_ipo_why})")
    endif()
endif()

if(VBIRD_QT_STATIC)
    # qt_add_executable generates the plugin-import stubs for a static Qt, which
    # is what links platforms/qwindows.dll into the exe instead of loading it
    # from disk at startup.
    message(STATUS "vbird: static Qt detected -> single self-contained bin/vbird.exe")
else()
    message(STATUS "vbird: shared Qt detected -> exe needs Qt DLLs alongside it")
    message(STATUS "vbird: run 'cmake --build <dir> --target deploy' to gather them")

    # Fallback for a shared Qt: copy the Qt DLLs and the qwindows platform plugin
    # next to the exe, so there is something runnable before a static Qt exists.
    #
    # windeployqt is invoked directly rather than through
    # qt_generate_deploy_app_script. That wrapper runs two deployment passes --
    # one honouring QT_DEPLOY_*, one following the Qt install's own layout -- so
    # every DLL lands twice (once beside the exe, once under bin/ and
    # share/qt6/plugins/) and the folder comes out at double the size. Calling
    # the tool directly gives the standard flat layout Qt searches at startup.
    get_target_property(_qt_qmake_location Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_location}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS "${_qt_bin_dir}"
        REQUIRED
    )

    add_custom_target(deploy
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                $<IF:$<CONFIG:Debug>,--debug,--release>
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:vbird>"
        # windeployqt handles Qt's own libraries but not the third-party ones a
        # distro Qt links against (ICU, PCRE2, zlib, zstd, ...). Without this
        # second pass the folder starts here, where those are on PATH, and fails
        # everywhere else.
        COMMAND ${CMAKE_COMMAND}
                -DVBIRD_DIR=$<TARGET_FILE_DIR:vbird>
                "-DVBIRD_SEARCH_DIRS=${_qt_bin_dir}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployDeps.cmake"
        DEPENDS vbird
        COMMENT "Copying Qt runtime next to vbird.exe"
        VERBATIM
    )

    # Zips the deployed folder into something shippable. Staged through a
    # directory named vbird/ so the archive unpacks to vbird/vbird.exe rather
    # than a bare bin/, and zipped from the staging parent so the entries stay
    # relative instead of embedding the build path.
    add_custom_target(package
        COMMAND ${CMAKE_COMMAND} -E rm -rf   "${CMAKE_BINARY_DIR}/package"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_FILE_DIR:vbird>"   "${CMAKE_BINARY_DIR}/package/vbird"
        # chdir rather than WORKING_DIRECTORY: the latter must already exist when
        # the target starts, but the staging dir is created by the copy above.
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}/package"
                ${CMAKE_COMMAND} -E tar cf
                "${CMAKE_BINARY_DIR}/vbird-win64.zip" --format=zip vbird
        COMMENT "Writing ${CMAKE_BINARY_DIR}/vbird-win64.zip"
        VERBATIM
    )
    add_dependencies(package deploy)
endif()

# Keep the exe at bin/vbird.exe, where run.ps1 and the old Makefile put it.
set_target_properties(vbird PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_target(run
    COMMAND "$<TARGET_FILE:vbird>"
    DEPENDS vbird
    COMMENT "Launching vbird"
    VERBATIM
)

# Reports what the built exe actually depends on, which is the only reliable
# check that it will start on a machine without Qt or MinGW installed.
add_custom_target(check-deps
    COMMAND ${CMAKE_COMMAND}
            -DVBIRD_EXE=$<TARGET_FILE:vbird>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckDeps.cmake"
    DEPENDS vbird
    USES_TERMINAL
    VERBATIM
)

install(TARGETS vbird RUNTIME DESTINATION .)
