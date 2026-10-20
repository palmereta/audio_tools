# Audio Tools Launcher: installs and updates the plugins from web/downloads/manifest.json.
# scripts/build-web.py reads LAUNCHER_VERSION from here for the manifest and the website.
set(LAUNCHER_VERSION 1.0.2)

juce_add_gui_app(AudioToolsLauncher
    VERSION ${LAUNCHER_VERSION}
    PRODUCT_NAME "Audio Tools Launcher"
    COMPANY_NAME "Audio Tools"
    BUNDLE_ID "com.audiotools.Launcher")
target_sources(AudioToolsLauncher PRIVATE
    Source/Main.cpp Source/MainComponent.cpp Source/SettingsPanel.cpp Source/Media.cpp Source/Core.cpp Source/Platform.cpp)
target_compile_definitions(AudioToolsLauncher PRIVATE
    LAUNCHER_VERSION="${LAUNCHER_VERSION}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="Audio Tools Launcher"
    JUCE_APPLICATION_VERSION_STRING="${LAUNCHER_VERSION}")
target_link_libraries(AudioToolsLauncher
    PRIVATE juce::juce_gui_basics juce::juce_cryptography juce::juce_audio_devices juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Headless test of the core (manifest, versions, checksum, install, record, remove):
#   build-launcher/tools/launcher/atlaunchertest_artefacts/Release/atlaunchertest
juce_add_console_app(atlaunchertest VERSION ${LAUNCHER_VERSION} PRODUCT_NAME "atlaunchertest")
target_sources(atlaunchertest PRIVATE test/LauncherTest.cpp Source/Core.cpp)
target_include_directories(atlaunchertest PRIVATE Source)
target_compile_definitions(atlaunchertest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(atlaunchertest PRIVATE juce::juce_core juce::juce_cryptography
    PUBLIC juce::juce_recommended_config_flags)
