# Lacquer — masterizador automático (análisis + cadena de mastering con reglas propias; docs/research.md).
set(LACQUER_VERSION 1.0.3)

if(APPLE)
    set(LACQUER_FORMATS VST3 AU Standalone)
    set(LACQUER_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LACQUER_FORMATS VST3 LV2 Standalone)
    set(LACQUER_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(LACQUER_FORMATS VST3 Standalone)
    set(LACQUER_COPY FALSE)
endif()

file(GLOB LACQUER_ASSETS CONFIGURE_DEPENDS assets/fonts/*.ttf)
juce_add_binary_data(LacquerAssets SOURCES ${LACQUER_ASSETS})
file(GLOB_RECURSE LACQUER_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

juce_add_plugin(Lacquer
    VERSION ${LACQUER_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.Lacquer"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Lacq
    PRODUCT_NAME "Lacquer"
    PLUGIN_NAME "Audio Tools Lacquer"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${LACQUER_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Fx" "Mastering" "Dynamics"
    AU_MAIN_TYPE kAudioUnitType_Effect
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${LACQUER_COPY}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Lacquer's standalone app masters what comes into the selected audio input.")
juce_generate_juce_header(Lacquer)
target_sources(Lacquer PRIVATE ${LACQUER_SOURCES})
target_include_directories(Lacquer PRIVATE Source Source/dsp)
target_compile_definitions(Lacquer PUBLIC
    LACQUER_HAS_ASSETS=1 LACQUER_VERSION="${LACQUER_VERSION}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
target_link_libraries(Lacquer
    PRIVATE LacquerAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
dh_add_manual(Lacquer ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)

# Render de archivos en dos pasadas y las pruebas del motor: sin JUCE, el mismo código que el plugin.
add_executable(lacquerrender tools/render.cpp)
target_include_directories(lacquerrender PRIVATE Source/dsp tools)
target_compile_definitions(lacquerrender PRIVATE LACQUER_VERSION="${LACQUER_VERSION}" _USE_MATH_DEFINES=1)
add_executable(lacquertest tools/mastertest.cpp)
target_include_directories(lacquertest PRIVATE Source/dsp tools)
target_compile_definitions(lacquertest PRIVATE _USE_MATH_DEFINES=1)
if(NOT MSVC)
    target_compile_options(lacquerrender PRIVATE -ffp-contract=off)
    target_compile_options(lacquertest PRIVATE -ffp-contract=off)
endif()

# Flujo en vivo del procesador (LEARN, traspaso de cadena, A/B, estado) sin host.
juce_add_console_app(lacquerlivetest VERSION ${LACQUER_VERSION} PRODUCT_NAME "lacquerlivetest")
juce_generate_juce_header(lacquerlivetest)
target_sources(lacquerlivetest PRIVATE tools/livetest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(lacquerlivetest PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/manual)
target_compile_definitions(lacquerlivetest PRIVATE LACQUER_HAS_ASSETS=1 LACQUER_VERSION="${LACQUER_VERSION}" JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 _USE_MATH_DEFINES=1)
target_link_libraries(lacquerlivetest PRIVATE LacquerAssets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
