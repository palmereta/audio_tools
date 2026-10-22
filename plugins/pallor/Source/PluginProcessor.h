#pragma once
// Procesador de Pallor: los 16 grupos de patrones (A, B, Fill) y la cadena de canción viven acá y en el estado del
// plugin; la interfaz los edita en el hilo de mensajes y el hilo de audio copia sólo el patrón que cambió (sin
// bloquear: si la copia está ocupada, la toma en el bloque siguiente). Las muestras propias de cada instrumento se
// cargan en el hilo de mensajes, viajan dentro del estado (FLAC de 16 bits en base64, hasta un tope) y se publican al
// audio por un puntero atómico; la memoria vieja se libera unos bloques después.

#include <JuceHeader.h>
#include "ParamsPal.h"
#include "PatternIO.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/PresetTracker.h"
#include "../../../shared/tools/LatencyReporter.h"
#include "../../../shared/tools/MidiOutDelay.h"

#ifndef PALLOR_VERSION
 #define PALLOR_VERSION "1.0.0"
#endif
#ifndef PALLOR_PRODUCT
 #define PALLOR_PRODUCT "Pallor"
#endif

class PallorProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    PallorProcessor();
    ~PallorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // El wrapper VST3 agrega su Bypass y JUCE no llama a processBlock mientras dura: acá se sueltan las notas de la
    // salida MIDI y se marca el reloj para que al volver arranque como un salto.
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return PALLOR_PRODUCT; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    float value (const char* id) const;
    float value (const juce::String& id) const { if (auto* p = apvts.getRawParameterValue (id)) return p->load(); return 0.0f; }
    void setValue (const juce::String& id, float v);

    // ── Patrones (hilo de mensajes) ──
    dh::pal::Pattern getPattern (int i) const;
    void setPattern (int i, const dh::pal::Pattern& p);
    std::vector<dh::pal::ChainEntry> getChain() const;
    void setChain (const std::vector<dh::pal::ChainEntry>& c);
    void clearBank();
    std::function<void()> onPatternsChanged;

    // ── Biblioteca de patrones (juegos A/B/Fill por género) ──
    struct PatternSet { juce::String name, genre; float tempo = 120, swing = 50; int swingRes = 1; dh::pal::Pattern a, b, fill; };
    const std::vector<PatternSet>& getLibrary() const { return library; }
    void loadLibrarySet (int index, int group);

    // ── Presets ──
    struct PresetEntry { juce::String name, category, description; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index, bool sounds = true, bool patterns = true);
    // Presets propios: JSON en la carpeta del usuario (sonido, patrones, cadena y la ruta de las muestras propias).
    static juce::File userPresetDir();
    void rescanUserPresets();
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    std::function<void()> onPresetChanged;
    dh::SafeString currentPresetName { "Init" };
    dh::PresetTracker tracker { { "run", "sync", "group", "fill", "mode", "tempo" } };
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }

    // ── Muestras propias ──
    bool loadUserSample (int slot, const juce::File& file, juce::String& error);
    void clearUserSample (int slot);
    juce::String userSampleName (int slot) const;
    std::shared_ptr<dh::pal::SampleData> userSampleData (int slot) const { return slot >= 0 && slot < dh::pal::kNumSlots ? userSamples[(size_t) slot].data : nullptr; }
    static constexpr int kMaxSampleSeconds = 20;          // más largo se recorta
    static constexpr int kEmbedBudgetBytes = 8 << 20;     // FLAC total que entra en el estado; lo demás va sólo con la ruta

    // ── MIDI ──
    bool exportMidi (const juce::File& file, int patternIndex, int repeats);
    // Para arrastrar un patrón al DAW: lo escribe en la carpeta temporal y devuelve el archivo.
    juce::File writeDragMidi (int patternIndex, int repeats);
    static juce::MidiFile patternToMidi (const dh::pal::Bank& bank, int patternIndex, int repeats, float swing, int swingRes, int noteMap, uint32_t seed, double bpm);

    // ── Para la vista ──
    struct ViewState
    {
        std::atomic<int> pattern { -1 }, iter { 0 }, entry { -1 }, rep { 0 };
        std::atomic<bool> running { false }, fill { false };
        std::atomic<double> ppq { 0 }, segStart { 0 }, segEnd { 0 }, bpm { 120 };
        std::array<std::atomic<int>, dh::pal::kNumSlots> step {};
        std::array<std::atomic<uint32_t>, dh::pal::kNumSlots> trig {};
        std::array<std::atomic<float>, dh::pal::kNumSlots> peak {};      // la interfaz lo lee y lo pone en 0
        std::atomic<float> gr { 0 };
    } view;
    // Tocar una voz desde la interfaz (pads): se suma como una nota de entrada al bloque siguiente.
    void audition (int slot, float velocity);

    std::vector<std::pair<const char*, std::atomic<float>*>> rawIds;
    std::vector<juce::String> rawIdStore;
    std::atomic<bool> presetDirty { false }, patternsDirty { false };
    dh::pal::EngineParams buildParams() const;
    dh::pal::Engine& engineForTests() { return engine; }

private:
    dh::pal::Engine engine;
    double sr = 48000;
    dh::pal::Bank bank;                                   // copia del hilo de mensajes
    mutable juce::SpinLock bankLock;
    std::atomic<uint64_t> patDirty { ~0ull }; std::atomic<bool> chainDirty { true };
    std::vector<PresetEntry> presets; int currentProgram = 0;
    std::vector<PatternSet> library;
    double freeClock = 0; bool wasBypassed = false;
    // El MIDI de salida, atrasado la latencia que el host compensa en el audio (QA en Live 2026-10-22, P1: salía
    // 78 muestras antes que el audio con Tape encendido).
    dh::MidiOutDelay midiDelay;
    dh::LatencyReporter latency { *this };
    std::vector<dh::pal::MidiIn> midiIn; std::vector<dh::pal::MidiOut> midiOut;
    std::array<float*, 2 * dh::pal::kBuses> outPtr {};
    std::array<std::array<std::atomic<float>*, 26>, dh::pal::kNumSlots> voicePtr {};

    // Muestras propias: el objeto publicado (atómico) y los que esperan para liberarse.
    struct UserSample { std::shared_ptr<dh::pal::SampleData> data; juce::String path; };
    std::array<UserSample, dh::pal::kNumSlots> userSamples;
    std::array<std::atomic<const dh::pal::SampleData*>, dh::pal::kNumSlots> userPtr {};
    std::vector<std::pair<std::shared_ptr<dh::pal::SampleData>, uint64_t>> retired;
    std::atomic<uint64_t> blockCount { 0 };
    std::array<std::atomic<int>, dh::pal::kNumSlots> auditionVel {};
    void publishSample (int slot, std::shared_ptr<dh::pal::SampleData> s, const juce::String& path);
    void releaseRetired();

    void handleAsyncUpdate() override;
    void loadFactory();
    juce::ValueTree bankToTree() const;
    void bankFromTree (const juce::ValueTree& t);
    juce::ValueTree samplesToTree() const;
    void samplesFromTree (const juce::ValueTree& t);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PallorProcessor)
};
