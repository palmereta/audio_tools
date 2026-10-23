# Murmur — granular en tiempo real: la entrada en un búfer, granos que vuelan como una bandada.
set(MURMUR_VERSION 1.0.4)

if(APPLE)
    set(MURMUR_FORMATS VST3 AU Standalone)
    set(MURMUR_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MURMUR_FORMATS VST3 LV2 Standalone)
    set(MURMUR_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(MURMUR_FORMATS VST3 Standalone)
    set(MURMUR_COPY FALSE)
endif()

file(GLOB MURMUR_ASSETS CONFIGURE_DEPENDS assets/presets/*.json)
juce_add_binary_data(MurmurAssets SOURCES ${MURMUR_ASSETS})
file(GLOB_RECURSE MURMUR_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(Murmur
    VERSION ${MURMUR_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.Murmur"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Mrmr
    PRODUCT_NAME "Murmur"
    PLUGIN_NAME "Audio Tools Murmur"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${MURMUR_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Fx" "Delay"
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${MURMUR_COPY}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Murmur's standalone app records the input you choose to turn it into grains.")
juce_generate_juce_header(Murmur)
target_sources(Murmur PRIVATE ${MURMUR_SOURCES})
target_include_directories(Murmur PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(Murmur PUBLIC
    MURMUR_HAS_ASSETS=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(Murmur
    PRIVATE MurmurAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(Murmur ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Render offline: un WAV pasado por un preset (clips de la web, medición de CPU).
juce_add_console_app(murmurrender VERSION ${MURMUR_VERSION} PRODUCT_NAME "murmurrender")
juce_generate_juce_header(murmurrender)
target_sources(murmurrender PRIVATE tools/render.cpp)
target_include_directories(murmurrender PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(murmurrender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(murmurrender PRIVATE juce::juce_audio_formats juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
