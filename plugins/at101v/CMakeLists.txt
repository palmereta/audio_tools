# AT101V — monofónico basado en el Roland SH-101 (1982), con arpegiador y secuenciador.
# Un solo plugin (instrumento): el 101 no tiene entrada de audio.
set(AT101V_VERSION 1.0.7)

if(APPLE)
    set(AT101V_FORMATS VST3 AU Standalone)
    set(AT101V_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(AT101V_FORMATS VST3 LV2 Standalone)
    set(AT101V_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(AT101V_FORMATS VST3 Standalone)
    set(AT101V_COPY FALSE)
endif()

# Imágenes renderizadas en Blender y presets, embebidos en el binario.
file(GLOB AT101V_ASSETS CONFIGURE_DEPENDS assets/ui/*.png assets/ui/*.jpg assets/ui/*.json assets/presets/*.json)
juce_add_binary_data(AT101VAssets SOURCES ${AT101V_ASSETS})
file(GLOB_RECURSE AT101V_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

function(at101v_plugin target name code is_synth categories)
    juce_add_plugin(${target}
        VERSION ${AT101V_VERSION}
        COMPANY_NAME "Audio Tools"
        BUNDLE_ID "com.audiotools.${target}"
        PLUGIN_MANUFACTURER_CODE Atls
        PLUGIN_CODE ${code}
        PRODUCT_NAME "${name}"
        PLUGIN_NAME "Audio Tools ${name}"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
        FORMATS ${AT101V_FORMATS}
        IS_SYNTH ${is_synth}
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES ${categories}
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD ${AT101V_COPY})
    juce_generate_juce_header(${target})
    target_sources(${target} PRIVATE ${AT101V_SOURCES})
    target_include_directories(${target} PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
    target_compile_definitions(${target} PUBLIC
        AT101V_HAS_ASSETS=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)
    target_link_libraries(${target}
        PRIVATE AT101VAssets juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
    dh_add_manual(${target} ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)
endfunction()

at101v_plugin(AT101V "AT101V" At1v TRUE "Instrument;Synth")

# Render offline sin host (tools/render.cpp).
juce_add_console_app(at101vrender VERSION ${AT101V_VERSION} PRODUCT_NAME "at101vrender")
juce_generate_juce_header(at101vrender)
target_sources(at101vrender PRIVATE tools/render.cpp)
target_include_directories(at101vrender PRIVATE Source Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(at101vrender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(at101vrender PRIVATE juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Pruebas con el plugin cargado como en un DAW (VST3 o AU compilados): tools/hostcheck.cpp.
juce_add_console_app(at101vhostcheck VERSION ${AT101V_VERSION} PRODUCT_NAME "at101vhostcheck")
juce_generate_juce_header(at101vhostcheck)
target_sources(at101vhostcheck PRIVATE tools/hostcheck.cpp)
target_compile_definitions(at101vhostcheck PRIVATE
    JUCE_PLUGINHOST_VST3=1 $<$<PLATFORM_ID:Darwin>:JUCE_PLUGINHOST_AU=1>
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(at101vhostcheck PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags)
