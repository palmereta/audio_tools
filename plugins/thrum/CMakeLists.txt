# Thrum — guitarra eléctrica completa por modelado físico: cuerdas, pastillas, digitación, rasgueo, pedales,
# amplificador y gabinete (docs/research.md).
# El nombre del producto está sólo acá: cambiarlo es cambiar THRUM_NAME (los artefactos y las herramientas toman el
# nombre en minúsculas; los ids de los parámetros, los presets y el estado no dependen de él).
set(THRUM_NAME "Thrum")
set(THRUM_VERSION 1.0.0)
string(TOLOWER "${THRUM_NAME}" THRUM_LOWER)

if(APPLE)
    set(THRUM_FORMATS VST3 AU Standalone)
    set(THRUM_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(THRUM_FORMATS VST3 LV2 Standalone)
    set(THRUM_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(THRUM_FORMATS VST3 Standalone)
    set(THRUM_COPY FALSE)
endif()

file(GLOB THRUM_ASSETS CONFIGURE_DEPENDS assets/presets/*.json assets/patterns/*.json assets/ui/*.jpg assets/ui/*.png assets/ui/*.json)
# Barlow Condensed (OFL), la misma de Lacquer y Pallor.
list(APPEND THRUM_ASSETS ${CMAKE_SOURCE_DIR}/plugins/lacquer/assets/fonts/BarlowCondensed-Medium.ttf ${CMAKE_SOURCE_DIR}/plugins/lacquer/assets/fonts/BarlowCondensed-SemiBold.ttf)
juce_add_binary_data(ThrumAssets SOURCES ${THRUM_ASSETS})
file(GLOB_RECURSE THRUM_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(${THRUM_NAME}
    VERSION ${THRUM_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.${THRUM_NAME}"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Thrm
    PRODUCT_NAME "${THRUM_NAME}"
    PLUGIN_NAME "Audio Tools ${THRUM_NAME}"            # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${THRUM_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument" "Synth"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${THRUM_COPY})
juce_generate_juce_header(${THRUM_NAME})
target_sources(${THRUM_NAME} PRIVATE ${THRUM_SOURCES})
target_include_directories(${THRUM_NAME} PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(${THRUM_NAME} PUBLIC
    THRUM_VERSION="${THRUM_VERSION}" THRUM_PRODUCT="${THRUM_NAME}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(${THRUM_NAME}
    PRIVATE ThrumAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(${THRUM_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Pruebas numeradas del motor (sin JUCE).
add_executable(${THRUM_LOWER}test tools/thrumtest.cpp)
target_include_directories(${THRUM_LOWER}test PRIVATE Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(${THRUM_LOWER}test PRIVATE _USE_MATH_DEFINES=1)
if(NOT MSVC)
    target_compile_options(${THRUM_LOWER}test PRIVATE -ffp-contract=off)
endif()

# Render a WAV con el procesador completo (demos, CPU con --bench, IR de fábrica, .mid).
juce_add_console_app(${THRUM_LOWER}render VERSION ${THRUM_VERSION} PRODUCT_NAME "${THRUM_LOWER}render")
juce_generate_juce_header(${THRUM_LOWER}render)
target_sources(${THRUM_LOWER}render PRIVATE tools/render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(${THRUM_LOWER}render PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/manual)
target_compile_definitions(${THRUM_LOWER}render PRIVATE JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="${THRUM_NAME}"
    THRUM_PRODUCT="${THRUM_NAME}" THRUM_VERSION="${THRUM_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(${THRUM_LOWER}render PRIVATE ThrumAssets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
