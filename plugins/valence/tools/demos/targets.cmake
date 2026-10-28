# Included at the end of the root CMakeLists by extra.cmake (see there).
set(_at303 ${CMAKE_SOURCE_DIR}/plugins/at303)
juce_add_console_app(at303midirender VERSION 1.0.0 PRODUCT_NAME "at303midirender")
juce_generate_juce_header(at303midirender)
target_sources(at303midirender PRIVATE ${VALENCE_DEMOS_DIR}/at303midi.cpp ${_at303}/Source/PluginProcessor.cpp ${_at303}/Source/PluginEditor.cpp)
target_include_directories(at303midirender PRIVATE ${_at303}/Source ${_at303}/Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp
    ${CMAKE_SOURCE_DIR}/shared/manual ${CMAKE_SOURCE_DIR}/shared/tools)
target_compile_definitions(at303midirender PRIVATE JucePlugin_Build_Standalone=0 DH_HAS_MANUAL=0 JucePlugin_Name="AT303"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(at303midirender PRIVATE At303Assets juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

set(_atmd ${CMAKE_SOURCE_DIR}/plugins/atmd)
juce_add_console_app(atmdmidirender VERSION 1.0.0 PRODUCT_NAME "atmdmidirender")
juce_generate_juce_header(atmdmidirender)
target_sources(atmdmidirender PRIVATE ${VALENCE_DEMOS_DIR}/atmdmidi.cpp)
target_include_directories(atmdmidirender PRIVATE ${_atmd}/Source ${_atmd}/Source/dsp ${CMAKE_SOURCE_DIR}/shared/dsp ${CMAKE_SOURCE_DIR}/shared/tools)
target_compile_definitions(atmdmidirender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 _USE_MATH_DEFINES=1)
target_link_libraries(atmdmidirender PRIVATE juce::juce_audio_formats juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
