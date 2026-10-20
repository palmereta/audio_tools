#pragma once
#include <bitset>

#include <JuceHeader.h>
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/LatencyReporter.h"
#include "../../../shared/tools/PresetTracker.h"
#include "../../../shared/tools/AutoGesture.h"
#include "ExtParams101.h"
#include "Preset101.h"

class AT101VProcessor : public juce::AudioProcessor
{
public:
    AT101VProcessor();
    ~AT101VProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AT101V"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 6.0; }     // release, delay y reverb

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── para el editor ──
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    float panelValue (dh::P p) const { return panelRaw[(size_t) p]->load(); }
    void setPanelValue (dh::P p, float v);
    void setExtValue (const juce::String& id, float v);

    // Botones con acción (no son parámetros): LOAD, REST, LEGATO, KEY TRANSPOSE.
    enum Command { CmdLoadToggle = 1, CmdRest, CmdTie, CmdKeyTranspose, CmdKeyTransposeReset, CmdPanic };
    void sendCommand (Command c);

    // Secuencia compartida con el editor de pasos.
    dh::Sequence getSequence() const;
    void editSequence (const std::function<void (dh::Sequence&)>& fn);
    std::atomic<int> seqVersion { 0 };

    // Presets
    struct PresetEntry { juce::String name, category; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    void loadPresetData (const dh::Preset& p, bool keepSequence = false);
    dh::Preset currentAsPreset (const juce::String& name, const juce::String& category) const;
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    void randomizePanel();
    dh::SafeString currentPresetName { "Init" };     // lo escribe el host desde cualquier hilo
    dh::PresetTracker tracker;
    dh::LatencyReporter latency { *this };
    dh::AutoGesture gesture;                          // gestos de automatización de lo que mueve la interfaz
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }
    std::function<void()> onPresetChanged;
    static juce::File userPresetDir();

    // MIDI learn: el próximo CC queda asignado al parámetro pedido.
    // El índice se calcula acá (hilo de la UI): el hilo de audio sólo lee un entero.
    void startMidiLearn (const juce::String& paramId) { learnIndex.store (allParamIds.indexOf (paramId)); learning.store (true); }
    void clearMidiMapping (const juce::String& paramId);
    int ccForParam (const juce::String& paramId) const;
    bool isLearning() const { return learning.load(); }

    // Estado en vivo (lo escribe el audio, lo lee la UI).
    std::atomic<bool> rateLed { false }, loadingLed { false }, keyTrArmed { false }, powerOn { true };
    std::atomic<int> keyTranspose { 0 }, stepPos { -1 }, soundingNote { -1 }, activeVoices { 0 };
    std::atomic<bool> midiInFlash { false }, midiOutFlash { false };
    std::atomic<float> cpuLoad { 0.0f };
    std::atomic<float> bender { 0.0f }, lfoPush { 0.0f };
    std::atomic<bool> holdPedal { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void loadFactoryPresets();
    void handleMidi (const juce::MidiMessage& m);
    void notifyUi (std::function<void()>& cb);

    dh::Engine101 engine;
    std::array<std::atomic<float>*, dh::kNumP> panelRaw {};
    dh::PanelValues smoothed {};
    std::vector<std::atomic<float>*> extRaw;
    std::atomic<float>* pQuality = nullptr; std::atomic<float>* pMidiOut = nullptr;
    double freePpq = 0.0;
    double currentSr = 48000.0;
    int currentBlock = 512;
    std::vector<float> outL, outR;

    // Comandos de la UI → audio.
    juce::AbstractFifo cmdFifo { 64 };
    std::array<int, 64> cmdBuf {};

    // Secuencia: copia compartida (UI) y la del motor (audio).
    mutable juce::SpinLock seqLock;
    dh::Sequence sharedSeq;
    int engineSeqVersion = -1;

    // MIDI learn.
    std::array<std::atomic<int>, 128> ccMap;        // CC → índice de parámetro (−1 = libre)
    std::atomic<bool> learning { false };
    std::atomic<int> learnIndex { -1 };
    std::atomic<int> pendingKeyTranspose { 1000 };      // estado restaurado → motor (1000 = nada pendiente)
    bool wasBypassed = false;
    std::array<std::bitset<128>, 16> thruOn;            // notas reenviadas en Thru todavía abiertas
    std::bitset<128> synthOn;                           // notas del modo Synth Notes que salieron y siguen abiertas
    void forwardThru (const juce::MidiBuffer& in, juce::MidiBuffer& out, int n, bool thru);
    juce::StringArray allParamIds;
    std::atomic<int> pendingLearnCc { -1 };

    std::vector<PresetEntry> presets;
    int currentProgram = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AT101VProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AT101VProcessor)
};
