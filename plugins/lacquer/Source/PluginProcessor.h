#pragma once
// Lacquer: masterizador automático. Dos maneras de usarlo:
//   - En el bus master: LEARN escucha la parte más fuerte (hasta 30 s), decide la cadena y la aplica en vivo.
//   - Con un archivo (Master a file…): analiza la canción entera, la renderiza, la deja escuchar en A/B con la misma
//     sonoridad y exporta el master con su informe. Mientras se escucha un archivo, la entrada del plugin se silencia.
// La cadena nueva se arma en un hilo aparte y se entrega al de audio sin bloqueos; el cambio se funde en 50 ms.

#include <JuceHeader.h>
#include "ParamsLq.h"
#include "PlanJson.h"
#include "LiveMeter.h"

class LacquerProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    LacquerProcessor();
    ~LacquerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Lacquer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    float value (const juce::String& id) const { if (auto* p = apvts.getRawParameterValue (id)) return p->load(); return 0.0f; }
    void setValue (const juce::String& id, float v);
    dh::lq::Options currentOptions() const;

    // ---- Aprendizaje en vivo ----
    enum class LearnState { idle, listening, analysing, ready };
    std::atomic<LearnState> learnState { LearnState::idle };
    void startLearning();
    void stopLearning();                       // termina (con al menos 8 s) o cancela
    void learnFromFile (const juce::File& f);  // capturas y pruebas: como si el archivo hubiera sonado durante LEARN
    double learnedSeconds() const { return (double) learnCount.load() / sr; }
    static constexpr double kLearnSeconds = 360.0, kLearnQuickSeconds = 30.0, kLearnMinSeconds = 8.0;

    // ---- Modo archivo ----
    enum class FileState { none, loading, rendering, ready, failed };
    std::atomic<FileState> fileState { FileState::none };
    void openFile (const juce::File& f);
    void closeFile();
    bool exportMaster (const juce::File& dest, int bits, juce::String& error);
    std::atomic<bool> filePlaying { false };
    std::atomic<double> filePos { 0.0 };       // segundos
    double fileSeconds() const { return fileLen > 0 ? (double) fileLen / fileRate : 0.0; }
    void seekFile (double seconds);
    juce::String fileName() const { const juce::ScopedLock sl (uiLock); return loadedFile.getFileName(); }
    std::atomic<float> fileProgress { 0.0f };

    // ---- Lo que ve la interfaz (copias bajo uiLock) ----
    dh::lq::Plan planCopy() const { const juce::ScopedLock sl (uiLock); return plan; }
    dh::lq::json::InputSummary inputCopy() const { const juce::ScopedLock sl (uiLock); return input; }
    std::vector<float> outGrooveCopy() const { const juce::ScopedLock sl (uiLock); return outGroove; }
    bool outSpectrum (dh::lq::BandCurve& c) const { const juce::ScopedLock sl (uiLock); c = outMid; return haveOutMid; }
    bool hasPlan() const { const juce::ScopedLock sl (uiLock); return planValid; }
    juce::String statusText() const { const juce::ScopedLock sl (uiLock); return status; }
    bool needsRelearn() const { return stale.load(); }

    dh::lq::LiveMeter inMeter, outMeter;
    std::atomic<float> grHarsh { 0 }, grSib { 0 }, grMb[3] { { 0 }, { 0 }, { 0 } }, grGlue { 0 }, grLim { 0 }, abGainDb { 0 };
    // Espectro de salida para la interfaz: el hilo de audio escribe, la interfaz lee la ventana más reciente.
    void readSpectrumWindow (std::vector<float>& mono) const;
    double getSampleRateNow() const { return sr; }
    std::function<void()> onPlanChanged;       // en el hilo de mensajes

private:
    double sr = 48000.0;
    int maxBlock = 512, latency = 0;

    // Cadena activa / entrante / retirada (traspaso sin bloqueos).
    std::unique_ptr<dh::lq::Chain> active, fading;
    std::atomic<dh::lq::Chain*> pending { nullptr }, retired { nullptr };
    int fadePos = 0, fadeLen = 2400;
    juce::AudioBuffer<float> work, work2, dry;
    std::vector<float> dryDelayL, dryDelayR; int dryPos = 0;
    float abGainNow = 1.0f, previewGainNow = 1.0f;

    // Aprendizaje
    juce::AudioBuffer<float> learnBuf;
    std::atomic<int> learnCount { 0 };
    int silentRun = 0;                         // muestras seguidas de silencio mientras escucha
    juce::CriticalSection learnBufLock;        // el trabajador lee learnBuf; startLearning lo redimensiona
    bool learnAudioValid = false;              // hay audio aprendido en memoria (para re-planificar)
    std::atomic<bool> stale { false };

    // Archivo
    juce::AudioBuffer<float> fileOrig, fileMaster;
    double fileRate = 48000.0; int64_t fileLen = 0;
    double fileReadPos = 0.0;                  // en muestras del archivo (hilo de audio)
    std::atomic<double> seekRequest { -1.0 };
    juce::File loadedFile;
    double fileAbGainDb = 0.0;
    dh::lq::Report fileReport;
    juce::LagrangeInterpolator interpO[2], interpM[2];

    // Plan
    mutable juce::CriticalSection uiLock;
    dh::lq::Plan plan; bool planValid = false;
    dh::lq::json::InputSummary input;
    std::vector<float> outGroove;
    dh::lq::BandCurve outMid {}; bool haveOutMid = false;
    // El modo en vivo mientras hay un archivo abierto: el archivo usa plan/input/... para la interfaz, y al cerrarlo
    // (o al guardar el estado) vale lo del bus master.
    struct LiveSnapshot { dh::lq::Plan plan; bool planValid = false; dh::lq::json::InputSummary input; std::vector<float> outGroove;
                          dh::lq::BandCurve outMid {}; bool haveOutMid = false; float abGainDb = 0; bool valid = false; } live;
    void saveLive(); void restoreLive();
    std::atomic<bool> audioInFile { false };
    juce::CriticalSection fileLock;
    void waitForAudioOutOfFile() const;
    juce::AudioBuffer<float> bypassDelay; int bypassPos = 0;
    struct Cancelled {};
    std::function<void (double)> cancelCheck() { return [this] (double) { if (worker->threadShouldExit()) throw Cancelled(); }; }
    juce::String status { "Press LEARN and play the loudest part of the song, or master a file." };

    // Hilo de trabajo
    struct Job { enum Kind { none, learnLive, replanLive, loadFile, renderFile } kind = none; juce::File file; };
    std::unique_ptr<juce::Thread> worker;
    juce::CriticalSection jobLock; Job nextJob; juce::WaitableEvent jobEvent;
    void postJob (Job j);
    void runWorker();
    void doLearn (bool fromLive);
    void doLoadFile (const juce::File& f);
    void doRenderFile();
    void installPlan (const dh::lq::Plan& p);
    void setStatus (const juce::String& s);

    // Espectro
    std::vector<float> specRing; std::atomic<int> specPos { 0 };

    std::atomic<double> paramsDirtyAt { 0.0 }, fileOutLufs { -70.0 };

    void parameterChanged (const juce::String& id, float v) override;
    void deleteRetired();

    friend class WorkerThread;
    JUCE_DECLARE_WEAK_REFERENCEABLE (LacquerProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LacquerProcessor)
};
