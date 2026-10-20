# Tarnish — cinta, casete, video, vinilo, radio AM y teléfono, con histéresis magnética (docs/research.md).
set(TARNISH_VERSION 1.0.2)

if(APPLE)
    set(TARNISH_FORMATS VST3 AU Standalone)
    set(TARNISH_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TARNISH_FORMATS VST3 LV2 Standalone)
    set(TARNISH_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(TARNISH_FORMATS VST3 Standalone)
    set(TARNISH_COPY FALSE)
endif()

file(GLOB TARNISH_ASSETS CONFIGURE_DEPENDS assets/presets/*.json assets/fonts/*.ttf)
juce_add_binary_data(TarnishAssets SOURCES ${TARNISH_ASSETS})
file(GLOB_RECURSE TARNISH_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(Tarnish
    VERSION ${TARNISH_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.Tarnish"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Trns
    PRODUCT_NAME "Tarnish"
    PLUGIN_NAME "Audio Tools Tarnish"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${TARNISH_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Fx" "Distortion"
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${TARNISH_COPY}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Tarnish's standalone app processes the selected audio input.")
juce_generate_juce_header(Tarnish)
target_sources(Tarnish PRIVATE ${TARNISH_SOURCES})
target_include_directories(Tarnish PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/plugins/lacquer/Source/dsp)
target_compile_definitions(Tarnish PUBLIC
    TARNISH_HAS_ASSETS=1 TARNISH_VERSION="${TARNISH_VERSION}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(Tarnish
    PRIVATE TarnishAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(Tarnish ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Pruebas del motor (sin JUCE).
add_executable(tarnishtest tools/tarnishtest.cpp)
target_include_directories(tarnishtest PRIVATE Source/dsp ${CMAKE_SOURCE_DIR}/plugins/lacquer/Source/dsp)
target_compile_definitions(tarnishtest PRIVATE _USE_MATH_DEFINES=1)
if(NOT MSVC)
    target_compile_options(tarnishtest PRIVATE -ffp-contract=off)
endif()

# Render de un archivo con un preset, con el procesador completo (clips de la web, CPU).
juce_add_console_app(tarnishrender VERSION ${TARNISH_VERSION} PRODUCT_NAME "tarnishrender")
juce_generate_juce_header(tarnishrender)
target_sources(tarnishrender PRIVATE tools/render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(tarnishrender PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/plugins/lacquer/Source/dsp ${CMAKE_SOURCE_DIR}/shared/manual)
target_compile_definitions(tarnishrender PRIVATE TARNISH_HAS_ASSETS=1 JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="Tarnish"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(tarnishrender PRIVATE TarnishAssets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
