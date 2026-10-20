#pragma once
// Procesador de Plumb: la voz entra en mono (la suma de los canales), pasa por el motor y sale igual por todos los
// canales. El modo de latencia se puede cambiar en marcha: el motor cambia su adelanto sin reservar memoria y la
// latencia nueva se informa al host desde el hilo de mensajes. La vista lee las decisiones del corrector de una
// cola sin bloqueo.

#include <JuceHeader.h>
#include "ParamsPl.h"
#include "SafeString.h"
#include "PresetTracker.h"

class PlumbProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    PlumbProcessor();
    ~PlumbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Plumb"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }
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
    dh::SafeString currentPresetName { "Init" };
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }

    // ── Para la vista ──
    const dh::pl::Engine& getEngine() const { return engine; }
    double getSampleRateNow() const { return sr; }
    std::atomic<float> inLevel { 0 }, outLevel { 0 };
    std::atomic<bool> compare { false };          // escuchar la entrada sin corregir, con la misma latencia
    // Tonalidad automática: "Listen" junta notas y propone; nunca cambia la tonalidad sola.
    void startListening() { listenRequested.store (true); listening.store (true); listenStart.store (samplesDone.load()); }
    bool isListening() const { return listening.load(); }
    double listenSeconds() const { return (double) (samplesDone.load() - listenStart.load()) / sr; }
    void keySuggestion (int& tonic, bool& minor, float& conf) const { tonic = sugTonic.load(); minor = sugMinor.load(); conf = sugConf.load(); }
    void stopListening() { listening.store (false); }

    // El conjunto de notas objetivo efectivo (para la vista y la página de escala).
    uint16_t allowedMask() const;
    uint16_t bypassMask() const;

private:
    dh::pl::Engine engine;
    double sr = 48000;
    std::vector<float> mono;
    juce::AudioBuffer<float> demo; int demoPos = 0;        // PLUMB_DEMO_INPUT: entrada de prueba en loop (capturas)
    std::array<std::atomic<float>*, 12> degPtr {};
    int lastMode = -1;
    std::vector<PresetEntry> presets; int currentProgram = 0;
    std::atomic<bool> listenRequested { false }, listening { false };
    std::atomic<int64_t> samplesDone { 0 }, listenStart { 0 };
    std::atomic<int> sugTonic { 0 }; std::atomic<bool> sugMinor { false }; std::atomic<float> sugConf { 0 };
    void loadFactory();
    dh::pl::EngineParams buildParams() const;
    void handleAsyncUpdate() override;
    dh::PresetTracker tracker { { "key", "scale", "mode", "ref_a", "gate", "midi_mode", "voice", "mix", "output",
                                  "deg0", "deg1", "deg2", "deg3", "deg4", "deg5", "deg6", "deg7", "deg8", "deg9", "deg10", "deg11" } };   // la canción y la sesión no "modifican" el preset
    std::atomic<bool> presetDirty { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlumbProcessor)
};
