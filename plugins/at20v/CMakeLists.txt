# AT20V — semimodular basado en el Korg MS-20 (1978).
set(AT20V_VERSION 1.0.5)

if(APPLE)
    set(AT20V_FORMATS VST3 AU Standalone)
    set(AT20V_COPY TRUE)    # instala en ~/Library/Audio/Plug-Ins al compilar
else()
    set(AT20V_FORMATS VST3 Standalone)
    set(AT20V_COPY FALSE)   # en Windows la carpeta de VST3 pide administrador: se instala aparte
endif()

# Imágenes renderizadas en Blender y presets, embebidos en el binario.
file(GLOB AT20V_ASSETS CONFIGURE_DEPENDS assets/ui/*.png assets/ui/*.json assets/presets/*.json)
juce_add_binary_data(AT20VAssets SOURCES ${AT20V_ASSETS})
file(GLOB_RECURSE AT20V_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

# Dos variantes del mismo código:
#   AT20V     instrumento (MIDI in/out; la entrada de audio para vocoder/ESP va por sidechain)
#   AT20V FX  efecto de audio con entrada MIDI: la pista de audio es el modulador del vocoder
#             (en Ableton: ponerlo en la pista de voz y rutear una pista MIDI hacia él)
function(at20v_plugin target name code is_synth categories)
    juce_add_plugin(${target}
        VERSION ${AT20V_VERSION}
        COMPANY_NAME "Audio Tools"
        BUNDLE_ID "com.audiotools.${target}"
        PLUGIN_MANUFACTURER_CODE Atls
        PLUGIN_CODE ${code}
        PRODUCT_NAME "${name}"
        PLUGIN_NAME "Audio Tools ${name}"                 # el nombre que ve el host: en Ableton se reconocen por el prefijo
        FORMATS ${AT20V_FORMATS}
        IS_SYNTH ${is_synth}
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES ${categories}
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD ${AT20V_COPY}
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "AT20V uses the audio input for the vocoder and the External Signal Processor.")
    juce_generate_juce_header(${target})
    target_sources(${target} PRIVATE ${AT20V_SOURCES})
    target_include_directories(${target} PRIVATE Source ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/ui)
    target_compile_definitions(${target} PUBLIC
        AT20V_HAS_ASSETS=1
        AT20V_IS_FX=$<NOT:$<BOOL:${is_synth}>>
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        _USE_MATH_DEFINES=1)
    target_link_libraries(${target}
        PRIVATE AT20VAssets juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
    dh_add_manual(${target} ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)
endfunction()

at20v_plugin(AT20V "AT20V" At2v TRUE "Instrument;Synth")
at20v_plugin(AT20VFX "AT20V FX" At2f FALSE "Fx;Synth")

# Render offline sin host (tools/render.cpp).
juce_add_console_app(at20vrender VERSION ${AT20V_VERSION} PRODUCT_NAME "at20vrender")
juce_generate_juce_header(at20vrender)
target_sources(at20vrender PRIVATE tools/render.cpp)
target_include_directories(at20vrender PRIVATE Source ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(at20vrender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(at20vrender PRIVATE juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
