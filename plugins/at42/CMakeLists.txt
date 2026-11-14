# AT42 — máquina de drones basada en la familia Elta SOLAR 42 (2023-2025), superset 42N.
set(AT42_VERSION 1.0.8)

if(AT42_HEADLESS)
    set(AT42_FORMATS VST VST3 LV2)
    set(AT42_COPY FALSE)
    set(AT42_VST3_MANIFEST FALSE)
elseif(APPLE)
    set(AT42_FORMATS VST VST3 AU Standalone)
    set(AT42_COPY FALSE)
    set(AT42_VST3_MANIFEST TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(AT42_FORMATS VST VST3 LV2 Standalone)
    set(AT42_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
    set(AT42_VST3_MANIFEST TRUE)
else()
    set(AT42_FORMATS VST3 Standalone)
    set(AT42_COPY FALSE)
    set(AT42_VST3_MANIFEST TRUE)
endif()

# Imágenes renderizadas en Blender y presets, embebidos en el binario.
if(AT42_HEADLESS)
    file(GLOB AT42_ASSETS CONFIGURE_DEPENDS assets/presets/*.json)
    set(AT42_SOURCES Source/PluginProcessor.cpp)
else()
    file(GLOB AT42_ASSETS CONFIGURE_DEPENDS assets/ui/*.png assets/ui/*.jpg assets/ui/*.json assets/presets/*.json)
    file(GLOB_RECURSE AT42_SOURCES CONFIGURE_DEPENDS Source/*.cpp)
endif()
juce_add_binary_data(AT42Assets SOURCES ${AT42_ASSETS})

juce_add_plugin(AT42
    VERSION ${AT42_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.AT42"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE At42
    PRODUCT_NAME "AT42"
    PLUGIN_NAME "Audio Tools AT42"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${AT42_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument;Synth"
    VST3_AUTO_MANIFEST ${AT42_VST3_MANIFEST}
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${AT42_COPY}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "AT42 uses the audio input for the EXT IN mixer channel and the preamp.")
juce_generate_juce_header(AT42)
target_sources(AT42 PRIVATE ${AT42_SOURCES})
target_include_directories(AT42 PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/ui)
target_compile_definitions(AT42 PUBLIC
    AT42_HAS_ASSETS=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)

if(AT42_HEADLESS)
    target_compile_definitions(AT42 PUBLIC
        AT42_HEADLESS=1
        JUCE_USE_FREETYPE=0
        JUCE_USE_FONTCONFIG=0
        JUCE_USE_XRANDR=0
        JUCE_USE_XINERAMA=0
        JUCE_USE_XSHM=0
        JUCE_USE_XRENDER=0
        JUCE_USE_XCURSOR=0
        JUCE_USE_XINPUT=0
        JUCE_MODAL_LOOPS_PERMITTED=0)
    target_link_libraries(AT42
        PRIVATE AT42Assets juce::juce_audio_processors juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(UNIX AND NOT APPLE)
        target_compile_options(AT42 PUBLIC $<$<CONFIG:Release>:-O3 -ffp-contract=fast -fno-math-errno -ffunction-sections -fdata-sections>)
        target_link_options(AT42 INTERFACE
            -Wl,--gc-sections
            -Wl,--as-needed
            -Wl,--exclude-libs,ALL
            -static-libstdc++
            -static-libgcc)
    endif()
else()
    target_link_libraries(AT42
        PRIVATE AT42Assets juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
    dh_add_manual(AT42 ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)
endif()

if(NOT AT42_HEADLESS)
    # Render offline sin host (tools/render.cpp).
    juce_add_console_app(at42render VERSION ${AT42_VERSION} PRODUCT_NAME "at42render")
    juce_generate_juce_header(at42render)
    target_sources(at42render PRIVATE tools/render.cpp)
    target_include_directories(at42render PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
    target_compile_definitions(at42render PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
    target_link_libraries(at42render PRIVATE juce::juce_audio_formats
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

    # Pruebas del procesador completo sin host (tools/proctest.cpp): estado byte a byte, instancia nueva = programa 0,
    # hueco del host con una nota colgada. `at42proctest` → 0 si todo pasa.
    juce_add_console_app(at42proctest VERSION ${AT42_VERSION} PRODUCT_NAME "at42proctest")
    juce_generate_juce_header(at42proctest)
    target_sources(at42proctest PRIVATE tools/proctest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(at42proctest PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/ui ${CMAKE_SOURCE_DIR}/shared/manual)
    target_compile_definitions(at42proctest PRIVATE AT42_HAS_ASSETS=1 JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="AT42"
        JucePlugin_VersionString="${AT42_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
    target_link_libraries(at42proctest PRIVATE AT42Assets juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
