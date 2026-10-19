#pragma once
// Procesador de Tarnish: traduce los parámetros al motor (sólo cuando cambian: el motor recalibra la histéresis y
// el ruido), dispara el tape stop por el botón, por notas MIDI o con el transporte del host, toma la posición del
// host para que lo aleatorio quede atado a la línea de tiempo, y deja a la vista lo que la interfaz dibuja.

#include <JuceHeader.h>
#include "ParamsTn.h"

class TarnishProcessor : public juce::AudioProcessor
{
public:
    TarnishProcessor();
    ~TarnishProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }
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

    // ── Presets ──
    struct PresetEntry { juce::String name, category, description; juce::var data; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    std::function<void()> onPresetChanged;
    juce::String currentPresetName { "Init" };

    // ── Para la vista (escritos por el hilo de audio) ──
    struct ViewState
    {
        std::atomic<double> pos { 0 };                 // posición de la salida (lo que se oye), en muestras
        std::atomic<float> vu { 0 }, speed { 1 }, wf { 0 }, spacing { 0 }, inPeak { 0 }, outPeak { 0 };
        std::atomic<float> reelS { 0.1f }, reelT { 0.05f }, turnsS { 0 }, turnsT { 0 }, groove { 0 }, rotation { 0 };
        std::atomic<int> machine { 0 };
        std::atomic<bool> stopped { false };
        static constexpr int kScope = 512;
        std::array<std::atomic<float>, kScope> scopeH {}, scopeM {};
        std::atomic<int> scopeWrite { 0 };
        juce::AbstractFifo eventFifo { 128 };
        std::array<dh::tn::UiEvent, 128> events;
    } view;
    int latencySamples() const { return engine.latency(); }
    double sampleRate() const { return sr; }
    const dh::tn::Profile& profileNow() const { return engine.currentProfile(); }
    double scratchAngle (int k) const { return engine.scratchAngle (k); }
    dh::tn::EngineParams currentEngineParams() const { return buildParams (bpmNow.load()); }

private:
    dh::tn::Engine engine;
    double sr = 48000.0;
    int currentProgram = 0;
    std::vector<PresetEntry> presets;
    std::vector<float> lastValues;
    int64_t counter = 0;
    int heldNotes = 0;
    bool midiStop = false, transportStop = false, lastStop = false;
    std::atomic<double> bpmNow { 120.0 };
    juce::AudioBuffer<float> demo; int demoPos = 0;        // TARNISH_DEMO_INPUT: entrada de prueba en loop (capturas)

    void loadFactory();
    dh::tn::EngineParams buildParams (double bpm) const;
    bool paramsChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TarnishProcessor)
};
