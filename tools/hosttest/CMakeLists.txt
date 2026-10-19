# athosttest: prueba los plugins compilados como un DAW (ver hosttest.cpp).
juce_add_console_app(athosttest VERSION 1.0.0 PRODUCT_NAME "athosttest")
juce_generate_juce_header(athosttest)
target_sources(athosttest PRIVATE hosttest.cpp)
target_compile_definitions(athosttest PRIVATE
    JUCE_PLUGINHOST_VST3=1 $<$<PLATFORM_ID:Darwin>:JUCE_PLUGINHOST_AU=1>
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(athosttest PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags)
