# AT303 — bajo de un oscilador con escalera de diodos, secuenciador de dos listas y generador ácido (docs/research.md).
set(AT303_VERSION 1.0.2)

if(APPLE)
    set(AT303_FORMATS VST3 AU Standalone)
    set(AT303_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(AT303_FORMATS VST3 LV2 Standalone)
    set(AT303_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(AT303_FORMATS VST3 Standalone)
    set(AT303_COPY FALSE)
endif()

file(GLOB AT303_ASSETS CONFIGURE_DEPENDS assets/presets/*.json assets/ui/*.png assets/ui/*.jpg assets/ui/*.json)
juce_add_binary_data(At303Assets SOURCES ${AT303_ASSETS})
file(GLOB_RECURSE AT303_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(AT303
    VERSION ${AT303_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.AT303"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE At33
    PRODUCT_NAME "AT303"
    PLUGIN_NAME "Audio Tools AT303"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${AT303_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument" "Synth"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${AT303_COPY})
juce_generate_juce_header(AT303)
target_sources(AT303 PRIVATE ${AT303_SOURCES})
target_include_directories(AT303 PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(AT303 PUBLIC
    AT303_VERSION="${AT303_VERSION}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(AT303
    PRIVATE At303Assets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(AT303 ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Pruebas del motor (sin JUCE).
add_executable(at303test tools/at303test.cpp)
target_include_directories(at303test PRIVATE Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(at303test PRIVATE _USE_MATH_DEFINES=1)
if(NOT MSVC)
    target_compile_options(at303test PRIVATE -ffp-contract=off)
endif()

# Render de un archivo con un preset y su patrón, con el procesador completo (clips de la web, CPU).
juce_add_console_app(at303render VERSION ${AT303_VERSION} PRODUCT_NAME "at303render")
juce_generate_juce_header(at303render)
target_sources(at303render PRIVATE tools/render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(at303render PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/manual)
target_compile_definitions(at303render PRIVATE JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="AT303"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(at303render PRIVATE At303Assets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
