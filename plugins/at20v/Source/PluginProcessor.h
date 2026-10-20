#pragma once
#include <bitset>

#include <JuceHeader.h>
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/LatencyReporter.h"
#include "../../../shared/tools/PresetTracker.h"
#include "Preset.h"
#include "ParamInfo.h"
#include "ExtParams.h"

class AT20VProcessor : public juce::AudioProcessor,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    AT20VProcessor();
    ~AT20VProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AT20V"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 8.0; }     // releases largos, delay y reverb

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Para el editor ──
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // Cables: los edita el hilo de mensajes; el audio lee la tabla atómica.
    // Copia bajo lock: el host puede restaurar el estado desde otro hilo mientras el editor dibuja los cables.
    std::vector<dh::Cable> getCables() const { const juce::SpinLock::ScopedLockType l (cableLock); return cables; }
    void setCables (std::vector<dh::Cable> newCables);
    std::function<void()> onCablesChanged;

    // Presets
    struct PresetEntry { juce::String name, category; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    void loadPresetData (const dh::Preset& p);
    dh::Preset currentAsPreset (const juce::String& name, const juce::String& category) const;
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    dh::SafeString currentPresetName { "Init" };     // lo escribe el host desde cualquier hilo
    dh::PresetTracker tracker { { "quality", "output", "midi_out", "bend_range" } };   // los ajustes de sesión no "modifican"                         // preset "modificado": la foto de los parámetros al cargarlo
    dh::LatencyReporter latency { *this };
    std::vector<float> bypassL, bypassR; int bypassPos = 0;   // AT20V FX: el seco del bypass
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }
    std::function<void()> onPresetChanged;

    // Estado en vivo para LEDs (escrito por el audio, leído por la UI).
    std::atomic<bool> ledMg { false }, ledEspPeak { false }, ledEspTrig { false };
    std::atomic<float> ledEg { 0.0f };

    // Barra inferior: actividad MIDI, voces, carga y nivel de entrada.
    std::atomic<bool> midiInFlash { false }, midiOutFlash { false };
    std::atomic<int> activeVoices { 1 };
    std::atomic<float> cpuLoad { 0.0f }, inputLevel { 0.0f };

    // Rueda de control y pulsador (los mueve la UI o el MIDI).
    std::atomic<float> wheel { 0.0f };
    std::atomic<bool> momentary { false };          // el pulsador apretado con el mouse
    std::atomic<bool> momentaryPedal { false };     // el pedal de sustain (CC64): el mouse no lo suelta ni lo pisa

    static juce::File userPresetDir();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();
    void parameterChanged (const juce::String& id, float newValue) override;
    void loadFactoryPresets();
    void handleMidi (const juce::MidiMessage& m);
    void notifyUi (std::function<void()>& cb);

    dh::Engine engine;
    std::array<std::atomic<float>*, dh::kNumKnobs> knobParams {};
    dh::KnobValues smoothed {};
    std::array<std::atomic<int8_t>, dh::kNumJacks> patchAtomic;
    std::vector<dh::Cable> cables;
    juce::SpinLock cableLock;

    std::vector<std::atomic<float>*> extRaw;
    std::atomic<float>* pQuality = nullptr; std::atomic<float>* pMidiOut = nullptr;
    double freePpq = 0.0;
    int patchNote = -1, pendingPatchOff = -1;     // nota de MIDI out (modo Patch); la que quedó al re-preparar
    bool wasBypassed = false;
    std::array<std::bitset<128>, 16> thruOn;     // notas reenviadas en Thru que todavía no recibieron su note-off
    void forwardThru (const juce::MidiBuffer& in, juce::MidiBuffer& out, int n, bool thru);
    float modWheel = 0.0f, bendWheel = 0.0f;     // CC1 y pitch bend mueven la misma rueda: se suman

    std::vector<PresetEntry> presets;
    int currentProgram = 0;
    double currentSr = 48000.0;
    int currentBlock = 512;
    std::atomic<int> pendingQuality { -1 };
    std::vector<float> espMono, monoOut, rightOut;
    bool espLedLatch = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AT20VProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AT20VProcessor)
};
