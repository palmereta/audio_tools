# ATFS1 — síntesis de formantes y FM basada en el Yamaha FS1R (1998).
set(ATFS1_VERSION 1.0.4)

if(APPLE)
    set(ATFS1_FORMATS VST3 AU Standalone)
    set(ATFS1_COPY TRUE)
else()
    set(ATFS1_FORMATS VST3 Standalone)
    set(ATFS1_COPY FALSE)
endif()

# Frente renderizado en Blender, presets y Formant Sequences de fábrica, embebidos en el binario.
file(GLOB ATFS1_ASSETS CONFIGURE_DEPENDS assets/ui/*.png assets/ui/*.jpg assets/ui/*.json assets/presets/*.json assets/fseq/*.json)
juce_add_binary_data(ATFS1Assets SOURCES ${ATFS1_ASSETS})
file(GLOB_RECURSE ATFS1_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(ATFS1
    VERSION ${ATFS1_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.ATFS1"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Atfs
    PRODUCT_NAME "ATFS1"
    PLUGIN_NAME "Audio Tools ATFS1"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${ATFS1_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument;Synth"
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${ATFS1_COPY}
    MICROPHONE_PERMISSION_ENABLED FALSE)
juce_generate_juce_header(ATFS1)
target_sources(ATFS1 PRIVATE ${ATFS1_SOURCES})
target_include_directories(ATFS1 PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/ui)
target_compile_definitions(ATFS1 PUBLIC
    ATFS1_HAS_ASSETS=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(ATFS1
    PRIVATE ATFS1Assets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(ATFS1 ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Render offline sin host (tools/render.cpp) y generador de Fseqs de fábrica.
juce_add_console_app(atfs1render VERSION ${ATFS1_VERSION} PRODUCT_NAME "atfs1render")
juce_generate_juce_header(atfs1render)
target_sources(atfs1render PRIVATE tools/render.cpp)
target_include_directories(atfs1render PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(atfs1render PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(atfs1render PRIVATE juce::juce_audio_formats juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
