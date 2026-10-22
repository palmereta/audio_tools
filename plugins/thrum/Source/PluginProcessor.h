#pragma once
// Procesador de Thrum: los parámetros, los presets, la biblioteca de patrones (rasgueo y arpegio), el patrón editado,
// el orden de la cadena y la IR del usuario viven acá y en el estado del plugin. La IR del modelo de gabinete se diseña
// en un hilo propio cuando se mueve un control del gabinete (una FFT de 8192 puntos no va en el hilo de audio) y se
// publica con una bandera atómica; el motor la toma al empezar el bloque y hace un fundido de 20 ms. Renderizando
// fuera de tiempo real (bounce, thrumrender) la IR se diseña en el mismo bloque: el resultado es determinista.
//
// Salidas: la principal (procesada, estéreo) y "DI" (la guitarra directa, alineada con la procesada, para reamplificar).
// Salida MIDI: lo que la guitarra tocó (cada cuerda, rasgueos y digitación expandidos), opcionalmente un canal por
// cuerda; además queda grabado para exportarlo a .mid.

#include <JuceHeader.h>
#include <bitset>
#include "ParamsTh.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/PresetTracker.h"
#include "../../../shared/tools/LatencyReporter.h"

#ifndef THRUM_PRODUCT
 #define THRUM_PRODUCT "Thrum"
#endif

class ThrumProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    ThrumProcessor();
    ~ThrumProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // El wrapper VST3 agrega su Bypass y JUCE no llama a processBlock mientras dura: acá se apagan las cuerdas y se
    // mandan los note-off de lo que estaba sonando.
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return THRUM_PRODUCT; }
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
    dh::gt::EngineParams buildParams() const;
    dh::gt::Engine& engineForTests() { return engine; }

    // ── Presets ──
    struct PresetEntry { juce::String name, category, description; juce::var data; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    std::function<void()> onPresetChanged;
    dh::SafeString currentPresetName { "Init" };
    dh::PresetTracker tracker { { "s_tempo", "m_in", "m_out", "m_out_str", "m_out_ch", "m_ks", "out_di_main", "out_di", "amp_quality" } };
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }

    // ── Patrones ──
    struct PatternEntry { juce::String name, genre, steps, accents; int div = 0; float swing = 50; };
    static const std::vector<PatternEntry>& strumLibrary();
    static const std::vector<PatternEntry>& arpLibrary();
    PatternEntry getStrum() const { const juce::SpinLock::ScopedLockType l (patLock); return strum; }
    PatternEntry getArp() const { const juce::SpinLock::ScopedLockType l (patLock); return arp; }
    void setStrum (const PatternEntry& p);              // el editor: queda como patrón propio
    void setArp (const PatternEntry& p);
    void loadStrumFromLibrary (int i);
    void loadArpFromLibrary (int i);
    std::function<void()> onPatternChanged;

    // ── Cadena ──
    std::array<int, dh::gt::kNumUnits> getOrder() const { return unpackOrder (orderPacked.load()); }
    void setOrder (const std::array<int, dh::gt::kNumUnits>& o);
    static juce::String orderToString (const std::array<int, dh::gt::kNumUnits>& o);
    static std::array<int, dh::gt::kNumUnits> orderFromString (const juce::String& s);

    // ── IR del usuario ──
    bool loadUserIr (const juce::File& f, juce::String& error);
    juce::String userIrName() const { return userIrNameStr.get(); }

    // ── MIDI ──
    bool exportPlayed (const juce::File& f);           // lo que la guitarra tocó (la grabación de la salida)
    bool exportPattern (const juce::File& f, int bars);   // el patrón actual sobre el acorde que suena (o C mayor)
    void clearRecording();
    int recordedEvents() const;

    // ── Para la vista ──
    struct ViewState
    {
        std::array<std::atomic<int>, dh::gt::kMaxStrings> note {};
        std::array<std::atomic<float>, dh::gt::kMaxStrings> fret {};
        std::array<std::atomic<float>, dh::gt::kMaxStrings> level {};
        std::atomic<int> strings { 6 }, art { 0 };
        std::atomic<float> hand { 1 };
        std::atomic<int> chordRoot { -1 }, chordType { 0 }, chordBass { -1 };   // (el texto lo arma la interfaz: sin memoria en el audio)
        std::atomic<float> outPeak { 0 };
    } view;
    juce::MidiKeyboardState keyboardState;

    std::vector<std::pair<const char*, std::atomic<float>*>> rawIds;
    std::vector<juce::String> rawIdStore;

private:
    dh::gt::Engine engine;
    double sr = 48000; int maxBlock = 512;
    std::vector<PresetEntry> presets; int currentProgram = 0;
    std::atomic<bool> presetDirty { false }, patternDirtyUi { false };
    mutable juce::SpinLock patLock;
    PatternEntry strum, arp; std::atomic<bool> patDirty { true };
    // El orden de la cadena en un entero atómico (15 unidades × 4 bits): el audio lo lee sin candado.
    std::atomic<uint64_t> orderPacked { 0 };
    static uint64_t packOrder (const std::array<int, dh::gt::kNumUnits>& o) { uint64_t v = 0; for (int i = 0; i < dh::gt::kNumUnits; ++i) v |= (uint64_t) (o[(size_t) i] & 15) << (4 * i); return v; }
    static std::array<int, dh::gt::kNumUnits> unpackOrder (uint64_t v) { std::array<int, dh::gt::kNumUnits> o {}; for (int i = 0; i < dh::gt::kNumUnits; ++i) o[(size_t) i] = (int) ((v >> (4 * i)) & 15); return o; }
    int lastStrumIdx = -1, lastArpIdx = -1;
    dh::LatencyReporter latency { *this };
    std::vector<dh::gt::MidiMsg> midiIn; std::vector<dh::gt::MidiOutEv> midiOut;
    std::vector<float> diScratch;
    std::array<std::bitset<128>, 16> hostNotes;          // notas de salida abiertas en el host (para cerrarlas siempre)
    bool lastMidiOut = false;

    // IR: diseño en un hilo, publicación por bandera.
    struct IrWorker;
    std::unique_ptr<IrWorker> worker;
    dh::gt::ConvKernel irStage; std::atomic<bool> irReady { false };       // armado por el hilo, tomado por el audio
    std::vector<float> userIr; double userIrRate = 48000.0; dh::SafeString userIrNameStr { "" }; std::atomic<bool> userIrDirty { false };
    juce::CriticalSection userIrLock;
    dh::gt::CabParams lastCab; int lastCabSource = -1, lastFactory = -1; bool cabInit = false;
    void requestIr (const dh::gt::EngineParams& p);

    // Grabación de lo tocado.
    struct Rec { int64_t t; uint8_t s, d1, d2; };
    std::vector<Rec> rec; std::atomic<int> recCount { 0 }; mutable juce::SpinLock recLock; double recBpm = 120.0; int64_t recOrigin = -1;

    void handleAsyncUpdate() override;
    void loadFactory();
    void applyPatternToEngine();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThrumProcessor)
};
