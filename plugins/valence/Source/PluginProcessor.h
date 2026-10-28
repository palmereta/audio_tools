#pragma once

#include <JuceHeader.h>
#include "ParamsVal.h"
#include "gen/Preview.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/PresetTracker.h"
#include "../../../shared/tools/HostGap.h"

namespace dh::val {

// Salidas MIDI directas (sintes físicos, IAC en macOS, loopMIDI en Windows): cada capa elige un puerto, y en macOS y
// Linux cada capa puede publicarse como un puerto virtual propio. El hilo de audio deja los mensajes con su hora en una
// cola sin bloqueo; un hilo propio los manda a tiempo (lo mismo que Orrery, con cuatro capas).
class DirectMidi : private juce::Thread
{
public:
    static constexpr int kSlots = 4;
    DirectMidi() : juce::Thread ("Valence MIDI out") { startThread (juce::Thread::Priority::highest); }
    ~DirectMidi() override;

    void setLayerPort (int layer, const juce::String& identifier);     // hilo de mensajes; vacío = ninguno
    void reopenMissing();
    bool layerConnected (int layer) const { return layerPorts[(size_t) layer].isEmpty() || layerSlot[(size_t) layer].load() >= 0; }
    void setLayerChannel (int layer, int channel) { layerChannel[(size_t) layer].store (channel, std::memory_order_relaxed); }
    juce::String layerPort (int layer) const { return layerPorts[(size_t) layer]; }
    bool anyOpen() const { for (auto& s : layerSlot) if (s.load() >= 0) return true; return false; }

    int setVirtualPorts (const juce::StringArray& names);              // hilo de mensajes; devuelve cuántos creó
    juce::StringArray virtualPortNames() const { const juce::ScopedLock sl (sendLock); return virtNames; }
    bool virtualPortsOpen() const { for (auto& v : virtOn) if (v.load()) return true; return false; }

    // Hilo de audio.
    int slotForLayer (int layer) const { return layerSlot[(size_t) layer].load (std::memory_order_relaxed); }
    int virtualSlotForLayer (int layer) const { return virtOn[(size_t) layer].load (std::memory_order_relaxed) ? kSlots + layer : -1; }
    void push (double atMs, int slot, const uint8_t* data, int size);
    void pushToAll (double atMs, const uint8_t* data, int size);        // reloj, start/stop

private:
    struct Msg { double at; int8_t slot; uint8_t size; uint8_t data[3]; };
    juce::AbstractFifo fifo { 4096 };
    std::array<Msg, 4096> buf {};
    std::array<std::atomic<int>, kLayers> layerSlot {}, layerChannel {};
    void allNotesOffOn (int slot, int channel);
    std::array<juce::String, kLayers> layerPorts;
    std::array<std::unique_ptr<juce::MidiOutput>, kSlots> slots;
    std::array<juce::String, kSlots> slotIds;
    std::array<std::unique_ptr<juce::MidiOutput>, kLayers> virt;
    std::array<std::atomic<bool>, kLayers> virtOn {};
    juce::StringArray virtNames;
    juce::MidiOutput* outputFor (int slot) const;
    juce::CriticalSection sendLock;
    void run() override;
};

// Vínculo entre instancias del mismo proceso: la líder publica su acorde y las seguidoras tocan sobre él.
struct LinkHub
{
    static LinkHub& get() { static LinkHub h; return h; }
    void publish (const ChordShare& c) { seq.fetch_add (1); data = c; seq.fetch_add (1); }
    bool read (ChordShare& c) const
    {
        for (int i = 0; i < 4; ++i)
        {
            const uint32_t a = seq.load();
            if (a & 1) continue;
            c = data;
            if (seq.load() == a) return true;
        }
        return false;
    }
    std::atomic<int> leaders { 0 };
private:
    std::atomic<uint32_t> seq { 0 };
    ChordShare data;
};

} // namespace dh::val

class ValenceProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ValenceProcessor();
    ~ValenceProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Valence"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
    double getTailLengthSeconds() const override { return 2.0; }      // la reverb corta del sonido de prueba

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

    // Filas de pasos (el estado, no parámetros). Hilo de mensajes; el audio las toma cuando cambia la versión.
    dh::val::Rows getRows() const { const juce::SpinLock::ScopedLockType l (rowsLock); return rows; }
    void setRows (const dh::val::Rows& r);
    void setStep (int layer, int step, const dh::val::Step& s);

    dh::val::DirectMidi direct;
    const dh::val::Engine::View& view() const { return engine.view; }
    std::atomic<int> midiOutFlash { 0 };

    // Cada nota que sale, para la interfaz (el átomo de la Fase 2): capa, nota, velocidad, nucleón, nota de origen, paso.
    struct Spark { int layer, note, vel, nucleon, src, step, flags; };
    juce::AbstractFifo sparkFifo { 1024 };
    std::array<Spark, 1024> sparks {};

    void panic() { panicRequest.store (true); }
    static bool virtualPortsSupported();
    juce::File captureToFile (double minutes, const juce::File& dir) const;
    bool newVariation();

    // Presets
    struct PresetEntry { juce::String name, category; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    dh::SafeString currentPresetName { "Init" };
    dh::PresetTracker tracker;
    juce::String presetDisplayName() { return tracker.modified (*this) || rowsEdited.load() ? currentPresetName.get() + " *" : currentPresetName.get(); }
    static juce::File userPresetDir();
    std::function<void()> onPresetChanged;
    int editorPage = 0;          // la última página del editor (no va en el estado): al reabrirlo vuelve a ella

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray& groups();
    static BusesProperties buses();

private:
    dh::val::Engine engine;
    dh::val::Preview preview;
    dh::HostGap hostGap;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> vals;
    std::vector<dh::val::OutEvent> events;
    std::vector<dh::val::InEvent> inEvents;
    double sr = 48000.0, lastHostBpm = 0.0;
    std::atomic<bool> panicRequest { false }, resetPending { false };
    bool wasBypassed = false, previewWasOn = false, lastRunParam = false;
    int linkMode = 0;
    juce::String instanceId;
    // Filas
    mutable juce::SpinLock rowsLock;
    dh::val::Rows rows = dh::val::defaultRows();
    std::atomic<uint32_t> rowsVersion { 1 };
    uint32_t rowsApplied = 0;
    std::atomic<bool> rowsEdited { false };
    // Salidas directas: los mensajes del bloque, ordenados por hora antes de encolar.
    struct DirectItem { double at; int slot; uint8_t data[3]; uint8_t size; };
    std::vector<DirectItem> directQ;
    void queueDirect (double at, int slot, const uint8_t* d, int size);
    void flushDirect();
    void sendOut (juce::MidiBuffer& midi, const dh::val::OutEvent& e, int n, double blockStartMs);
    void mirrorRun (bool on);
    void timerCallback() override { housekeeping(); }
    void housekeeping();
    std::vector<PresetEntry> presets;
    int currentProgram = 0;
    void loadFactory();
    void notifyUi (std::function<void()>& cb);

    JUCE_DECLARE_WEAK_REFERENCEABLE (ValenceProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValenceProcessor)
};
