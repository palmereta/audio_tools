#pragma once

#include <JuceHeader.h>
#include "../../../shared/tools/SafeString.h"
#include "../../../shared/tools/HostGap.h"
#include "SynthFs.h"
#include "PresetFs.h"

// Intercambio sin bloqueo de datos grandes (voz, Fseq) entre el hilo de mensajes y el de audio: tres
// lugares; el de mensajes escribe en uno que el audio no usa ni tiene pendiente y lo publica; el audio lo
// toma al principio del bloque. Nada se copia ni se libera en el hilo de audio.
template <typename T>
class TripleSlot
{
public:
    // Triple buffer: la UI escribe siempre en su casillero (back) y lo intercambia con el del medio; el audio,
    // si hay uno nuevo, intercambia el suyo (front) con el del medio. Ninguno toca nunca el casillero del otro
    // (la versión anterior dejaba una ventana en la que la UI pisaba el que estaba leyendo el audio).
    void publish (const T& v)
    {
        slots[(size_t) back] = v;
        back = middle.exchange (back | kNew) & kIdx;
    }
    const T& audio()   // hilo de audio
    {
        if (middle.load() & kNew) front = middle.exchange (front) & kIdx;
        return slots[(size_t) front];
    }
private:
    static constexpr int kNew = 4, kIdx = 3;
    std::array<T, 3> slots {};
    int back = 1, front = 0;
    std::atomic<int> middle { 2 };
};

class ATFS1Processor : public juce::AudioProcessor
{
public:
    ATFS1Processor();
    ~ATFS1Processor() override = default;

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
    bool wasBypassed = false;
    dh::HostGap hostGap;              // REAPER no llama al plugin en bypass: al volver se sueltan las notas (X1)

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ATFS1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }    // releases lentos del FS1R y reverb

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── parámetros del host ──
    juce::AudioProcessorValueTreeState apvts;
    float value (const juce::String& id) const { if (auto* p = apvts.getRawParameterValue (id)) return p->load(); return 0.0f; }
    void setValue (const juce::String& id, float v);

    // ── voz y Fseq (hilo de mensajes) ──
    const dh::fs::VoiceData& voice() const { return editVoice; }
    // La voz y el Fseq en edición los escribe la interfaz y los lee getStateInformation, que el host puede llamar
    // desde otro hilo (autoguardado): todo bajo editLock (y así hay un solo publicador por vez en las ranuras).
    template <typename Fn> void editVoiceWith (Fn&& fn) { { const juce::ScopedLock l (editLock); fn (editVoice); voiceSlot.publish (editVoice); } voiceDirty = true; notifyUi (onVoiceChanged); }
    void setVoice (const dh::fs::VoiceData& v) { { const juce::ScopedLock l (editLock); editVoice = v; voiceSlot.publish (editVoice); } notifyUi (onVoiceChanged); }
    const dh::fs::FseqData& fseq() const { return editFseq; }
    juce::String fseqRef() const { return currentFseqRef; }
    void setFseq (const dh::fs::FseqData& f, const juce::String& ref = {});
    void setFseqFromLibrary (int index);
    const std::vector<dh::fs::FseqData>& fseqLibrary() const { return library; }
    int libraryIndexOf (const juce::String& name) const;

    // Importar / exportar (.syx del FS1R, WAV → Fseq). Devuelven un texto para la pantalla.
    juce::String importSyx (const juce::File& f);
    bool exportVoiceSyx (const juce::File& f) const;
    bool exportFseqSyx (const juce::File& f) const;
    void analyzeAudioAsync (const juce::File& f);
    std::vector<dh::fs::VoiceData> importedVoices;   // voces del último .syx con más de una
    std::atomic<bool> analyzing { false };

    // Interacción en vivo (editor → audio), por una cola sin bloqueo.
    enum Command { CmdNoteOn = 1, CmdNoteOff, CmdPanic };
    void sendCommand (int cmd, int arg = 0, int arg2 = 100);

    // Estado visible (audio → editor).
    struct Live
    {
        std::array<std::atomic<float>, 16> opMeter {};
        std::atomic<int> voices { 0 };
        std::atomic<float> fseqPos { -1.0f }, outL { 0.0f }, outR { 0.0f };
        std::array<std::atomic<bool>, 128> keyDown {};
        std::array<float, 4096> scope {};          // salida en mono para el analizador (lectura sin bloqueo: se tolera un cuadro mezclado)
        std::atomic<int> scopePos { 0 };
    } live;
    std::atomic<float> cpuLoad { 0.0f };
    std::atomic<int> muteMode { 0 };      // MUTE/SOLO del frente: 0 todo, 1 sólo con voz, 2 sólo sin voz
    std::atomic<bool> power { true };
    std::atomic<bool> midiInFlash { false };
    std::function<void()> onPresetChanged, onVoiceChanged, onFseqChanged, onMessage;
    juce::String lastMessage;
    void postMessage (const juce::String& s) { lastMessage = s; notifyUi (onMessage); }

    // Presets
    struct PresetEntry { juce::String name, category; juce::var data; bool factory = true; juce::File file; };
    const std::vector<PresetEntry>& getPresets() const { return presets; }
    void loadPreset (int index);
    void loadPresetData (const dh::fs::Preset& p);
    dh::fs::Preset currentAsPreset (const juce::String& name, const juce::String& category) const;
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    dh::SafeString currentPresetName { "Init" };     // lo escribe el host desde cualquier hilo
    static juce::File userPresetDir();
    bool voiceDirty = false;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void notifyUi (std::function<void()>& cb);
    void loadFactory();
    void handleCc (int cc, int val);

    dh::fs::Synth synth;
    std::vector<std::atomic<float>*> raw;
    TripleSlot<dh::fs::VoiceData> voiceSlot;
    TripleSlot<dh::fs::FseqData> fseqSlot;
    juce::CriticalSection editLock;
    dh::fs::VoiceData editVoice;
    dh::fs::FseqData editFseq;
    juce::String currentFseqRef;
    std::vector<dh::fs::FseqData> library;
    double currentSr = 48000.0;
    std::vector<float> outL, outR;
    std::array<bool, 128> uiNote {}, midiNote {};

    juce::AbstractFifo cmdFifo { 256 };
    std::array<std::array<int, 3>, 256> cmdBuf {};

    std::vector<PresetEntry> presets;
    int currentProgram = 0;
    juce::ThreadPool pool { 1 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (ATFS1Processor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ATFS1Processor)
};
