#pragma once

#include <JuceHeader.h>
#include "ParamsMd.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/LatencyReporter.h"
#include "../../../shared/tools/PresetTracker.h"

class AtmdProcessor : public juce::AudioProcessor
{
public:
    AtmdProcessor();
    ~AtmdProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ATMD"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    float value (const juce::String& id) const { if (auto* p = apvts.getRawParameterValue (id)) return p->load(); return 0.0f; }
    void setValue (const juce::String& id, float v);

    // Ruedas: las mueven el MIDI (pitch bend, CC 1) o el mouse; la interfaz las dibuja.
    std::atomic<float> pitchWheel { 0.0f }, modWheel { 0.0f };
    juce::MidiKeyboardState keyboardState;    // teclas que toca el mouse y las que llegan por MIDI (para dibujarlas)
    std::atomic<bool> overload { false };
    std::atomic<float> outLevel { 0.0f }, cpuLoad { 0.0f };
    std::atomic<int> voicesActive { 0 };

    // Presets
    struct PresetEntry { juce::String name, category; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    dh::SafeString currentPresetName { "Init" };
    dh::PresetTracker tracker;
    dh::LatencyReporter latency { *this };
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }
    static juce::File userPresetDir();
    std::function<void()> onPresetChanged;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();

private:
    dh::md::Engine eng;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> vals;
    std::vector<float> extBuf, tmpR;
    double sr = 48000.0;
    int maxBlock = 512, osNow = 0, overloadHold = 0;
    bool wasBypassed = false;
    std::vector<PresetEntry> presets;
    int currentProgram = 0;
    void loadFactory();
    void notifyUi (std::function<void()>& cb);
    void handleMidi (const juce::MidiMessage& m);

    JUCE_DECLARE_WEAK_REFERENCEABLE (AtmdProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtmdProcessor)
};
