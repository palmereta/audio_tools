# Plumb — corrección de altura en tiempo real con latencia muy baja (docs/research.md).
set(PLUMB_VERSION 1.0.1)

if(APPLE)
    set(PLUMB_FORMATS VST3 AU Standalone)
    set(PLUMB_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLUMB_FORMATS VST3 LV2 Standalone)
    set(PLUMB_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(PLUMB_FORMATS VST3 Standalone)
    set(PLUMB_COPY FALSE)
endif()

file(GLOB PLUMB_ASSETS CONFIGURE_DEPENDS assets/presets/*.json assets/ui/*.png assets/ui/*.jpg assets/ui/*.json)
juce_add_binary_data(PlumbAssets SOURCES ${PLUMB_ASSETS})
file(GLOB_RECURSE PLUMB_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(Plumb
    VERSION ${PLUMB_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.Plumb"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Plmb
    PRODUCT_NAME "Plumb"
    PLUGIN_NAME "Audio Tools Plumb"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${PLUMB_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Fx" "Pitch Shift"
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${PLUMB_COPY}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Plumb's standalone app corrects the pitch of the selected audio input.")
juce_generate_juce_header(Plumb)
target_sources(Plumb PRIVATE ${PLUMB_SOURCES})
target_include_directories(Plumb PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/tools)
target_compile_definitions(Plumb PUBLIC
    PLUMB_VERSION="${PLUMB_VERSION}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(Plumb
    PRIVATE PlumbAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(Plumb ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Pruebas del motor (sin JUCE).
add_executable(plumbtest tools/plumbtest.cpp)
target_include_directories(plumbtest PRIVATE Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(plumbtest PRIVATE _USE_MATH_DEFINES=1)
if(NOT MSVC)
    target_compile_options(plumbtest PRIVATE -ffp-contract=off)
endif()

# Render de un archivo con un preset y su patrón, con el procesador completo (clips de la web, CPU).
juce_add_console_app(plumbrender VERSION ${PLUMB_VERSION} PRODUCT_NAME "plumbrender")
juce_generate_juce_header(plumbrender)
target_sources(plumbrender PRIVATE tools/render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(plumbrender PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/manual ${CMAKE_SOURCE_DIR}/shared/tools)
target_compile_definitions(plumbrender PRIVATE JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="Plumb"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(plumbrender PRIVATE PlumbAssets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
