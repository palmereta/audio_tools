# Halation — delay y reverb en un solo espacio (red de realimentación que va de ecos a sala; docs/research.md).
set(HALATION_VERSION 1.0.1)

if(APPLE)
    set(HALATION_FORMATS VST3 AU Standalone)
    set(HALATION_COPY TRUE)
else()
    set(HALATION_FORMATS VST3 Standalone)
    set(HALATION_COPY FALSE)
endif()

file(GLOB HALATION_ASSETS CONFIGURE_DEPENDS assets/presets/*.json assets/fonts/*.ttf)
juce_add_binary_data(HalationAssets SOURCES ${HALATION_ASSETS})
file(GLOB_RECURSE HALATION_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(Halation
    VERSION ${HALATION_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.Halation"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Halo
    PRODUCT_NAME "Halation"
    PLUGIN_NAME "Audio Tools Halation"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${HALATION_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Fx" "Delay" "Reverb"
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${HALATION_COPY}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Halation's standalone app processes the selected audio input.")
juce_generate_juce_header(Halation)
target_sources(Halation PRIVATE ${HALATION_SOURCES})
target_include_directories(Halation PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/plugins/lacquer/Source/dsp)
target_compile_definitions(Halation PUBLIC
    HALATION_HAS_ASSETS=1 HALATION_VERSION="${HALATION_VERSION}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(Halation
    PRIVATE HalationAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(Halation ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Pruebas del motor (sin JUCE).
add_executable(halationtest tools/spacetest.cpp)
target_include_directories(halationtest PRIVATE Source/dsp ${CMAKE_SOURCE_DIR}/plugins/lacquer/Source/dsp)
target_compile_definitions(halationtest PRIVATE _USE_MATH_DEFINES=1)
if(NOT MSVC)
    target_compile_options(halationtest PRIVATE -ffp-contract=off)
endif()

# Render de un archivo con un preset, con el procesador completo (clips de la web, CPU).
juce_add_console_app(halationrender VERSION ${HALATION_VERSION} PRODUCT_NAME "halationrender")
juce_generate_juce_header(halationrender)
target_sources(halationrender PRIVATE tools/render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(halationrender PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/plugins/lacquer/Source/dsp ${CMAKE_SOURCE_DIR}/shared/manual)
target_compile_definitions(halationrender PRIVATE HALATION_HAS_ASSETS=1 JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="Halation"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(halationrender PRIVATE HalationAssets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
