# Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
# SPDX-License-Identifier: AGPL-3.0-only

# Unit tests, run with ctest. All but test_hardening are JUCE-free and test the DSP, MIDI input
# and dice on their own.
# Add a name here for each tests/test_<name>.cpp. BleepForge_Tests builds them all, so CI needs
# no list of its own.
set(BLEEPFORGE_TESTS bleep note_input foley machine effects presets randomizer themes)
add_custom_target (BleepForge_Tests)
foreach (name ${BLEEPFORGE_TESTS})
    add_executable (test_${name} test_${name}.cpp)
    target_include_directories (test_${name} PRIVATE ${PROJECT_SOURCE_DIR}/Source)
    target_compile_features (test_${name} PRIVATE cxx_std_17)
    add_test (NAME ${name} COMMAND test_${name})
    add_dependencies (BleepForge_Tests test_${name})
endforeach()

# Damaged or hostile projects and files through the real processor: links the plug-in code and
# JUCE (the BleepForge target is defined after this folder, which CMake allows). One ctest test
# per case, each its own run of test_hardening.
add_executable (test_hardening test_hardening.cpp)
target_include_directories (test_hardening PRIVATE ${PROJECT_SOURCE_DIR}/Source $<TARGET_PROPERTY:BleepForge,INCLUDE_DIRECTORIES>)
target_compile_definitions (test_hardening PRIVATE $<TARGET_PROPERTY:BleepForge,COMPILE_DEFINITIONS>)
target_link_libraries (test_hardening PRIVATE BleepForge)
add_dependencies (BleepForge_Tests test_hardening)
foreach (case not-numbers open-folder huge-xml typed-numbers)
    add_test (NAME hardening-${case} COMMAND test_hardening ${case})
endforeach()
