# Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
# SPDX-License-Identifier: AGPL-3.0-only

cmake_minimum_required(VERSION 3.22)
project(BleepForge VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC: link the C++ runtime statically (/MT). A plug-in runs inside the host's process, so a
# dynamic MSVCP140.dll would have to be installed on the user's PC and could be an older version
# already loaded by the host. Set before any target is created, so JUCE and the CLAP wrapper match.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# SDKs default to ~/SDKs (Linux, macOS) or %USERPROFILE%\SDKs (Windows); override with -DJUCE_DIR=... etc.
if (WIN32)
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}" BLEEPFORGE_HOME)
else()
    set(BLEEPFORGE_HOME "$ENV{HOME}")
endif()
set(JUCE_DIR "${BLEEPFORGE_HOME}/SDKs/JUCE-8.0.15" CACHE PATH "JUCE source tree")
set(CLAP_JUCE_EXTENSIONS_DIR "${BLEEPFORGE_HOME}/SDKs/clap-juce-extensions-main" CACHE PATH "clap-juce-extensions source tree")

# Per-user plug-in folders from the CLAP and VST3 specifications (no administrator rights needed)
if (WIN32)
    file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}" BLEEPFORGE_LOCALAPPDATA)
    set(BLEEPFORGE_DEFAULT_CLAP_DIR "${BLEEPFORGE_LOCALAPPDATA}/Programs/Common/CLAP")
    set(BLEEPFORGE_DEFAULT_VST3_DIR "${BLEEPFORGE_LOCALAPPDATA}/Programs/Common/VST3")
elseif (APPLE)
    set(BLEEPFORGE_DEFAULT_CLAP_DIR "${BLEEPFORGE_HOME}/Library/Audio/Plug-Ins/CLAP")
    set(BLEEPFORGE_DEFAULT_VST3_DIR "${BLEEPFORGE_HOME}/Library/Audio/Plug-Ins/VST3")
else()
    set(BLEEPFORGE_DEFAULT_CLAP_DIR "${BLEEPFORGE_HOME}/.clap")
    set(BLEEPFORGE_DEFAULT_VST3_DIR "${BLEEPFORGE_HOME}/.vst3")
endif()
option(BLEEPFORGE_INSTALL_AFTER_BUILD "Copy the CLAP and VST3 into the user's plug-in folders after each build" ON)
set(CLAP_INSTALL_DIR "${BLEEPFORGE_DEFAULT_CLAP_DIR}" CACHE PATH "Where the built .clap is copied")
set(VST3_INSTALL_DIR "${BLEEPFORGE_DEFAULT_VST3_DIR}" CACHE PATH "Where the built .vst3 is copied")

add_subdirectory(${JUCE_DIR} JUCE)

option(BLEEPFORGE_BUILD_TESTS "Build the JUCE-free unit tests (run with ctest)" ON)
if (BLEEPFORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
add_subdirectory(${CLAP_JUCE_EXTENSIONS_DIR} clap-juce-extensions EXCLUDE_FROM_ALL)

# NEEDS_MIDI_OUTPUT: the sequencer can send its notes to the host (brief section 5.1).
juce_add_plugin(BleepForge
    COMPANY_NAME "Henrik Juhl"
    BUNDLE_ID dk.hjuhl.bleepforge
    PLUGIN_MANUFACTURER_CODE Hjuh
    PLUGIN_CODE BpFg
    PRODUCT_NAME "BleepForge"
    FORMATS VST3
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${BLEEPFORGE_INSTALL_AFTER_BUILD}   # VST3 (CLAP is copied by the command further down)
    VST3_COPY_DIR "${VST3_INSTALL_DIR}"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

target_sources(BleepForge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# BLEEPFORGE_VERSION_STRING in BleepForgeVersion.h: "git describe --tags --dirty", refreshed at
# every build so it always names the exact commit; project VERSION when git is unavailable.
find_package(Git QUIET)
set(BLEEPFORGE_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/BleepForgeVersion.h")
set(BLEEPFORGE_VERSION_COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT=${BLEEPFORGE_VERSION_HEADER}
    -DFALLBACK=${PROJECT_VERSION} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake)
execute_process(COMMAND ${BLEEPFORGE_VERSION_COMMAND}) # so the header exists before the first build
file(STRINGS ${BLEEPFORGE_VERSION_HEADER} BLEEPFORGE_VERSION REGEX "BLEEPFORGE_VERSION_STRING")
string(REGEX REPLACE ".*\"(.*)\".*" "\\1" BLEEPFORGE_VERSION "${BLEEPFORGE_VERSION}") # as of configuring, for the package name
add_custom_target(BleepForgeVersion COMMAND ${BLEEPFORGE_VERSION_COMMAND} BYPRODUCTS ${BLEEPFORGE_VERSION_HEADER}
    COMMENT "Checking the BleepForge version")
add_dependencies(BleepForge BleepForgeVersion)
target_include_directories(BleepForge PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_compile_definitions(BleepForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_VST3_CAN_REPLACE_VST2=0)  # no VST2 SDK; VST3 only

target_link_libraries(BleepForge
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET BleepForge
    CLAP_ID "dk.hjuhl.bleepforge"
    CLAP_FEATURES instrument synthesizer)

if (BLEEPFORGE_INSTALL_AFTER_BUILD)
    add_custom_command(TARGET BleepForge_CLAP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CLAP_INSTALL_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:BleepForge_CLAP> ${CLAP_INSTALL_DIR}/
        COMMENT "Installing BleepForge.clap into ${CLAP_INSTALL_DIR}")
endif()

# Release archive with the CLAP and VST3 plug-ins, the licence, the third-party notices and
# install notes: dist/BleepForge-linux-x86_64.tar.gz on Linux, dist/BleepForge-windows-x64.zip
# on Windows. The file names carry no version so the README can link to the latest release;
# the folder inside the archive does, from "git describe" when configuring (CI configures from
# scratch; reconfigure a local build before packaging).
if (WIN32)
    set(BLEEPFORGE_PACKAGE_TARGET package-windows)
    set(BLEEPFORGE_PACKAGE_NAME "BleepForge-${BLEEPFORGE_VERSION}-windows-x64")
    set(BLEEPFORGE_PACKAGE_FILE "BleepForge-windows-x64.zip")
    set(BLEEPFORGE_TAR_FLAGS cf)
    set(BLEEPFORGE_PACKAGE_FORMAT --format=zip)
    set(BLEEPFORGE_INSTALL_NOTES "${CMAKE_SOURCE_DIR}/packaging/INSTALL-windows.txt")
else()
    set(BLEEPFORGE_PACKAGE_TARGET package-linux)
    set(BLEEPFORGE_PACKAGE_NAME "BleepForge-${BLEEPFORGE_VERSION}-linux-x86_64")
    set(BLEEPFORGE_PACKAGE_FILE "BleepForge-linux-x86_64.tar.gz")
    set(BLEEPFORGE_TAR_FLAGS cfz)
    set(BLEEPFORGE_PACKAGE_FORMAT --format=gnutar)
    set(BLEEPFORGE_INSTALL_NOTES "${CMAKE_SOURCE_DIR}/packaging/INSTALL-linux.txt")
endif()
set(BLEEPFORGE_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package/${BLEEPFORGE_PACKAGE_NAME}")
add_custom_target(${BLEEPFORGE_PACKAGE_TARGET}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${BLEEPFORGE_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BLEEPFORGE_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:BleepForge_CLAP> ${BLEEPFORGE_PACKAGE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:BleepForge_VST3>/../.. ${BLEEPFORGE_PACKAGE_DIR}/BleepForge.vst3
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${BLEEPFORGE_PACKAGE_DIR}/LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.txt ${BLEEPFORGE_PACKAGE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy ${BLEEPFORGE_INSTALL_NOTES} ${BLEEPFORGE_PACKAGE_DIR}/INSTALL.txt
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/package
            ${CMAKE_COMMAND} -E tar ${BLEEPFORGE_TAR_FLAGS} ${CMAKE_SOURCE_DIR}/dist/${BLEEPFORGE_PACKAGE_FILE} ${BLEEPFORGE_PACKAGE_FORMAT} ${BLEEPFORGE_PACKAGE_NAME}
    DEPENDS BleepForge_CLAP BleepForge_VST3
    COMMENT "Packaging dist/${BLEEPFORGE_PACKAGE_FILE}"
    VERBATIM)
