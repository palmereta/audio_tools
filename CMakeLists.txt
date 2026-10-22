cmake_minimum_required(VERSION 3.22)

# Binario universal en macOS (Apple Silicon e Intel). Tiene que ir antes de project();
# en Windows estas variables se ignoran.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
project(AudioTools LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    # Runtime estático: el .vst3 no depende del VC++ Redistributable instalado.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # /fp:fast: ~25 % menos CPU en los motores (medido en tools/bench). Sin /arch:AVX2, que no
    # aportaba y dejaría afuera procesadores anteriores a 2013.
    add_compile_options("$<$<CONFIG:Release>:/fp:fast>")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# JUCE 9.0.4 como submódulo (git submodule update --init); si falta, se descarga.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt)
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Linux: JUCE arma la URI de cada LV2 con el sitio (https://audiotools.com.ar/plugins/Audio_Tools_<nombre>), y las
# standalone y el launcher ofrecen JACK además de ALSA (libjack se carga al abrir, no es dependencia del binario).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_property(GLOBAL PROPERTY JUCE_COMPANY_WEBSITE "https://audiotools.com.ar")
    add_compile_definitions(JUCE_JACK=1)
endif()

# El manual dentro de cada plugin (shared/manual).
include(shared/manual/manual.cmake)

# Un subdirectorio por instrumento. Cada uno lleva su versión en su propio CMakeLists.
add_subdirectory(plugins/at20v)
add_subdirectory(plugins/at101v)
add_subdirectory(plugins/at42)
add_subdirectory(plugins/at700)
add_subdirectory(plugins/atfs1)
add_subdirectory(plugins/orrery)
add_subdirectory(plugins/murmur)
add_subdirectory(plugins/atmd)
add_subdirectory(plugins/lacquer)
add_subdirectory(plugins/halation)
add_subdirectory(plugins/tarnish)
add_subdirectory(plugins/at303)
add_subdirectory(plugins/plumb)
add_subdirectory(plugins/pallor)
add_subdirectory(plugins/thrum)

# Pruebas de los plugins compilados, como un DAW: tools/hosttest (athosttest).
add_subdirectory(tools/hosttest)

# El launcher: descarga, instala y actualiza los plugins desde web/downloads/manifest.json.
add_subdirectory(tools/launcher)
