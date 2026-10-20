# AT700 — consola modular basada en el Roland System-700 (1976), teclado 701A y secuenciador 717A.
set(AT700_VERSION 1.0.4)

if(APPLE)
    set(AT700_FORMATS VST3 AU Standalone)
    set(AT700_COPY TRUE)
else()
    set(AT700_FORMATS VST3 Standalone)
    set(AT700_COPY FALSE)
endif()

# Imágenes renderizadas en Blender y presets, embebidos en el binario.
file(GLOB AT700_ASSETS CONFIGURE_DEPENDS assets/ui/*.png assets/ui/*.jpg assets/ui/*.json assets/presets/*.json)
juce_add_binary_data(AT700Assets SOURCES ${AT700_ASSETS})
file(GLOB_RECURSE AT700_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(AT700
    VERSION ${AT700_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.AT700"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE At70
    PRODUCT_NAME "AT700"
    PLUGIN_NAME "Audio Tools AT700"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${AT700_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument;Synth"
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${AT700_COPY}
    MICROPHONE_PERMISSION_ENABLED FALSE)
juce_generate_juce_header(AT700)
target_sources(AT700 PRIVATE ${AT700_SOURCES})
target_include_directories(AT700 PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/ui)
target_compile_definitions(AT700 PUBLIC
    AT700_HAS_ASSETS=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(AT700
    PRIVATE AT700Assets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(AT700 ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Render offline sin host (tools/render.cpp).
juce_add_console_app(at700render VERSION ${AT700_VERSION} PRODUCT_NAME "at700render")
juce_generate_juce_header(at700render)
target_sources(at700render PRIVATE tools/render.cpp)
target_include_directories(at700render PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(at700render PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(at700render PRIVATE juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
