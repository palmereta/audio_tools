#pragma once
// Procesador del AT330: el motor (Source/dsp/Engine330.h) con sus parámetros, los presets de fábrica y el MIDI
// (notas, pedal de sostén, rueda de modulación, pitch bend). La entrada de audio es opcional y es la voz del vocoder:
// en un DAW entra por sidechain (el mismo bus del AT20V). Con el portador externo, el canal izquierdo es la voz y el
// derecho el portador.

#include <JuceHeader.h>
#include "Params330.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/PresetTracker.h"
#include "../../../shared/tools/HostGap.h"

#ifndef AT330_VERSION
 #define AT330_VERSION "1.0.0"
#endif

class At330Processor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    At330Processor();
    ~At330Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    // reset() del host (AudioUnitReset antes de un render, VST3 setProcessing(false)): lo mismo que preparar de nuevo,
    // con el lock del proceso tomado (QA en REAPER 2026-10-22: REAPER sólo resetea el AU antes del segundo render).
    void reset() override
    {
        const juce::ScopedLock sl (getCallbackLock());
        if (getSampleRate() > 0.0 && getBlockSize() > 0) prepareToPlay (getSampleRate(), getBlockSize());
    }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // El wrapper VST3 agrega su Bypass y JUCE no llama a processBlock mientras dura: se sueltan las notas (un note-off
    // que llega en bypass no se pierde). Un host que en bypass no llama a nada (REAPER) lo detecta hostGap.
    void processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "AT330"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    // Lectura sin reservar memoria (se llama ~45 veces por bloque en el hilo de audio).
    float value (const char* id) const
    {
        for (auto& r : rawIds) if (std::strcmp (r.first, id) == 0) return r.second->load();
        return 0.0f;
    }
    float value (const juce::String& id) const { if (auto* p = apvts.getRawParameterValue (id)) return p->load(); return 0.0f; }
    void setValue (const juce::String& id, float v);

    // ── Presets ──
    struct PresetEntry { juce::String name, category, description; juce::var data; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    std::function<void()> onPresetChanged;
    dh::SafeString currentPresetName { "Init" };
    dh::PresetTracker tracker { { "split" } };
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }

    // ── Para la vista ──
    struct ViewState
    {
        std::atomic<float> mic { 0 }, out { 0 };
        std::array<std::atomic<float>, dh::vp::kBands> bands {};
        std::atomic<int> keys { 0 };
    } view;
    juce::MidiKeyboardState keyboardState;                // teclas del mouse y del MIDI

    dh::vp::Params buildParams() const;
    dh::vp::Engine& engineForTests() { return engine; }
    std::vector<std::pair<const char*, std::atomic<float>*>> rawIds;
    std::vector<juce::String> rawIdStore;

private:
    dh::vp::Engine engine;
    double sr = 48000;
    int maxBlock = 512;
    std::vector<float> micBuf, carBuf;
    std::vector<PresetEntry> presets; int currentProgram = 0;
    std::atomic<bool> presetDirty { false };
    dh::HostGap hostGap;
    void handleAsyncUpdate() override { if (presetDirty.exchange (false) && onPresetChanged) onPresetChanged(); }
    void loadFactory();
    void handleMidi (const juce::MidiMessage& m);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (At330Processor)
};
