# Valence — arpegiador MIDI de cuatro capas que entran y salen de fase sobre el acorde que se toca.
set(VALENCE_VERSION 1.0.0)

if(APPLE)
    set(VALENCE_FORMATS VST3 AU Standalone)
    set(VALENCE_COPY TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(VALENCE_FORMATS VST3 LV2 Standalone)
    set(VALENCE_COPY FALSE)   # Linux: se instala aparte (~/.vst3, ~/.lv2)
else()
    set(VALENCE_FORMATS VST3 Standalone)
    set(VALENCE_COPY FALSE)
endif()

file(GLOB VALENCE_ASSETS CONFIGURE_DEPENDS assets/presets/*.json)
juce_add_binary_data(ValenceAssets SOURCES ${VALENCE_ASSETS})
file(GLOB_RECURSE VALENCE_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

set(VALENCE_DEFS
    VALENCE_HAS_ASSETS=1
    # JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS queda prendido (como en AT303, AT330 y DeviceNTI): un VST3 sólo recibe CC
    # por esos parámetros, y Valence lee el pedal de sustain (latch), All Notes Off / All Sound Off (CC 123/120, los que
    # manda el host al parar) y reenvía los demás CC. Con 0, athosttest midi dejaba el arpegio sonando después del CC 123.
    # El precio son los ~2.080 "MIDI CC n|m" en la lista de REAPER (QA en DAWs, X4): no automatizables y después de los 64.
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES=1)

juce_add_plugin(Valence
    VERSION ${VALENCE_VERSION}
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.Valence"
    PLUGIN_MANUFACTURER_CODE Atls
    PLUGIN_CODE Vlnc
    PRODUCT_NAME "Valence"
    PLUGIN_NAME "Audio Tools Valence"                # el nombre que ve el host: en Ableton se reconocen por el prefijo
    FORMATS ${VALENCE_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument;Generator"
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${VALENCE_COPY}
    MICROPHONE_PERMISSION_ENABLED FALSE)
juce_generate_juce_header(Valence)
target_sources(Valence PRIVATE ${VALENCE_SOURCES})
target_include_directories(Valence PRIVATE Source Source/gen ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(Valence PUBLIC ${VALENCE_DEFS})
target_link_libraries(Valence
    PRIVATE ValenceAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Logic pone los arpegiadores en la ranura de MIDI FX: un AU aparte de tipo MIDI processor ('aumi'), con el mismo
# código y sin buses de audio (el tiempo sale del playhead, no del audio).
if(APPLE)
    juce_add_plugin(ValenceMidiFx
        VERSION ${VALENCE_VERSION}
        COMPANY_NAME "Audio Tools"
        BUNDLE_ID "com.audiotools.ValenceMidiFx"
        PLUGIN_MANUFACTURER_CODE Atls
        PLUGIN_CODE Vlnm
        PRODUCT_NAME "Valence MIDI FX"
        PLUGIN_NAME "Audio Tools Valence MIDI FX"
        FORMATS AU
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        MICROPHONE_PERMISSION_ENABLED FALSE)
    juce_generate_juce_header(ValenceMidiFx)
    target_sources(ValenceMidiFx PRIVATE ${VALENCE_SOURCES})
    target_include_directories(ValenceMidiFx PRIVATE Source Source/gen ${CMAKE_SOURCE_DIR}/shared/dsp)
    target_compile_definitions(ValenceMidiFx PUBLIC ${VALENCE_DEFS})
    target_link_libraries(ValenceMidiFx
        PRIVATE ValenceAssets juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endif()

# Render offline: el MIDI de cada capa, el audio del sonido de prueba y el JSON de cada nota.
juce_add_console_app(valencerender VERSION ${VALENCE_VERSION} PRODUCT_NAME "valencerender")
juce_generate_juce_header(valencerender)
target_sources(valencerender PRIVATE tools/render.cpp)
target_include_directories(valencerender PRIVATE Source Source/gen ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(valencerender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(valencerender PRIVATE juce::juce_audio_formats juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Pruebas del motor sin JUCE (también se compilan a mano: ver el encabezado de tools/valencetest.cpp).
add_executable(valencetest tools/valencetest.cpp)
target_include_directories(valencetest PRIVATE Source/gen)

# La misma variación tiene que dar la misma música en todos los sistemas: el motor usa su propia matemática
# (Theory.h, dm::) y se compila sin contracción a FMA ni /fp:fast (que reordena operaciones).
if(MSVC)
    set(VALENCE_FP /fp:precise)
else()
    set(VALENCE_FP -ffp-contract=off)
endif()
foreach(t Valence valencerender valencetest)
    target_compile_options(${t} PRIVATE ${VALENCE_FP})
endforeach()
if(APPLE)
    target_compile_options(ValenceMidiFx PRIVATE ${VALENCE_FP})
endif()

# Grabador de la interfaz del átomo, sin audio ni DAW: corre el editor con VALENCE_RECORD=dir VALENCE_DEMO=carpeta
# (un JPEG por 1/30 s de sesión de una demo) y sale solo. Para pc50-wsl, bajo Xvfb.
juce_add_gui_app(valencerecord VERSION ${VALENCE_VERSION} PRODUCT_NAME "valencerecord")
juce_generate_juce_header(valencerecord)
target_sources(valencerecord PRIVATE tools/record_main.cpp ${VALENCE_SOURCES})
target_include_directories(valencerecord PRIVATE Source Source/gen ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(valencerecord PRIVATE ${VALENCE_DEFS} JucePlugin_IsMidiEffect=0)
target_link_libraries(valencerecord PRIVATE ValenceAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
target_compile_options(valencerecord PRIVATE ${VALENCE_FP})

# Banco de pruebas del costo de dibujo del átomo, sin ventana (ver el encabezado de tools/atombench.cpp). Usa
# getrusage: sólo macOS y Linux.
if(NOT WIN32)
juce_add_console_app(valenceatombench VERSION ${VALENCE_VERSION} PRODUCT_NAME "valenceatombench")
juce_generate_juce_header(valenceatombench)
target_sources(valenceatombench PRIVATE tools/atombench.cpp ${VALENCE_SOURCES})
target_include_directories(valenceatombench PRIVATE Source Source/gen ${CMAKE_SOURCE_DIR}/shared/dsp)
target_compile_definitions(valenceatombench PRIVATE ${VALENCE_DEFS} JucePlugin_IsMidiEffect=0)
target_link_libraries(valenceatombench PRIVATE ValenceAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
target_compile_options(valenceatombench PRIVATE ${VALENCE_FP})
endif()
