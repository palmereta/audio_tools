# Renders extra for the Valence demos: the AT303 and the ATMD played from a .mid with CC rows mapped to their knobs
# (at303render only plays the preset's own pattern, and atmdrender reads only CC 1 and 64). Nothing in the repo's
# CMakeLists changes: the file is injected into the root project from the command line and defers its targets to the
# end of the root directory, when At303Assets and the rest already exist.
#
#   cmake -S . -B build -DCMAKE_PROJECT_AudioTools_INCLUDE=$PWD/plugins/valence/tools/demos/extra.cmake ...
#   cmake --build build --target at303midirender atmdmidirender
set(VALENCE_DEMOS_DIR ${CMAKE_CURRENT_LIST_DIR})
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL include ${VALENCE_DEMOS_DIR}/targets.cmake)
