#pragma once

#include <JuceHeader.h>
#include "../../../shared/tools/HostGap.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/LatencyReporter.h"
#include "../../../shared/tools/PresetTracker.h"
#include "ExtParams700.h"
#include "Preset700.h"

class AT700Processor : public juce::AudioProcessor
{
public:
    AT700Processor();
    ~AT700Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AT700"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }     // releases largos y spring reverb

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── para el editor ──
    juce::AudioProcessorValueTreeState apvts;
    float value (dh::P700 p) const { return panelRaw[(size_t) p]->load(); }
    float value (int i) const { return panelRaw[(size_t) i]->load(); }
    void setValue (int i, float v);
    void setValue (dh::P700 p, float v) { setValue ((int) p, v); }
    void setExtValue (const juce::String& id, float v);
    float extValue (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    // Copia bajo lock: el host puede restaurar el estado desde otro hilo mientras el editor dibuja.
    std::vector<dh::Cable700> getCables() const { const juce::SpinLock::ScopedLockType l (cableLock); return cables; }
    void setCables (std::vector<dh::Cable700> c);
    std::function<void()> onCablesChanged, onPresetChanged;

    // Interacción en vivo (editor → audio), por una cola sin bloqueo.
    enum Command { CmdNoteOn = 1, CmdNoteOff, CmdSeqStart, CmdSeqContinue, CmdSeqStop, CmdSeqEnd, CmdSeqStep, CmdSeqChan, CmdPanic };
    void sendCommand (int cmd, int arg = 0);
    std::atomic<bool> manualGate { false };
    std::atomic<float> uiBender { 0.0f };

    // Estado visible (audio → editor).
    struct Live
    {
        std::array<std::atomic<float>, 2> vcfOver {}, vcfSig {}, vcaOver {}, vcaSig {};
        std::atomic<float> rmSig { 0 }, ef { 0 }, lfo1 { 0 }, lfo2 { 0 }, vuL { 0 }, vuR { 0 }, pshaper { 0 }, clock { 0 }, bender { 0 };
        std::atomic<int> step { 0 }, chan { 0 };
        std::atomic<bool> running { false }, power { true };
        std::array<std::atomic<bool>, 128> keyDown {};
    } live;
    std::atomic<float> cpuLoad { 0.0f };
    std::atomic<bool> midiInFlash { false };

    // Presets
    struct PresetEntry { juce::String name, category; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    void loadPresetData (const dh::Preset700& p, bool sequencerCommands = true);
    dh::Preset700 currentAsPreset (const juce::String& name, const juce::String& category) const;
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    void randomizePanel();
    dh::SafeString currentPresetName { "Init" };     // lo escribe el host desde cualquier hilo
    dh::PresetTracker tracker;                         // preset "modificado": la foto de los parámetros al cargarlo
    dh::LatencyReporter latency { *this };
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }
    static juce::File userPresetDir();

    // MIDI learn.
    void startMidiLearn (const juce::String& paramId) { learnIndex.store (allParamIds.indexOf (paramId)); learning.store (true); }
    void clearMidiMapping (const juce::String& paramId);
    int ccForParam (const juce::String& paramId) const;
    bool isLearning() const { return learning.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (const juce::MidiMessage& m);
    void notifyUi (std::function<void()>& cb);
    void loadFactoryPresets();

    dh::Engine700 engine;
    std::array<std::atomic<float>*, dh::kNumP700> panelRaw {};
    dh::Panel700 smoothed {};
    std::vector<std::atomic<float>*> extRaw;
    std::vector<dh::Cable700> cables;
    juce::SpinLock cableLock;
    bool wasBypassed = false;
    dh::HostGap hostGap;          // REAPER no llama al plugin en bypass: al volver se sueltan las notas (X1)
    std::array<std::atomic<int16_t>, dh::jk700::kNumJacks> patchAtomic;
    std::atomic<int> patchVersion { 0 };
    int seenPatchVersion = -1;
    double currentSr = 48000.0;
    int currentBlock = 512;
    std::vector<float> outL, outR;
    float midiBend = 0.0f;
    std::array<bool, 128> uiNote {}, midiNote {}, sustained {};
    bool sustainPedal = false;

    juce::AbstractFifo cmdFifo { 256 };
    std::array<std::pair<int, int>, 256> cmdBuf {};

    std::array<std::atomic<int>, 128> ccMap;
    std::atomic<bool> learning { false };
    std::atomic<int> learnIndex { -1 };      // el hilo de audio sólo lee un entero
    juce::StringArray allParamIds;
    std::atomic<int> pendingLearnCc { -1 };

    std::vector<PresetEntry> presets;
    int currentProgram = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AT700Processor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AT700Processor)
};
