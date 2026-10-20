# ATMD — recreación del Moog Minimoog Model D (unidad de mediados de los 70, placa de osciladores Rev 2).
set(ATMD_VERSION 1.0.2)

if(APPLE)
    set(ATMD_FORMATS VST3 AU Standalone)
    set(ATMD_COPY TRUE)
else()
    set(ATMD_FORMATS VST3 Standalone)
    set(ATMD_COPY FALSE)
endif()

file(GLOB ATMD_ASSETS CONFIGURE_DEPENDS assets/presets/*.json assets/ui/*)
juce_add_binary_data(AtmdAssets SOURCES ${ATMD_ASSETS})
file(GLOB_RECURSE ATMD_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(ATMD
    VERSION ${ATMD_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.ATMD"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Atmd
    PRODUCT_NAME "ATMD"
    PLUGIN_NAME "Audio Tools ATMD"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${ATMD_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument" "Synth"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${ATMD_COPY}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "ATMD's standalone app can use an audio input as the EXTERNAL INPUT of its mixer.")
juce_generate_juce_header(ATMD)
target_sources(ATMD PRIVATE ${ATMD_SOURCES})
target_include_directories(ATMD PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(ATMD PUBLIC
    ATMD_HAS_ASSETS=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(ATMD
    PRIVATE AtmdAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(ATMD ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Render offline: un preset tocado desde un MIDI (clips de la web, medición de CPU).
juce_add_console_app(atmdrender VERSION ${ATMD_VERSION} PRODUCT_NAME "atmdrender")
juce_generate_juce_header(atmdrender)
target_sources(atmdrender PRIVATE tools/render.cpp)
target_include_directories(atmdrender PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/tools)
target_compile_definitions(atmdrender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(atmdrender PRIVATE juce::juce_audio_formats juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
