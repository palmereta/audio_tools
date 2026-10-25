#pragma once
// DeVicenti processor. The project (sounds, patterns, chain, effect modes, MIDI settings) lives here and in the
// state; the interface and the preset loader edit it on the message thread and mark what changed; the audio thread
// copies only the dirty parts into the engine with a try-lock (if the copy is busy it takes it on the next block).
// Edits that start on the audio thread (MIDI CC to a sound parameter, live recording) are applied to the engine at
// once and sent back to the message thread through lock-free queues.
//
// MIDI (plan §13.3): notes, bend, aftertouch, mod wheel, sustain go to the engine; CCs go through the fixed CC map
// (PanelMap.h) or a learned map, absolute or relative; program change selects the pattern (or the preset); in the
// standalone, MIDI clock in and out with start/stop. MIDI out (MIDI-machine tracks, doubled tracks, CC feedback, clock)
// leaves delayed by the plugin latency.

#include <JuceHeader.h>
#include "Params.h"
#include "ProjectIO.h"
#include "PanelMap.h"
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/PresetTracker.h"
#include "../../../shared/tools/LatencyReporter.h"
#include "../../../shared/tools/MidiOutDelay.h"
#include "../../../shared/tools/HostGap.h"

#ifndef DV_VERSION
 #define DV_VERSION "1.0.0"
#endif

class DeVicentiProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    DeVicentiProcessor();
    ~DeVicentiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    // The host's reset() (AudioUnitReset before a render, VST3 setProcessing(false)): as freshly prepared.
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
    const juce::String getName() const override { return "DeVicenti"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 10.0; }
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

    // ── Project (message thread) ──
    // Run `fn` on the project under the lock and mark what it touched (bits of Dirty, patterns by mask).
    enum Dirty : uint32_t { dTrack0 = 1, dPatches = 1 << 4, dChordMem = 1 << 5, dFx = 1 << 6, dMidi = 1 << 7, dTracks = 1 << 8, dChain = 1 << 9, dAll = 0x3FF };
    template <typename F> void editProject (F&& fn, uint32_t dirty, uint32_t patternMask = 0)
    {
        { const juce::SpinLock::ScopedLockType l (projLock); fn (*project); }
        dirtyBits.fetch_or (dirty); dirtyPatterns.fetch_or (patternMask);
    }
    std::unique_ptr<dh::dv::Project> projectCopy() const;     // (1.2 MB: on the heap)
    // Read the project under the lock (the interface reads what it draws this way; keep `fn` short).
    template <typename F> auto withProject (F&& fn) const { const juce::SpinLock::ScopedLockType l (projLock); return fn (std::as_const (*project)); }

    // ── Interface (message thread): the panel's controls through their targets (PanelMap.h) ──
    // `lane`: the sound of the track the Sound / operator targets act on (a KIT track's selected lane; 0 otherwise).
    float uiNorm (const dh::dv::panel::Target& t, int track, int lane = 0) const;
    void uiSet (const dh::dv::panel::Target& t, int track, float u, int lane = 0);
    void uiGesture (const dh::dv::panel::Target& t, int track, bool begin);
    float soundValue (int track, int lane, int param) const;
    void setSound (int track, int lane, int param, float value);
    // Notes from the panel's STEP keys (played like MIDI input, on the next block); panic and all notes off.
    void uiMidi (uint8_t status, uint8_t d1, uint8_t d2);
    std::atomic<bool> panicRequest { false }, notesOffRequest { false };
    // Chord memory capture (hold CHORD and play): while on, every note-on that reaches the engine (MIDI input and
    // the STEP keys) is marked here; the panel reads the marks when CHORD is let go.
    std::atomic<bool> chordCapture { false };
    std::array<std::atomic<uint8_t>, 128> captured {};
    juce::String projectJson (bool withMidi) const;

    // ── Presets ──
    struct PresetEntry { juce::String name, category, description; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    bool loadPresetVar (const juce::var& v, const juce::String& name);
    static juce::File userPresetDir();
    void rescanUserPresets();
    bool saveUserPreset (const juce::String& name);
    dh::SafeString currentPresetName { "Init" };
    dh::PresetTracker tracker { { "pattern", "tempo" } };
    juce::String presetDisplayName() { return tracker.displayName (*this, currentPresetName.get()); }
    std::function<void()> onPresetChanged;

    // ── Transport and performance (not host parameters) ──
    std::atomic<bool> standalonePlay { false }, fill { false }, songMode { false }, rec { false };
    // Direct jump (FUNC + STOP + pattern on the panel): the new pattern starts now, not at the end of the current one.
    void jumpToPattern (int index) { jumpRequest.store (juce::jlimit (0, dh::dv::kPatterns - 1, index)); }
    bool forceStandalone = false;                   // tests: behave like the standalone (internal clock, MIDI clock out)
    bool isStandalone() const { return forceStandalone || wrapperType == wrapperType_Standalone; }

    // ── MIDI learn ──
    void armLearn (const juce::String& targetId) { const juce::SpinLock::ScopedLockType l (learnLock); learnArmed = targetId; }
    juce::String learnedTarget (int channel, int cc) const;
    void clearLearn() { const juce::SpinLock::ScopedLockType l (learnLock); learned.clear(); }

    // ── For the view, the tools and the tests ──
    dh::dv::Engine& engineForTests() { return engine; }
    int engineLatency() const { return engine.latency(); }
    dh::dv::EngineParams buildParams() const;
    std::atomic<int> viewPattern { -1 };
    std::array<std::atomic<int>, dh::dv::kTracks> viewStep {};

private:
    dh::dv::Engine engine;
    std::unique_ptr<dh::dv::Project> project;
    mutable juce::SpinLock projLock;
    std::atomic<uint32_t> dirtyBits { dAll }, dirtyPatterns { 0xFFFF };
    double sr = 48000.0;
    std::vector<PresetEntry> presets; int currentProgram = 0;
    dh::MidiOutDelay midiDelay;
    dh::LatencyReporter latency { *this };
    dh::HostGap hostGap;
    std::vector<dh::dv::MidiIn> midiIn; std::vector<dh::dv::MidiOut> midiOut;
    std::array<float*, 10> outPtr {};
    std::vector<std::atomic<float>*> trackPtr;     // [t * 13 + k]
    std::atomic<float>* gPtr[12] {};
    double freeClock = 0.0; bool wasStandalonePlaying = false;
    // MIDI clock in (standalone).
    bool extPlaying = false; int64_t extTicks = 0; double extTickSamples = 0.0, extBpm = 120.0; int64_t sampleCount = 0, lastTickAt = -1;
    double clockOutNext = 0.0; bool clockRunning = false;
    // CC feedback: last value sent per track and CC.
    std::array<std::array<int16_t, 128>, dh::dv::kTracks> ccSent {};
    bool ccPrimed = false;
    // CC map lookup (fixed map): main and FUNC layer share one table of targets by CC number.
    std::array<dh::dv::panel::Target, 128> ccTarget {};
    // Learned map: (channel − 1) · 128 + cc → target id ("t1_cutoff", "swing", "s:<key>").
    std::map<int, juce::String> learned; juce::String learnArmed; mutable juce::SpinLock learnLock;
    // Audio → message queues.
    juce::AbstractFifo editFifo { 512 }; std::array<dh::dv::SoundEdit, 512> editBuf {};
    juce::AbstractFifo recFifo { 512 }; std::array<dh::dv::RecEvent, 512> recBuf {};
    juce::AbstractFifo uiMidiFifo { 256 }; std::array<std::array<uint8_t, 3>, 256> uiMidiBuf {};
    std::atomic<int> pendingProgram { -1 }, jumpRequest { -1 };
    std::map<int, std::pair<int, int>> recOpen;     // (track · 128 + note) → (pattern, step) of a note being recorded
    float lastAuto[dh::dv::kTracks][13] {};

    void handleAsyncUpdate() override;
    void loadFactory();
    void copyDirtyToEngine();
    void handleCc (int channel, int cc, int value, int offset);
    void applyTarget (const dh::dv::panel::Target& t, int track, float u, bool relative, float delta);
    void applyTargetId (const juce::String& id, int track, float u);
    float currentNorm (const dh::dv::panel::Target& t, int track);
    void ccFeedback (juce::MidiBuffer& out);
    void pushEdit (const dh::dv::SoundEdit& e);
    void applyRecord (const dh::dv::RecEvent& r);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeVicentiProcessor)
};
