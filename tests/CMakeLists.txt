# Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
# SPDX-License-Identifier: AGPL-3.0-only

# Unit tests, run with ctest. All but test_hardening are JUCE-free and test the DSP, MIDI input
# and dice on their own.
# Add a name here for each tests/test_<name>.cpp. BleepWerk_Tests builds them all, so CI needs
# no list of its own.
set(BLEEPWERK_TESTS bleep note_input werk sequencer effects presets randomizer themes tooltips)
add_custom_target (BleepWerk_Tests)
foreach (name ${BLEEPWERK_TESTS})
    add_executable (test_${name} test_${name}.cpp)
    target_include_directories (test_${name} PRIVATE ${PROJECT_SOURCE_DIR}/Source)
    target_compile_features (test_${name} PRIVATE cxx_std_17)
    add_test (NAME ${name} COMMAND test_${name})
    add_dependencies (BleepWerk_Tests test_${name})
endforeach()

# Damaged or hostile projects and files through the real processor: links the plug-in code and
# JUCE (the BleepWerk target is defined after this folder, which CMake allows). One ctest test
# per case, each its own run of test_hardening.
add_executable (test_hardening test_hardening.cpp)
target_include_directories (test_hardening PRIVATE ${PROJECT_SOURCE_DIR}/Source $<TARGET_PROPERTY:BleepWerk,INCLUDE_DIRECTORIES>)
target_compile_definitions (test_hardening PRIVATE $<TARGET_PROPERTY:BleepWerk,COMPILE_DEFINITIONS>)
target_link_libraries (test_hardening PRIVATE BleepWerk)
add_dependencies (BleepWerk_Tests test_hardening)
foreach (case not-numbers open-folder huge-xml typed-numbers nameless-preset migrate-old-folders export-layers new-instance-defaults pattern-sets held-key-shows-root preset-search reset-patterns)
    add_test (NAME hardening-${case} COMMAND test_hardening ${case})
endforeach()

# The CI workflow pins its dependencies to exact commits.
add_test (NAME ci-pins COMMAND ${CMAKE_COMMAND} -DWORKFLOW=${PROJECT_SOURCE_DIR}/.github/workflows/build.yml
                               -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ci_pins.cmake)
