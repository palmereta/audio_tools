# Orrery — generador de MIDI para ambient, drone y música tranquila: ocho carriles en órbita.
set(ORRERY_VERSION 1.0.5)

if(APPLE)
    set(ORRERY_FORMATS VST3 AU Standalone)
    set(ORRERY_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ORRERY_FORMATS VST3 LV2 Standalone)
    set(ORRERY_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(ORRERY_FORMATS VST3 Standalone)
    set(ORRERY_COPY FALSE)
endif()

file(GLOB ORRERY_ASSETS CONFIGURE_DEPENDS assets/presets/*.json)
juce_add_binary_data(OrreryAssets SOURCES ${ORRERY_ASSETS})
file(GLOB_RECURSE ORRERY_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(Orrery
    VERSION ${ORRERY_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.Orrery"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Orry
    PRODUCT_NAME "Orrery"
    PLUGIN_NAME "Audio Tools Orrery"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${ORRERY_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument;Generator"
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${ORRERY_COPY}
    MICROPHONE_PERMISSION_ENABLED FALSE)
juce_generate_juce_header(Orrery)
target_sources(Orrery PRIVATE ${ORRERY_SOURCES})
target_include_directories(Orrery PRIVATE Source Source/gen ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(Orrery PUBLIC
    ORRERY_HAS_ASSETS=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(Orrery
    PRIVATE OrreryAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(Orrery ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Render offline: el MIDI de cada carril y el audio del sonido de prueba.
juce_add_console_app(orreryrender VERSION ${ORRERY_VERSION} PRODUCT_NAME "orreryrender")
juce_generate_juce_header(orreryrender)
target_sources(orreryrender PRIVATE tools/render.cpp)
target_include_directories(orreryrender PRIVATE Source Source/gen ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(orreryrender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(orreryrender PRIVATE juce::juce_audio_formats juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Prueba de las salidas MIDI directas con un puerto virtual (sólo macOS).
if(APPLE)
    juce_add_console_app(orrerymiditest VERSION ${ORRERY_VERSION} PRODUCT_NAME "orrerymiditest")
    juce_generate_juce_header(orrerymiditest)
    target_sources(orrerymiditest PRIVATE tools/miditest.cpp Source/PluginProcessor.cpp)
    target_include_directories(orrerymiditest PRIVATE Source Source/gen ${CMAKE_SOURCE_DIR}/shared/dsp)
    target_compile_definitions(orrerymiditest PRIVATE ORRERY_HAS_ASSETS=1 ORRERY_NO_EDITOR=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
    target_link_libraries(orrerymiditest PRIVATE OrreryAssets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
endif()

# La misma variación tiene que dar la misma música en macOS y Windows: el generador usa su propia
# matemática (Theory.h, dm::) y se compila sin contracción a FMA ni /fp:fast (que reordena operaciones).
if(MSVC)
    set(ORRERY_FP /fp:precise)
else()
    set(ORRERY_FP -ffp-contract=off)
endif()
foreach(t Orrery orreryrender)
    target_compile_options(${t} PRIVATE ${ORRERY_FP})
endforeach()
if(APPLE)
    target_compile_options(orrerymiditest PRIVATE ${ORRERY_FP})
endif()
