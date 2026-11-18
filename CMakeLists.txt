# Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
# SPDX-License-Identifier: AGPL-3.0-only

cmake_minimum_required(VERSION 3.22)
project(BleepWerk VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC: link the C++ runtime statically (/MT). A plug-in runs inside the host's process, so a
# dynamic MSVCP140.dll would have to be installed on the user's PC and could be an older version
# already loaded by the host. Set before any target is created, so JUCE and the CLAP wrapper match.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# SDKs default to ~/SDKs (Linux, macOS) or %USERPROFILE%\SDKs (Windows); override with -DJUCE_DIR=... etc.
if (WIN32)
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}" BLEEPWERK_HOME)
else()
    set(BLEEPWERK_HOME "$ENV{HOME}")
endif()
set(JUCE_DIR "${BLEEPWERK_HOME}/SDKs/JUCE-8.0.15" CACHE PATH "JUCE source tree")
set(CLAP_JUCE_EXTENSIONS_DIR "${BLEEPWERK_HOME}/SDKs/clap-juce-extensions-main" CACHE PATH "clap-juce-extensions source tree")

# Per-user plug-in folders from the CLAP and VST3 specifications (no administrator rights needed)
if (WIN32)
    file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}" BLEEPWERK_LOCALAPPDATA)
    set(BLEEPWERK_DEFAULT_CLAP_DIR "${BLEEPWERK_LOCALAPPDATA}/Programs/Common/CLAP")
    set(BLEEPWERK_DEFAULT_VST3_DIR "${BLEEPWERK_LOCALAPPDATA}/Programs/Common/VST3")
elseif (APPLE)
    set(BLEEPWERK_DEFAULT_CLAP_DIR "${BLEEPWERK_HOME}/Library/Audio/Plug-Ins/CLAP")
    set(BLEEPWERK_DEFAULT_VST3_DIR "${BLEEPWERK_HOME}/Library/Audio/Plug-Ins/VST3")
else()
    set(BLEEPWERK_DEFAULT_CLAP_DIR "${BLEEPWERK_HOME}/.clap")
    set(BLEEPWERK_DEFAULT_VST3_DIR "${BLEEPWERK_HOME}/.vst3")
endif()
# Link-time optimisation makes every build slow, so it is for release builds only: configure
# with -DBLEEPWERK_LTO=ON. CI and the package targets need it (see below).
option(BLEEPWERK_LTO "Link with link-time optimisation (slow; for release builds)" OFF)
option(BLEEPWERK_INSTALL_AFTER_BUILD "Copy the CLAP and VST3 into the user's plug-in folders after each build" ON)
set(CLAP_INSTALL_DIR "${BLEEPWERK_DEFAULT_CLAP_DIR}" CACHE PATH "Where the built .clap is copied")
set(VST3_INSTALL_DIR "${BLEEPWERK_DEFAULT_VST3_DIR}" CACHE PATH "Where the built .vst3 is copied")

add_subdirectory(${JUCE_DIR} JUCE)

option(BLEEPWERK_BUILD_TESTS "Build the JUCE-free unit tests (run with ctest)" ON)
if (BLEEPWERK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
add_subdirectory(${CLAP_JUCE_EXTENSIONS_DIR} clap-juce-extensions EXCLUDE_FROM_ALL)

# An instrument played from the host's notes; sequencing is done in the DAW (brief section 5).
juce_add_plugin(BleepWerk
    COMPANY_NAME "Henrik Juhl"
    BUNDLE_ID dk.hjuhl.bleepwerk
    PLUGIN_MANUFACTURER_CODE Hjuh
    PLUGIN_CODE BpWk
    PRODUCT_NAME "BleepWerk"
    FORMATS VST3
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${BLEEPWERK_INSTALL_AFTER_BUILD}   # VST3 (CLAP is copied by the command further down)
    VST3_COPY_DIR "${VST3_INSTALL_DIR}"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE    # played from the host's notes
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

target_sources(BleepWerk PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MainView.cpp
    Source/EngineView.cpp
    Source/Controls.cpp
    Source/Fonts.cpp
    Source/ChaosView.cpp
    Source/DriveView.cpp
    Source/EffectsView.cpp
    Source/SoundPresets.cpp
    Source/PatternPresets.cpp
    Source/SequencerView.cpp
    Source/AboutView.cpp)

# BLEEPWERK_VERSION_STRING in BleepWerkVersion.h: "git describe --tags --dirty", refreshed at
# every build so it always names the exact commit; project VERSION when git is unavailable.
find_package(Git QUIET)
set(BLEEPWERK_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/BleepWerkVersion.h")
set(BLEEPWERK_VERSION_COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT=${BLEEPWERK_VERSION_HEADER}
    -DFALLBACK=${PROJECT_VERSION} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake)
execute_process(COMMAND ${BLEEPWERK_VERSION_COMMAND}) # so the header exists before the first build
file(STRINGS ${BLEEPWERK_VERSION_HEADER} BLEEPWERK_VERSION REGEX "BLEEPWERK_VERSION_STRING")
string(REGEX REPLACE ".*\"(.*)\".*" "\\1" BLEEPWERK_VERSION "${BLEEPWERK_VERSION}") # as of configuring, for the package name
add_custom_target(BleepWerkVersion COMMAND ${BLEEPWERK_VERSION_COMMAND} BYPRODUCTS ${BLEEPWERK_VERSION_HEADER}
    COMMENT "Checking the BleepWerk version")
add_dependencies(BleepWerk BleepWerkVersion)
target_include_directories(BleepWerk PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_compile_definitions(BleepWerk PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_VST3_CAN_REPLACE_VST2=0)  # no VST2 SDK; VST3 only

# The typefaces, embedded (licences in THIRD-PARTY-NOTICES.txt)
juce_add_binary_data(BleepWerkAssets SOURCES
    Resources/Fonts/BarlowSemiCondensed/BarlowSemiCondensed-SemiBold.ttf
    Resources/Fonts/BarlowSemiCondensed/BarlowSemiCondensed-Bold.ttf
    Resources/Fonts/IBMPlexMono/IBMPlexMono-Medium.ttf)
set_target_properties(BleepWerkAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# The licence texts, built in for the Licences button in About (LicenceData::THIRDPARTYNOTICES_txt)
juce_add_binary_data(BleepWerkLicences HEADER_NAME LicenceData.h NAMESPACE LicenceData SOURCES THIRD-PARTY-NOTICES.txt)
set_target_properties(BleepWerkLicences PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(BleepWerk
    PRIVATE
        BleepWerkAssets
        BleepWerkLicences
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
if (BLEEPWERK_LTO)
    target_link_libraries(BleepWerk PUBLIC juce::juce_recommended_lto_flags)
endif()

clap_juce_extensions_plugin(TARGET BleepWerk
    CLAP_ID "dk.hjuhl.bleepwerk"
    CLAP_FEATURES instrument synthesizer)

# Renders the interface to a PNG (not part of a normal build):
#   cmake --build build --target BleepWerk_Screenshot && build/BleepWerk_Screenshot screenshot.png
add_executable(BleepWerk_Screenshot EXCLUDE_FROM_ALL tools/screenshot.cpp)
target_include_directories(BleepWerk_Screenshot PRIVATE Source $<TARGET_PROPERTY:BleepWerk,INCLUDE_DIRECTORIES>)
target_compile_definitions(BleepWerk_Screenshot PRIVATE $<TARGET_PROPERTY:BleepWerk,COMPILE_DEFINITIONS>)
target_link_libraries(BleepWerk_Screenshot PRIVATE BleepWerk)

if (BLEEPWERK_INSTALL_AFTER_BUILD)
    add_custom_command(TARGET BleepWerk_CLAP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CLAP_INSTALL_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:BleepWerk_CLAP> ${CLAP_INSTALL_DIR}/
        COMMENT "Installing BleepWerk.clap into ${CLAP_INSTALL_DIR}")
endif()

# Release archive with the CLAP and VST3 plug-ins, the licence, the third-party notices and
# install notes: dist/BleepWerk-linux-x86_64.tar.gz on Linux, dist/BleepWerk-windows-x64.zip
# on Windows. The file names carry no version so the README can link to the latest release;
# the folder inside the archive does, from "git describe" when configuring (CI configures from
# scratch; reconfigure a local build before packaging).
if (WIN32)
    set(BLEEPWERK_PACKAGE_TARGET package-windows)
    set(BLEEPWERK_PACKAGE_NAME "BleepWerk-${BLEEPWERK_VERSION}-windows-x64")
    set(BLEEPWERK_PACKAGE_FILE "BleepWerk-windows-x64.zip")
    set(BLEEPWERK_TAR_FLAGS cf)
    set(BLEEPWERK_PACKAGE_FORMAT --format=zip)
    set(BLEEPWERK_INSTALL_NOTES "${CMAKE_SOURCE_DIR}/packaging/INSTALL-windows.txt")
else()
    set(BLEEPWERK_PACKAGE_TARGET package-linux)
    set(BLEEPWERK_PACKAGE_NAME "BleepWerk-${BLEEPWERK_VERSION}-linux-x86_64")
    set(BLEEPWERK_PACKAGE_FILE "BleepWerk-linux-x86_64.tar.gz")
    set(BLEEPWERK_TAR_FLAGS cfz)
    set(BLEEPWERK_PACKAGE_FORMAT --format=gnutar)
    set(BLEEPWERK_INSTALL_NOTES "${CMAKE_SOURCE_DIR}/packaging/INSTALL-linux.txt")
endif()
set(BLEEPWERK_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package/${BLEEPWERK_PACKAGE_NAME}")
# What is released must be LTO-optimised: the package targets stop if the build is not.
set(BLEEPWERK_PACKAGE_CHECK "")
if (NOT BLEEPWERK_LTO)
    file(WRITE "${CMAKE_BINARY_DIR}/need-lto.cmake" "message(FATAL_ERROR \"Release packages need LTO: reconfigure with -DBLEEPWERK_LTO=ON and rebuild\")\n")
    set(BLEEPWERK_PACKAGE_CHECK COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/need-lto.cmake")
endif()
add_custom_target(${BLEEPWERK_PACKAGE_TARGET}
    ${BLEEPWERK_PACKAGE_CHECK}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${BLEEPWERK_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BLEEPWERK_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:BleepWerk_CLAP> ${BLEEPWERK_PACKAGE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:BleepWerk_VST3>/../.. ${BLEEPWERK_PACKAGE_DIR}/BleepWerk.vst3
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${BLEEPWERK_PACKAGE_DIR}/LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.txt ${BLEEPWERK_PACKAGE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy ${BLEEPWERK_INSTALL_NOTES} ${BLEEPWERK_PACKAGE_DIR}/INSTALL.txt
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/package
            ${CMAKE_COMMAND} -E tar ${BLEEPWERK_TAR_FLAGS} ${CMAKE_SOURCE_DIR}/dist/${BLEEPWERK_PACKAGE_FILE} ${BLEEPWERK_PACKAGE_FORMAT} ${BLEEPWERK_PACKAGE_NAME}
    DEPENDS BleepWerk_CLAP BleepWerk_VST3
    COMMENT "Packaging dist/${BLEEPWERK_PACKAGE_FILE}"
    VERBATIM)
