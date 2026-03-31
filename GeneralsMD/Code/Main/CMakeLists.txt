cmake_minimum_required(VERSION 3.16)

project(z_generals LANGUAGES C CXX RC)

add_executable(z_generals WIN32)

set(Z_GENERALS_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin")

set(Z_GENERALS_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin")

set_target_properties(z_generals PROPERTIES
    OUTPUT_NAME "generalszh${RTS_BUILD_OUTPUT_SUFFIX}"

    RUNTIME_OUTPUT_DIRECTORY "${Z_GENERALS_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${Z_GENERALS_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${Z_GENERALS_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${Z_GENERALS_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${Z_GENERALS_BIN_DIR}"

    PDB_OUTPUT_DIRECTORY "${Z_GENERALS_BIN_DIR}"
    PDB_OUTPUT_DIRECTORY_DEBUG "${Z_GENERALS_BIN_DIR}"
    PDB_OUTPUT_DIRECTORY_RELEASE "${Z_GENERALS_BIN_DIR}"
    PDB_OUTPUT_DIRECTORY_RELWITHDEBINFO "${Z_GENERALS_BIN_DIR}"
    PDB_OUTPUT_DIRECTORY_MINSIZEREL "${Z_GENERALS_BIN_DIR}"
)

if(MSVC)
    set_target_properties(z_generals PROPERTIES
        COMPILE_PDB_OUTPUT_DIRECTORY "${Z_GENERALS_BIN_DIR}"
        COMPILE_PDB_OUTPUT_DIRECTORY_DEBUG "${Z_GENERALS_BIN_DIR}"
        COMPILE_PDB_OUTPUT_DIRECTORY_RELEASE "${Z_GENERALS_BIN_DIR}"
        COMPILE_PDB_OUTPUT_DIRECTORY_RELWITHDEBINFO "${Z_GENERALS_BIN_DIR}"
        COMPILE_PDB_OUTPUT_DIRECTORY_MINSIZEREL "${Z_GENERALS_BIN_DIR}"
    )
endif()

target_link_libraries(z_generals PRIVATE
    binkstub
    bridge
    comctl32
    core_debug
    core_profile
    d3dx8
    dinput8
    dxguid
    imm32
    vfw32
    winmm
    z_gameengine
    z_gameenginedevice
    zi_always
)

# TODO Originally referred to build host and user, replace with git info perhaps?
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/GeneratedVersion.h
"#pragma once

#define VERSION_LOCALBUILDNUM 0
#define VERSION_BUILDUSER \"\"
#define VERSION_BUILDLOC \"\"
"
)

# Based on original binary values for these variables.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.h
"#pragma once

#define VERSION_MAJOR 1
#define VERSION_MINOR 4
#define VERSION_BUILDNUM 601
"
)

if(MSVC)
    target_link_options(z_generals PRIVATE "/NODEFAULTLIB:libci.lib")
endif()

target_include_directories(z_generals PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../bridge"
)

target_sources(z_generals PRIVATE
    WinMain.cpp
    WinMain.h
)

# RC files optional for MinGW builds
# Icon: LoadIcon() handles missing resource gracefully (uses default system icon)
# Manifest: DPI awareness metadata (nice to have but not essential)
if(MSVC)
    # VS2005 and later adds default manifest, we need to turn it off to prevent conflict with custom manifest
    if(NOT IS_VS6_BUILD)
        target_link_options(z_generals PRIVATE
            "/MANIFEST:NO"
        )
    endif()

    target_sources(z_generals PRIVATE
        RTS.RC
    )
endif()

# Strip debug symbols to separate file for MinGW Release builds
# This creates generalszh${RTS_BUILD_OUTPUT_SUFFIX}.exe.debug (similar to MSVC .pdb files)
if(MINGW AND COMMAND add_debug_strip_target)
    add_debug_strip_target(z_generals)
endif()
