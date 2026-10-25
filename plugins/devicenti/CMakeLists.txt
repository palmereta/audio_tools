# DeviceNTI V (slug devicenti; 1.0.0 was called DeVicenti): an FM groovebox for downtempo, trip hop, acid jazz, ambient and dark ambient. Four tracks (four-operator
# FM voice with sub, a drum machine of eight FM lanes, or MIDI), a 64-step sequencer with step locks, conditions,
# chord mode and chord memory, per-track inserts, delay and reverb sends and a master bus (docs/research.md).
set(DEVICENTI_VERSION 1.0.1)

if(APPLE)
    set(DEVICENTI_FORMATS VST3 AU Standalone)
    set(DEVICENTI_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(DEVICENTI_FORMATS VST3 LV2 Standalone)
    set(DEVICENTI_COPY FALSE)   # Linux: installed separately (~/.vst3, ~/.lv2)
else()
    set(DEVICENTI_FORMATS VST3 Standalone)
    set(DEVICENTI_COPY FALSE)
endif()

file(GLOB DEVICENTI_ASSETS CONFIGURE_DEPENDS assets/presets/*.json assets/ui/*.png assets/ui/*.jpg assets/ui/*.json assets/fonts/ArchivoCondensed-*.ttf)
juce_add_binary_data(DeVicentiAssets SOURCES ${DEVICENTI_ASSETS})
file(GLOB_RECURSE DEVICENTI_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(DeVicenti
    VERSION ${DEVICENTI_VERSION}
    COMPANY_NAME "Audio Tools"
    # Renamed in 1.0.1. What a host or a saved project identifies the plugin by stays as in 1.0.0: the bundle id, the
    # manufacturer and plugin codes (the VST3 class id and the AU type/subtype/manufacturer come from them) and the LV2
    # URI (by default it would follow PLUGIN_NAME), so projects from 1.0.0 open unchanged.
    BUNDLE_ID "com.audiotools.DeVicenti"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Dvct
    LV2URI "https://audiotools.com.ar/plugins/Audio_Tools_DeVicenti"
    PRODUCT_NAME "DeviceNTI V"
    PLUGIN_NAME "Audio Tools DeviceNTI V"           # the name the host shows: in Ableton they are recognised by the prefix
    FORMATS ${DEVICENTI_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument" "Synth"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${DEVICENTI_COPY})
juce_generate_juce_header(DeVicenti)
target_sources(DeVicenti PRIVATE ${DEVICENTI_SOURCES})
target_include_directories(DeVicenti PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(DeVicenti PUBLIC
    DV_VERSION="${DEVICENTI_VERSION}"
    # JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS stays on: the plugin reads pitch bend, CCs (its CC map, learn) and
    # the sustain pedal, and without it a VST3 does not receive All Notes Off either.
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(DeVicenti
    PRIVATE DeVicentiAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(DeVicenti ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Engine tests (no JUCE).
add_executable(devicentitest tools/devicentitest.cpp)
target_include_directories(devicentitest PRIVATE Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(devicentitest PRIVATE _USE_MATH_DEFINES=1)
if(NOT MSVC)
    target_compile_options(devicentitest PRIVATE -O2 -ffp-contract=off)
endif()

# The full processor to WAV: demos, CPU, processor tests (MIDI in/out, state).
juce_add_console_app(devicentirender VERSION ${DEVICENTI_VERSION} PRODUCT_NAME "devicentirender")
juce_generate_juce_header(devicentirender)
file(GLOB DEVICENTI_UI_SOURCES CONFIGURE_DEPENDS Source/ui/*.cpp)
target_sources(devicentirender PRIVATE tools/render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp ${DEVICENTI_UI_SOURCES})
target_include_directories(devicentirender PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/manual)
target_compile_definitions(devicentirender PRIVATE JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="DeviceNTI V"
    DV_VERSION="${DEVICENTI_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(devicentirender PRIVATE DeVicentiAssets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

# The tools build the engine and the processor on the stack in places (the project is ~1.2 MB): on Windows the
# default stack is 1 MB.
if(MSVC)
    target_link_options(devicentitest PRIVATE /STACK:33554432)
    target_link_options(devicentirender PRIVATE /STACK:33554432)
endif()
