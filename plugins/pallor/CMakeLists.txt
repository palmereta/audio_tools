# Pallor — caja de ritmos para dark wave, coldwave, post-punk, EBM y afines: 16 voces con motor analógico y digital,
# efectos de los 80/90 y secuenciador (docs/research.md).
# El nombre del producto está sólo acá: cambiarlo antes de publicar es cambiar PALLOR_NAME (y los artefactos se
# llaman igual; los ids de los parámetros, los presets y el estado no dependen de él).
set(PALLOR_NAME "Pallor")
set(PALLOR_VERSION 1.0.1)

if(APPLE)
    set(PALLOR_FORMATS VST3 AU Standalone)
    set(PALLOR_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PALLOR_FORMATS VST3 LV2 Standalone)
    set(PALLOR_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(PALLOR_FORMATS VST3 Standalone)
    set(PALLOR_COPY FALSE)
endif()

file(GLOB PALLOR_ASSETS CONFIGURE_DEPENDS assets/presets/*.json assets/patterns/*.json assets/ui/*.jpg assets/ui/*.png assets/ui/*.json)
# Barlow Condensed (OFL): la misma tipografía de Lacquer, sin copiarla.
list(APPEND PALLOR_ASSETS ${CMAKE_SOURCE_DIR}/plugins/lacquer/assets/fonts/BarlowCondensed-Medium.ttf ${CMAKE_SOURCE_DIR}/plugins/lacquer/assets/fonts/BarlowCondensed-SemiBold.ttf)
juce_add_binary_data(PallorAssets SOURCES ${PALLOR_ASSETS})
file(GLOB_RECURSE PALLOR_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(Pallor
    VERSION ${PALLOR_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.${PALLOR_NAME}"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Pllr
    PRODUCT_NAME "${PALLOR_NAME}"
    PLUGIN_NAME "Audio Tools ${PALLOR_NAME}"           # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${PALLOR_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument" "Drum"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${PALLOR_COPY})
juce_generate_juce_header(Pallor)
target_sources(Pallor PRIVATE ${PALLOR_SOURCES})
target_include_directories(Pallor PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(Pallor PUBLIC
    PALLOR_VERSION="${PALLOR_VERSION}" PALLOR_PRODUCT="${PALLOR_NAME}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(Pallor
    PRIVATE PallorAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(Pallor ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Pruebas del motor (sin JUCE).
add_executable(pallortest tools/pallortest.cpp)
target_include_directories(pallortest PRIVATE Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(pallortest PRIVATE _USE_MATH_DEFINES=1)
if(NOT MSVC)
    target_compile_options(pallortest PRIVATE -ffp-contract=off)
endif()

# Render de un preset o patrón a WAV con el procesador completo (demos, CPU, .mid).
juce_add_console_app(pallorrender VERSION ${PALLOR_VERSION} PRODUCT_NAME "pallorrender")
juce_generate_juce_header(pallorrender)
target_sources(pallorrender PRIVATE tools/render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(pallorrender PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/manual)
target_compile_definitions(pallorrender PRIVATE JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="${PALLOR_NAME}" PALLOR_VERSION="${PALLOR_VERSION}"
    PALLOR_PRODUCT="${PALLOR_NAME}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(pallorrender PRIVATE PallorAssets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

# Las herramientas crean el motor y el procesador en la pila (un banco de patrones son ~400 KB): en Windows la pila
# por defecto es de 1 MB.
if(MSVC)
    target_link_options(pallortest PRIVATE /STACK:33554432)
    target_link_options(pallorrender PRIVATE /STACK:33554432)
endif()
