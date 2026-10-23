# AT330 — cuerdas, coro y vocoder de 10 bandas con ensemble de tres BBD, basado en el Roland VP-330 (1979)
# (docs/research.md).
set(AT330_VERSION 1.0.0)

if(APPLE)
    set(AT330_FORMATS VST3 AU Standalone)
    set(AT330_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(AT330_FORMATS VST3 LV2 Standalone)
    set(AT330_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(AT330_FORMATS VST3 Standalone)
    set(AT330_COPY FALSE)
endif()

file(GLOB AT330_ASSETS CONFIGURE_DEPENDS assets/presets/*.json assets/ui/*.png assets/ui/*.jpg assets/ui/*.json)
# Barlow Condensed (OFL): la misma tipografía de Lacquer y Pallor, sin copiarla.
list(APPEND AT330_ASSETS ${CMAKE_SOURCE_DIR}/plugins/lacquer/assets/fonts/BarlowCondensed-Medium.ttf ${CMAKE_SOURCE_DIR}/plugins/lacquer/assets/fonts/BarlowCondensed-SemiBold.ttf)
juce_add_binary_data(At330Assets SOURCES ${AT330_ASSETS})
file(GLOB_RECURSE AT330_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

# La entrada de audio es el modulador del vocoder (la voz, por sidechain) y, si se elige, el portador externo: el
# mismo bus opcional del AT20V, ya probado en Live y REAPER.
juce_add_plugin(AT330
    VERSION ${AT330_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.AT330"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE At3v
    PRODUCT_NAME "AT330"
    PLUGIN_NAME "Audio Tools AT330"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${AT330_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument" "Synth"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${AT330_COPY}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "AT330 uses the audio input as the voice of the vocoder.")
juce_generate_juce_header(AT330)
target_sources(AT330 PRIVATE ${AT330_SOURCES})
target_include_directories(AT330 PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(AT330 PUBLIC
    AT330_VERSION="${AT330_VERSION}"
    # JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS queda prendido: el AT330 lee pitch bend, rueda de modulación y pedal
    # de sostén, y sin él un VST3 tampoco recibe All Notes Off / All Sound Off (QA en DAWs 2026-10-22, X4).
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(AT330
    PRIVATE At330Assets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(AT330 ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Pruebas numeradas del motor (sin JUCE) y del procesador completo sin host (hueco del host, programa 0, estado,
# reset, DC y nivel de los presets).
juce_add_console_app(at330test VERSION ${AT330_VERSION} PRODUCT_NAME "at330test")
juce_generate_juce_header(at330test)
target_sources(at330test PRIVATE tools/at330test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(at330test PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/manual)
target_compile_definitions(at330test PRIVATE JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="AT330"
    AT330_VERSION="${AT330_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(at330test PRIVATE At330Assets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
if(NOT MSVC)
    target_compile_options(at330test PRIVATE -ffp-contract=off)
endif()

# Render con el procesador completo: un preset tocando un .mid (o acordes de prueba), con la voz del vocoder desde un
# WAV (demos, clips de la web, CPU con --bench).
juce_add_console_app(at330render VERSION ${AT330_VERSION} PRODUCT_NAME "at330render")
juce_generate_juce_header(at330render)
target_sources(at330render PRIVATE tools/render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(at330render PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/manual)
target_compile_definitions(at330render PRIVATE JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="AT330"
    AT330_VERSION="${AT330_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(at330render PRIVATE At330Assets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
