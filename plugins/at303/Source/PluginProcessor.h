#pragma once
// Procesador del AT303: el banco de 64 patrones (4 grupos × A/B × 8) vive acá y en el estado del plugin; la
// interfaz los edita en el hilo de mensajes y el hilo de audio toma la copia nueva sin bloquear. El reloj sale del
// host (24 pulsos por negra desde su posición) o de un contador propio con el Tempo.

#include <JuceHeader.h>
#include "Params303.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/PresetTracker.h"
#include "../../../shared/tools/LatencyReporter.h"

class At303Processor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    At303Processor();
    ~At303Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // En bypass el motor sigue recibiendo el MIDI (si no, un note-off que llega en bypass deja la nota colgada).
    void processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override { processBlock (b, m); b.clear(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "AT303"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    // Lectura sin reservar memoria (se llama ~50 veces por bloque en el hilo de audio): ids literales contra una
    // tabla armada en el constructor, en vez de construir un juce::String por llamada.
    float value (const char* id) const
    {
        for (auto& r : rawIds) if (std::strcmp (r.first, id) == 0) return r.second->load();
        return 0.0f;
    }
    float value (const juce::String& id) const { if (auto* p = apvts.getRawParameterValue (id)) return p->load(); return 0.0f; }
    void setValue (const juce::String& id, float v);

    // ── Patrones (hilo de mensajes) ──
    dh::tb::Pattern getPattern (int i) const { const juce::SpinLock::ScopedLockType l (patLock); return bank[(size_t) juce::jlimit (0, 63, i)]; }
    void setPattern (int i, const dh::tb::Pattern& p);
    int patternIndex() const { return juce::jlimit (0, 63, (int) value ("pattern")); }
    void generate (bool mutate);                          // generador ácido sobre el patrón actual
    void clearPattern();
    uint32_t genSeed = 1;
    std::function<void()> onPatternsChanged;

    // ── Presets ──
    struct PresetEntry { juce::String name, category, description; juce::var data; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    std::function<void()> onPresetChanged;
    dh::SafeString currentPresetName { "Init" };
    dh::PresetTracker tracker { { "run", "sync", "mode", "pattern", "quality", "tempo" } };   // transporte y sesión no "modifican"
    dh::LatencyReporter latency { *this };
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }
    std::vector<std::pair<const char*, std::atomic<float>*>> rawIds;
    std::vector<juce::String> rawIdStore;
    std::atomic<bool> presetDirty { false }, patternsDirty { false };
    void handleAsyncUpdate() override { if (presetDirty.exchange (false) && onPresetChanged) onPresetChanged(); if (patternsDirty.exchange (false) && onPatternsChanged) onPatternsChanged(); }

    // ── Para la vista ──
    struct ViewState { std::atomic<int> step { -1 }; std::atomic<float> c13 { 0 }, meg { 0 }, veg { 0 }, cutoffHz { 500 }, cv { 2 }, k { 0 }, level { 0 }; std::atomic<bool> running { false }, gate { false }; std::array<std::atomic<float>, 4> ladder {}; std::atomic<float> vca { 0 }; } view;
    juce::MidiKeyboardState keyboardState;                // teclas del mouse y del MIDI

private:
    dh::tb::Engine engine;
    double sr = 48000;
    std::array<dh::tb::Pattern, 64> bank;
    juce::SpinLock patLock; std::atomic<bool> patDirty { true }; int lastPatIdx = -1;
    std::vector<PresetEntry> presets; int currentProgram = 0;
    double freeClock = 0;
    void loadFactory();
    dh::tb::EngineParams buildParams() const;
    juce::ValueTree patternsToTree() const;
    void patternsFromTree (const juce::ValueTree& t);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (At303Processor)
};
