#pragma once

#include <JuceHeader.h>
#include "../../../shared/tools/HostGap.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/LatencyReporter.h"
#include "../../../shared/tools/PresetTracker.h"
#include "../../../shared/tools/AutoGesture.h"
#include "ExtParams42.h"
#include "Preset42.h"

class AT42Processor : public juce::AudioProcessor
{
public:
    AT42Processor();
    ~AT42Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    // reset() del host (AudioUnitReset antes de un render, VST3 setProcessing(false)): lo mismo que preparar de nuevo, con
    // el lock del proceso tomado. REAPER hace dos renders seguidos de un AU sin volver a prepararlo, sólo con
    // AudioUnitReset, y el segundo salía distinto (QA en REAPER 2026-10-22, re-test).
    void reset() override
    {
        const juce::ScopedLock sl (getCallbackLock());
        if (getSampleRate() > 0.0 && getBlockSize() > 0) prepareToPlay (getSampleRate(), getBlockSize());
    }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AT42"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }     // releases de los drones y efector

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── para el editor ──
    juce::AudioProcessorValueTreeState apvts;
    float value (dh::P42 p) const { return panelRaw[(size_t) p]->load(); }
    void setValue (dh::P42 p, float v);
    void setExtValue (const juce::String& id, float v);
    float extValue (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    // Cables (los edita el hilo de mensajes; el audio lee la tabla atómica).
    // Copia bajo lock: el host puede restaurar el estado desde otro hilo mientras el editor dibuja.
    std::vector<dh::Cable42> getCables() const { const juce::SpinLock::ScopedLockType l (cableLock); return cables; }
    void setCables (std::vector<dh::Cable42> c);
    std::function<void()> onCablesChanged, onPresetChanged;

    // Interacción en vivo (editor → audio).
    std::array<std::atomic<bool>, 7> droneKeyUi {};
    std::array<std::atomic<float>, 4> orbShade {};
    void platePress (int plate, float pressure);
    void plateRelease (int plate);
    std::atomic<int> uiKeysOctave { 0 };
    enum Command { CmdSeqRec = 1, CmdPanic };
    void sendCommand (Command c) { pendingCmd.store ((int) c); }

    // Estado visible (audio → editor).
    struct Live
    {
        std::array<std::atomic<float>, 7> droneEnv {}, orbLight {}, orbLed {};
        std::array<std::atomic<bool>, 7> shFlash {};
        std::atomic<float> egA { 0 }, egB { 0 }, lfoA { 0 }, lfoB { 0 }, efEnv { 0 }, pressure { 0 };
        std::atomic<bool> efGate { false }, preClip { false }, recording { false };
        std::atomic<int> seqStep { 0 }, kbStep { -1 }, kbSeqCount { 0 };
        std::atomic<float> seqGate { 0 };
        std::array<std::atomic<bool>, 12> plateDown {};
        std::array<std::atomic<bool>, 7> droneKeyDown {};
    } live;
    std::atomic<float> cpuLoad { 0.0f };
    std::atomic<bool> midiInFlash { false };

    // Presets
    struct PresetEntry { juce::String name, category; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    void loadPresetData (const dh::Preset42& p);
    dh::Preset42 currentAsPreset (const juce::String& name, const juce::String& category) const;
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    void randomizePanel();
    dh::SafeString currentPresetName { "Init" };     // lo escribe el host desde cualquier hilo
    dh::PresetTracker tracker;                         // preset "modificado": la foto de los parámetros al cargarlo
    dh::LatencyReporter latency { *this };
    dh::AutoGesture gesture;                           // gestos de automatización de botones, llaves y joystick
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

    dh::Engine42 engine;
    std::array<std::atomic<float>*, dh::kNumP42> panelRaw {};
    dh::Panel42 smoothed {};
    std::vector<std::atomic<float>*> extRaw;
    std::vector<dh::Cable42> cables;
    juce::SpinLock cableLock;
    bool wasBypassed = false;
    dh::HostGap hostGap;          // REAPER no llama al plugin en bypass: al volver se sueltan las notas (X1)
    std::array<std::atomic<int8_t>, dh::jk42::kNumJacks> patchAtomic;
    double currentSr = 48000.0, freePpq = 0.0;
    int currentBlock = 512;
    std::vector<float> outL, outR, inCopy;

    // Placas (editor) → audio por una cola sin bloqueo.
    juce::AbstractFifo plateFifo { 64 };
    std::array<std::pair<int, float>, 64> plateBuf {};
    std::array<int, 128> midiPlateNote {};
    std::array<bool, 7> midiDrone {};
    std::atomic<int> pendingCmd { 0 };
    std::array<int, 16> pendingSeq {}; std::atomic<int> pendingSeqCount { -1 };

    std::array<std::atomic<int>, 128> ccMap;
    std::atomic<bool> learning { false };
    std::atomic<int> learnIndex { -1 };      // el hilo de audio sólo lee un entero
    juce::StringArray allParamIds;
    std::atomic<int> pendingLearnCc { -1 };

    std::vector<PresetEntry> presets;
    int currentProgram = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AT42Processor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AT42Processor)
};
