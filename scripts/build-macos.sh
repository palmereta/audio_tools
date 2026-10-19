#!/bin/sh
# Builds every Audio Tools plugin on macOS (universal arm64 + x86_64) and packages them.
# Requirements: Xcode Command Line Tools, CMake ≥ 3.22, Ninja.
set -e
cd "$(dirname "$0")/.."
[ -f external/JUCE/CMakeLists.txt ] || git submodule update --init --depth 1 external/JUCE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
sh scripts/package-macos.sh
