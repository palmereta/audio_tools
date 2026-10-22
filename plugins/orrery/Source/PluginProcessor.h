#pragma once

#include <JuceHeader.h>
#include "ParamsOr.h"
#include "gen/Preview.h"
#include "gen/Constellation.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/PresetTracker.h"

// Salidas MIDI directas (sintes físicos, IAC en macOS, loopMIDI en Windows): cada carril elige un puerto.
// El hilo de audio deja los mensajes con su hora en una cola sin bloqueo; un hilo propio los manda a tiempo.
class DirectMidi : private juce::Thread
{
public:
    static constexpr int kSlots = 8;
    DirectMidi() : juce::Thread ("Orrery MIDI out") { startThread (juce::Thread::Priority::highest); }
    ~DirectMidi() override;

    // Hilo de mensajes: asigna un puerto (identificador del dispositivo) a un carril; vacío = ninguno.
    void setLanePort (int lane, const juce::String& identifier);
    void reopenMissing();
    bool laneConnected (int lane) const { return lanePorts[(size_t) lane].isEmpty() || laneSlot[(size_t) lane].load() >= 0; }
    void setLaneChannel (int lane, int channel) { laneChannel[(size_t) lane].store (channel, std::memory_order_relaxed); }
    juce::String lanePort (int lane) const { return lanePorts[(size_t) lane]; }
    bool anyOpen() const { for (auto& s : laneSlot) if (s.load() >= 0) return true; return false; }

    // Los carriles como puertos MIDI virtuales (macOS y Linux): uno por carril, con el nombre dado. Vacío = ninguno.
    // Hilo de mensajes. Devuelve cuántos se pudieron crear.
    int setVirtualPorts (const juce::StringArray& names);
    juce::StringArray virtualPortNames() const { const juce::ScopedLock sl (sendLock); return virtNames; }
    bool virtualPortsOpen() const { for (auto& v : virtOn) if (v.load()) return true; return false; }

    // Hilo de audio.
    int slotForLane (int lane) const { return laneSlot[(size_t) lane].load (std::memory_order_relaxed); }
    int virtualSlotForLane (int lane) const { return virtOn[(size_t) lane].load (std::memory_order_relaxed) ? kSlots + lane : -1; }
    void push (double atMs, int slot, const uint8_t* data, int size);
    void pushToAll (double atMs, const uint8_t* data, int size);   // reloj, start/stop

private:
    struct Msg { double at; int8_t slot; uint8_t size; uint8_t data[3]; };
    juce::AbstractFifo fifo { 4096 };
    std::array<Msg, 4096> buf {};
    std::array<std::atomic<int>, dh::orr::kLanes> laneSlot {}, laneChannel {};
    void allNotesOffOn (int slot, int channel);      // con sendLock tomado
    std::array<juce::String, dh::orr::kLanes> lanePorts;
    std::array<std::unique_ptr<juce::MidiOutput>, kSlots> slots;
    std::array<juce::String, kSlots> slotIds;
    std::array<std::unique_ptr<juce::MidiOutput>, dh::orr::kLanes> virt;
    std::array<std::atomic<bool>, dh::orr::kLanes> virtOn {};
    juce::StringArray virtNames;
    juce::MidiOutput* outputFor (int slot) const;        // con sendLock
    juce::CriticalSection sendLock;
    void run() override;
};

// Vínculo entre instancias del mismo proceso: la líder publica su armonía y las seguidoras la leen.
struct LinkHub
{
    static LinkHub& get() { static LinkHub h; return h; }
    void publish (const dh::orr::Harmony& h) { seq.fetch_add (1); data = h; seq.fetch_add (1); }
    bool read (dh::orr::Harmony& h) const
    {
        for (int i = 0; i < 4; ++i)
        {
            const uint32_t a = seq.load();
            if (a & 1) continue;
            h = data;
            if (seq.load() == a) return true;
        }
        return false;
    }
    std::atomic<int> leaders { 0 };
private:
    std::atomic<uint32_t> seq { 0 };
    dh::orr::Harmony data;
};

class OrreryProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    OrreryProcessor();
    ~OrreryProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void updateTrackProperties (const TrackProperties&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orrery"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 16.0; }     // la reverb del sonido de prueba (~14 s) y un release de drone

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

    DirectMidi direct;
    bool wasBypassed = false, previewWasOn = false;
    struct DirectItem { double at; int slot; uint8_t data[3]; uint8_t size; };
    std::vector<DirectItem> directQ;     // mensajes del bloque a los puertos directos, ordenados por hora antes de encolar
    void queueDirect (double at, int slot, const uint8_t* d, int size);
    void flushDirect();
    const dh::orr::Generator::View& view() const { return effRole.load() == dh::orr::RoleSatellite ? sat.rep.view : gen.view; }
    std::atomic<int> midiOutFlash { 0 };

    // Chispas para el orrery: cada nota que sale.
    struct Spark { int lane, note, vel; };
    juce::AbstractFifo sparkFifo { 512 };
    std::array<Spark, 512> sparks {};

    void panic() { panicRequest.store (true); }

    // ── Main y satélites (gen/Constellation.h)
    int role() const { return effRole.load(); }                     // 0 sin resolver, 1 independiente, 2 Main, 3 satélite
    uint32_t satelliteLanes() const { return effLanes.load(); }
    int satelliteSync() const { return sat.state.load(); }
    const dh::orr::LaneParams& satelliteLane (int l) const { return sat.rep.lanes[(size_t) juce::jlimit (0, dh::orr::kLanes - 1, l)]; }
    const dh::orr::GlobalParams& satelliteGlobals() const { return sat.rep.g; }
    struct SatInfo { juce::String name; uint32_t lanes = 0; int sync = 0; bool alive = false; };
    std::vector<SatInfo> satellites() const;                        // los satélites que siguen a esta (si es Main)
    juce::String mainName() const;                                  // el Main que sigue (si es satélite)
    bool otherMainActive() const;                                   // hay otro Main más antiguo (éste no manda)
    juce::String displayName() const;                               // nombre de la pista, o "Orrery"
    void setRoleFromUi (int roleSetting);                           // el menú de rol
    void makeIndependentFromMain();                                 // satélite → independiente con los ajustes del Main
    uint32_t skippedLanes() const { return skipped.load(); }
    void housekeeping();                                            // hilo de mensajes: rol, puertos virtuales
    static bool virtualPortsSupported();
    // Capturas del manual (ORRERY_SAT_DEMO): otras Orrery del mismo proceso, procesadas antes (Main) o después
    // (satélites) de ésta en cada bloque, como pistas de un DAW. Sólo las crea el editor en ese modo.
    void addDemoInstance (std::unique_ptr<OrreryProcessor> p, bool before);
    // Guarda los últimos "minutes" minutos como archivo MIDI (una pista por canal). Devuelve el archivo.
    juce::File captureToFile (double minutes, const juce::File& dir) const;
    bool newVariation();      // cambia la semilla

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
    dh::orr::Generator gen;
    dh::orr::Preview preview;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> vals;
    std::vector<float> monoR;                     // la vista previa en una pista mono
    std::vector<dh::orr::OutEvent> events;
    double sr = 48000.0;
    std::atomic<bool> panicRequest { false };
    bool wasRunning = false;
    double clockBeat = 0.0;
    int linkMode = 0;
    // Main y satélites
    int regSlot = -1;                                               // en Constellation
    std::atomic<int> effRole { 0 }, autoRole { 0 };
    std::atomic<uint32_t> effLanes { 0 }, autoLanes { 0 }, skipped { 0 };
    std::atomic<int> regRole { -1 }; std::atomic<uint32_t> regLanes { 0 };   // lo registrado (se escribe con el lock del registro)
    int lastRole = -1;
    std::atomic<bool> resetPending { false }, restoring { false };
    dh::orr::MainCore mainCore;
    dh::orr::SatelliteCore sat;
    std::vector<dh::orr::OutEvent> genEvents;
    std::vector<std::unique_ptr<OrreryProcessor>> demoOwned;
    std::atomic<OrreryProcessor*> demoBefore { nullptr };
    std::array<std::atomic<OrreryProcessor*>, 4> demoAfter {};
    juce::AudioBuffer<float> demoBuf { 2, 8192 }; juce::MidiBuffer demoMidi;
    void runDemo (std::atomic<OrreryProcessor*>& slot_, int n);
    juce::String trackName;                                         // hilo de mensajes
    void updateRole (bool blocking);
    static std::vector<OrreryProcessor*>& allInstances();
    void timerCallback() override { housekeeping(); }
    dh::orr::Generator& active() { return effRole.load() == dh::orr::RoleSatellite ? sat.rep : gen; }
    std::vector<PresetEntry> presets;
    int currentProgram = 0;
    void loadFactory();
    void notifyUi (std::function<void()>& cb);

    JUCE_DECLARE_WEAK_REFERENCEABLE (OrreryProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrreryProcessor)
};
