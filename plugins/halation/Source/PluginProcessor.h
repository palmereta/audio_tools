#pragma once
// Procesador del efecto de espacio: traduce los parámetros al motor (tiempo sincronizado, Length → realimentación y
// caída, congelado total o por bandas, notas MIDI del resonador, morph A/B, envolvente), y deja a la vista lo que
// necesita para dibujar la sala (líneas, ataques, estados) sin bloqueos.

#include <JuceHeader.h>
#include "ParamsSp.h"

class SpaceProcessor : public juce::AudioProcessor
{
public:
    SpaceProcessor();
    ~SpaceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;
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

    // ── Escena B (morph): instantánea de los parámetros continuos ──
    void storeSceneB();
    bool hasSceneB() const { return sceneBValid.load(); }

    // ── Presets ──
    struct PresetEntry { juce::String name, category, description; juce::var data; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    std::function<void()> onPresetChanged;
    juce::String currentPresetName { "Init" };

    // ── Para la vista (escritos por el hilo de audio) ──
    struct ViewState
    {
        std::atomic<float> lineMs[8], lineEnergy[8], freeze { 0 }, limiterDb { 0 }, loopGain { 0 }, sizeNow { 1 }, wetPeak { 0 }, inLevel { 0 };
        std::atomic<float> space { 0 }, length { 0 }, decayS { 2 }, feedback { 0.4f }, bpm { 120 };
        std::atomic<int> shimmerLayers { 0 }, grains { 0 }, activeLines { 2 };
        std::atomic<bool> stutter { false };
        juce::AbstractFifo onsetFifo { 64 };
        std::array<dh::sp::Onset, 64> onsets;
    } view;

    dh::sp::EngineParams currentEngineParams() const;

private:
    dh::sp::Engine engine;
    double sr = 48000.0;
    int currentProgram = 0;
    std::vector<PresetEntry> presets;
    std::array<int, 8> heldNotes {}; int nHeld = 0;
    std::map<juce::String, float> sceneB; std::atomic<bool> sceneBValid { false };
    juce::CriticalSection sceneLock;
    float envFollow = 0.0f;
    double lastPpq = -1.0;
    juce::AudioBuffer<float> demo; int demoPos = 0;        // HALATION_DEMO_INPUT: entrada de prueba en loop (capturas)

    void loadFactory();
    dh::sp::EngineParams buildParams (double bpm) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceProcessor)
};
