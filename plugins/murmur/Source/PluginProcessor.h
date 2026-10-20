#pragma once

#include <JuceHeader.h>
#include "ParamsMm.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/PresetTracker.h"

class MurmurProcessor : public juce::AudioProcessor
{
public:
    MurmurProcessor();
    ~MurmurProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Murmur"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }     // feedback, reverb y freeze

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

    const dh::mm::Engine& engine() const { return eng; }
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, cpuLoad { 0.0f };
    std::atomic<int> midiFlash { 0 };

    // Presets
    struct PresetEntry { juce::String name, category; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    dh::SafeString currentPresetName { "Init" };
    dh::PresetTracker tracker;
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }
    static juce::File userPresetDir();
    std::function<void()> onPresetChanged;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    dh::mm::Engine eng;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> vals;
    std::vector<float> tmpL, tmpR;
    std::vector<int16_t> memData; int memCount = 0; double memRate = 48000.0; std::atomic<bool> memPending { false };
    juce::AudioBuffer<float> demoIn;      // MURMUR_DEMO_INPUT=archivo.wav: entrada de prueba en loop (capturas, demos)
    int demoPos = 0;
    double sr = 48000.0;
    bool wasBypassed = false;
    std::vector<PresetEntry> presets;
    int currentProgram = 0;
    void loadFactory();
    void notifyUi (std::function<void()>& cb);
    void handleMidi (const juce::MidiMessage& m);

    JUCE_DECLARE_WEAK_REFERENCEABLE (MurmurProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MurmurProcessor)
};
