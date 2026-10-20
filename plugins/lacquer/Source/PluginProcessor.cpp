#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/ReportJson.h"

using namespace dh::lq;

namespace {
// Nivel cada 50 ms (dB RMS de L+R), para dibujar el surco del disco.
std::vector<float> grooveOf (const float* L, const float* R, size_t n, double fs)
{
    std::vector<float> g;
    const size_t hop = (size_t) std::max (1.0, 0.05 * fs);
    for (size_t p = 0; p + hop <= n; p += hop)
    {
        double s = 0.0;
        for (size_t i = p; i < p + hop; ++i) s += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]);
        g.push_back ((float) (10.0 * std::log10 (s / (double) hop + 1e-12)));
    }
    return g;
}
} // namespace

class WorkerThread : public juce::Thread
{
public:
    explicit WorkerThread (LacquerProcessor& p) : juce::Thread ("Lacquer worker"), proc (p) {}
    void run() override { proc.runWorker(); }
private:
    LacquerProcessor& proc;
};

LacquerProcessor::LacquerProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LACQUER", params::layout())
{
    for (auto* id : { &params::style, &params::intensity, &params::target, &params::ceiling }) apvts.addParameterListener (*id, this);
    plan.opt.constantLatency = true;
    worker = std::make_unique<WorkerThread> (*this);
    worker->startThread (juce::Thread::Priority::low);
}

LacquerProcessor::~LacquerProcessor()
{
    for (auto* id : { &params::style, &params::intensity, &params::target, &params::ceiling }) apvts.removeParameterListener (*id, this);
    // El análisis y el render miran threadShouldExit y salen enseguida; se espera sin plazo (con plazo, JUCE mata
    // el hilo por la fuerza y deja candados y memoria en cualquier estado).
    worker->signalThreadShouldExit(); jobEvent.signal(); worker->stopThread (-1);
    delete pending.exchange (nullptr);
    delete retired.exchange (nullptr);
}

bool LacquerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

void LacquerProcessor::setValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

Options LacquerProcessor::currentOptions() const
{
    Options o;
    o.style = (Style) juce::jlimit (0, kStyles - 1, (int) value (params::style));
    o.intensity = value (params::intensity) / 100.0;
    const auto& t = loudTargets()[(size_t) juce::jlimit (0, (int) loudTargets().size() - 1, (int) value (params::target))];
    o.targetLufs = t.lufs; o.matchInput = t.matchInput;
    o.ceilingDb = value (params::ceiling);
    o.constantLatency = true;
    return o;
}

void LacquerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = std::max (64, samplesPerBlock);
    Plan p;
    { const juce::ScopedLock sl (uiLock); p = plan; if (! planValid) { p = Plan(); p.opt.constantLatency = true; p.lim.ceilingDb = value (params::ceiling); } }
    p.opt.constantLatency = true;
    delete pending.exchange (nullptr);
    active = std::make_unique<Chain>(); active->prepare (sr, p);
    fading.reset();
    latency = active->latency();
    setLatencySamples (latency);
    work.setSize (2, maxBlock, false, false, true); work2.setSize (2, maxBlock, false, false, true); dry.setSize (2, maxBlock, false, false, true);
    dryDelayL.assign ((size_t) std::max (1, latency), 0.0f); dryDelayR.assign ((size_t) std::max (1, latency), 0.0f); dryPos = 0;
    bypassDelay.setSize (2, std::max (1, latency)); bypassDelay.clear(); bypassPos = 0;
    inMeter.prepare (sr); outMeter.prepare (sr);
    fadeLen = (int) (0.05 * sr); fadePos = 0;
    specRing.assign (8192, 0.0f); specPos.store (0);
    for (auto& i : interpO) i.reset();
    for (auto& i : interpM) i.reset();
}

// ---------------------------------------------------------------------------------------------------------------
void LacquerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n == 0 || nch == 0) return;
    float* outL = buffer.getWritePointer (0);
    float* outR = nch > 1 ? buffer.getWritePointer (1) : nullptr;

    // Cadena nueva: sólo si no hay un fundido en curso ni una retirada sin borrar.
    if (fading == nullptr && retired.load() == nullptr)
        if (auto* c = pending.exchange (nullptr))
        {
            if (std::abs (c->sampleRate() - sr) > 0.5) retired.store (c);
            else { fading = std::move (active); active.reset (c); fadePos = 0; }
        }

    const auto platform = params::platforms()[(size_t) juce::jlimit (0, 3, (int) value (params::preview))];
    const bool compare = value (params::compare) > 0.5f;

    // ---- Modo archivo: la salida es el archivo (original o master), la entrada se silencia ----
    if (fileState.load() == FileState::ready)
    {
        const double seek = seekRequest.exchange (-1.0);
        if (seek >= 0.0) { fileReadPos = juce::jlimit (0.0, (double) fileLen, seek * fileRate); for (auto& i : interpO) i.reset(); for (auto& i : interpM) i.reset(); }
        buffer.clear();
        audioInFile.store (true);                // el trabajador no toca los buffers del archivo mientras tanto
        if (filePlaying.load())
        {
            const double ratio = fileRate / sr;
            // De a trozos del tamaño preparado: un host puede mandar un bloque mayor que el anunciado.
            for (int done = 0; done < n && filePlaying.load();)
            {
                const auto avail = (int) std::max<int64_t> (0, fileLen - (int64_t) fileReadPos);
                if (avail <= 0) break;
                float* tmpO[2] = { work.getWritePointer (0), work.getWritePointer (1) };
                float* tmpM[2] = { work2.getWritePointer (0), work2.getWritePointer (1) };
                const int m = std::min (n - done, work.getNumSamples());
                int used = 0;
                for (int c = 0; c < 2; ++c)
                {
                    used = interpO[c].process (ratio, fileOrig.getReadPointer (c, (int) fileReadPos), tmpO[c], m, avail, 0);
                    interpM[c].process (ratio, fileMaster.getReadPointer (c, (int) fileReadPos), tmpM[c], m, avail, 0);
                }
                fileReadPos += used;
                const float ab = (float) std::pow (10.0, fileAbGainDb / 20.0);
                const double outLufs = fileOutLufs.load();
                const float pv = platform.lufs < 0.0 && outLufs > platform.lufs ? (float) std::pow (10.0, (platform.lufs - outLufs) / 20.0) : 1.0f;
                const float target = compare ? 1.0f : 0.0f;
                for (int i = 0; i < m; ++i)
                {
                    abGainNow += (target - abGainNow) * 0.002f;
                    const float l = (tmpO[0][i] * ab * abGainNow + tmpM[0][i] * (1.0f - abGainNow)) * pv;
                    const float r = (tmpO[1][i] * ab * abGainNow + tmpM[1][i] * (1.0f - abGainNow)) * pv;
                    outL[done + i] = outR ? l : 0.5f * (l + r);
                    if (outR) outR[done + i] = r;
                }
                inMeter.process (tmpO[0], tmpO[1], m);
                done += m;
            }
            if (fileReadPos >= (double) fileLen) { filePlaying.store (false); fileReadPos = 0.0; }
            filePos.store (fileReadPos / fileRate);
        }
        audioInFile.store (false);
        outMeter.process (outL, outR ? outR : outL, n);
        return;
    }

    // ---- En vivo ----
    for (int pos = 0; pos < n;)
    {
        const int m = std::min (n - pos, work.getNumSamples());
        const float* inL = buffer.getReadPointer (0, pos);
        const float* inR = nch > 1 ? buffer.getReadPointer (1, pos) : inL;
        inMeter.process (inL, inR, m);

        // Aprendizaje: guarda los bloques que no son silencio (−60 dBFS RMS).
        // Termina solo cuando la música para (2 s de silencio con al menos 8 s aprendidos) o al llenarse (6 min).
        if (learnState.load() == LearnState::listening)
        {
            double e = 0.0; for (int i = 0; i < m; ++i) e += 0.5 * (inL[i] * inL[i] + inR[i] * inR[i]);
            if (e / m > 1e-6)
            {
                silentRun = 0;
                const int at = learnCount.load(), k = std::min (m, learnBuf.getNumSamples() - at);
                if (k > 0) { learnBuf.copyFrom (0, at, inL, k); learnBuf.copyFrom (1, at, inR, k); learnCount.store (at + k); }
                if (at + k >= learnBuf.getNumSamples()) { learnState.store (LearnState::analysing); jobEvent.signal(); }
            }
            else if ((silentRun += m) > (int) (2.0 * sr) && learnCount.load() >= (int) (kLearnMinSeconds * sr))
            {
                learnState.store (LearnState::analysing); jobEvent.signal();
            }
        }

        work.copyFrom (0, 0, inL, m); work.copyFrom (1, 0, inR, m);
        // Seco demorado lo mismo que la cadena (para la comparación).
        for (int i = 0; i < m; ++i)
        {
            const float l = inL[i], r = inR[i];
            dry.setSample (0, i, dryDelayL[(size_t) dryPos]); dry.setSample (1, i, dryDelayR[(size_t) dryPos]);
            dryDelayL[(size_t) dryPos] = l; dryDelayR[(size_t) dryPos] = r;
            dryPos = (dryPos + 1) % (int) dryDelayL.size();
        }
        if (fading != nullptr) { work2.copyFrom (0, 0, inL, m); work2.copyFrom (1, 0, inR, m); fading->process (work2.getWritePointer (0), work2.getWritePointer (1), m); }
        active->process (work.getWritePointer (0), work.getWritePointer (1), m);
        if (fading != nullptr)
        {
            for (int i = 0; i < m; ++i)
            {
                const float t = juce::jlimit (0.0f, 1.0f, (float) (fadePos + i) / (float) fadeLen);
                for (int c = 0; c < 2; ++c) work.setSample (c, i, work.getSample (c, i) * t + work2.getSample (c, i) * (1.0f - t));
            }
            fadePos += m;
            if (fadePos >= fadeLen) retired.store (fading.release());
        }

        const auto& P = active->plan();
        const bool havePlan = ! P.silent && P.outLufs > -69.0;
        const double inLufs = havePlan ? P.outLufs - P.gainDb : 0.0;
        juce::ignoreUnused (inLufs);
        // Comparación igualada: el original sube o baja lo que el aprendizaje midió de diferencia.
        const float abDb = havePlan ? abGainDb.load() : 0.0f;
        const float ab = std::pow (10.0f, abDb / 20.0f);
        const float pv = havePlan && platform.lufs < 0.0 && P.outLufs > platform.lufs ? (float) std::pow (10.0, (platform.lufs - P.outLufs) / 20.0) : 1.0f;
        const float target = compare ? 1.0f : 0.0f;
        for (int i = 0; i < m; ++i)
        {
            abGainNow += (target - abGainNow) * 0.002f;
            previewGainNow += (pv - previewGainNow) * 0.002f;
            const float l = (dry.getSample (0, i) * ab * abGainNow + work.getSample (0, i) * (1.0f - abGainNow)) * previewGainNow;
            const float r = (dry.getSample (1, i) * ab * abGainNow + work.getSample (1, i) * (1.0f - abGainNow)) * previewGainNow;
            outL[pos + i] = outR ? l : 0.5f * (l + r);
            if (outR) outR[pos + i] = r;
        }
        pos += m;
    }
    outMeter.process (outL, outR ? outR : outL, n);
    grHarsh.store ((float) active->grHarsh()); grSib.store ((float) active->grSib());
    for (int b = 0; b < 3; ++b) grMb[b].store ((float) active->grMb (b));
    grGlue.store ((float) active->grGlue()); grLim.store ((float) active->grLimiter());
    // Espectro (mono de la salida).
    int sp = specPos.load();
    for (int i = 0; i < n; ++i) { specRing[(size_t) sp] = 0.5f * (outL[i] + (outR ? outR[i] : outL[i])); sp = (sp + 1) & 8191; }
    specPos.store (sp);
}

void LacquerProcessor::readSpectrumWindow (std::vector<float>& mono) const
{
    mono.resize (4096);
    const int sp = specPos.load();
    for (int i = 0; i < 4096; ++i) mono[(size_t) i] = specRing.empty() ? 0.0f : specRing[(size_t) ((sp - 4096 + i) & 8191)];
}

// ---------------------------------------------------------------------------------------------------------------
void LacquerProcessor::startLearning()
{
    if (fileState.load() != FileState::none) closeFile();
    {
        // El búfer se reserva al empezar (6 min estéreo), no en prepareToPlay: cada instancia sin usar no ocupa nada.
        const juce::ScopedLock sl (learnBufLock);
        learnState.store (LearnState::idle);
        learnCount.store (0); silentRun = 0;
        learnBuf.setSize (2, (int) (kLearnSeconds * sr), false, false, true);
    }
    learnState.store (LearnState::listening);
    inMeter.requestReset(); outMeter.requestReset();
    setStatus ("Listening. Play the whole song for an exact loudness, or at least its loudest 30 s. Press Stop when done; Lacquer also stops when the music stops.");
}

void LacquerProcessor::stopLearning()
{
    if (learnState.load() != LearnState::listening) return;
    if (learnedSeconds() >= kLearnMinSeconds) { learnState.store (LearnState::analysing); jobEvent.signal(); }
    else
    {
        learnState.store (hasPlan() ? LearnState::ready : LearnState::idle);
        setStatus ("Learning cancelled: it needs at least 8 seconds of music.");
    }
}

void LacquerProcessor::learnFromFile (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) return;
    const int n = (int) std::min<int64_t> (r->lengthInSamples, (int64_t) (kLearnSeconds * sr));
    {
        const juce::ScopedLock sl (learnBufLock);
        learnBuf.setSize (2, std::max (n, 1), false, true, false);
        r->read (&learnBuf, 0, n, 0, true, r->numChannels > 1);
        if (r->numChannels == 1) learnBuf.copyFrom (1, 0, learnBuf, 0, 0, n);
        learnCount.store (n);
    }
    learnState.store (LearnState::analysing);
    jobEvent.signal();
}

void LacquerProcessor::openFile (const juce::File& f)
{
    saveLive();
    learnState.store (hasPlan() ? LearnState::ready : LearnState::idle);
    filePlaying.store (false);
    fileState.store (FileState::loading);
    fileProgress.store (0.0f);
    Job j; j.kind = Job::loadFile; j.file = f; postJob (j);
}

void LacquerProcessor::closeFile()
{
    filePlaying.store (false);
    fileState.store (FileState::none);
    restoreLive();                               // el plan, la entrada y el A/B del modo en vivo, como estaban
    setStatus ("Back to live mode: Lacquer processes what comes into the plugin.");
}

void LacquerProcessor::seekFile (double seconds) { seekRequest.store (juce::jlimit (0.0, fileSeconds(), seconds)); filePos.store (seconds); }

void LacquerProcessor::postJob (Job j)
{
    { const juce::ScopedLock sl (jobLock); nextJob = j; }
    jobEvent.signal();
}

void LacquerProcessor::setStatus (const juce::String& s)
{
    { const juce::ScopedLock sl (uiLock); status = s; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<LacquerProcessor> (this)] { if (safe != nullptr && safe->onPlanChanged) safe->onPlanChanged(); });
}

void LacquerProcessor::deleteRetired() { delete retired.exchange (nullptr); }

void LacquerProcessor::parameterChanged (const juce::String&, float)
{
    // Puede llegar desde el hilo de audio (automatización): sólo marca. El trabajador re-planifica con 300 ms de calma.
    paramsDirtyAt.store (juce::Time::getMillisecondCounterHiRes());
    jobEvent.signal();
}

void LacquerProcessor::runWorker()
{
    while (! worker->threadShouldExit())
    {
        jobEvent.wait (100);
        deleteRetired();
        if (worker->threadShouldExit()) break;
        Job j; { const juce::ScopedLock sl (jobLock); j = nextJob; nextJob = Job(); }
        if (j.kind == Job::loadFile) { doLoadFile (j.file); continue; }
        if (learnState.load() == LearnState::analysing) { doLearn (true); continue; }
        // Mientras LEARN escucha, un cambio de control no re-planifica (leería el audio mientras el hilo de audio lo
        // escribe y cortaría la escucha); se aplica cuando la escucha termina y analiza con las opciones nuevas.
        if (learnState.load() == LearnState::listening) continue;
        const double dirty = paramsDirtyAt.load();
        if (dirty > 0.0 && juce::Time::getMillisecondCounterHiRes() - dirty > 300.0)
        {
            paramsDirtyAt.store (0.0);
            if (fileState.load() == FileState::ready) doRenderFile();
            else if (learnAudioValid) doLearn (false);
            else if (hasPlan())
            {
                // Sin el audio aprendido (proyecto reabierto): objetivo y techo se ajustan; estilo e intensidad piden LEARN.
                Plan p = planCopy();
                const Options o = currentOptions();
                if (o.style != p.opt.style || std::fabs (o.intensity - p.opt.intensity) > 1e-6)
                {
                    stale.store (true);
                    setStatus ("Press LEARN again to hear the new style or intensity (the learned audio is not stored with the project).");
                }
                else if (! o.matchInput)
                {
                    p.gainDb += o.targetLufs - p.targetLufs; p.outLufs += o.targetLufs - p.targetLufs;
                    p.targetLufs = o.targetLufs; p.ceilingDb = o.ceilingDb; p.lim.ceilingDb = o.ceilingDb; p.opt.targetLufs = o.targetLufs; p.opt.ceilingDb = o.ceilingDb;
                    installPlan (p);
                    setStatus ("Target changed without the learned audio: the loudness is approximate. Press LEARN for an exact match.");
                }
            }
        }
    }
}

void LacquerProcessor::installPlan (const Plan& p)
{
    auto* c = new Chain(); Plan q = p; q.opt.constantLatency = true; c->prepare (sr, q);
    delete pending.exchange (c);
    {
        const juce::ScopedLock sl (uiLock); plan = q; planValid = true;
    }
    abGainDb.store ((float) (q.silent ? 0.0 : q.outLufs - input.lufs));
    juce::MessageManager::callAsync ([safe = juce::WeakReference<LacquerProcessor> (this)] { if (safe != nullptr && safe->onPlanChanged) safe->onPlanChanged(); });
}

void LacquerProcessor::doLearn (bool fromLive)
{
    const juce::ScopedLock bl (learnBufLock);
    const int n = learnCount.load();
    if (n < (int) (kLearnMinSeconds * sr * 0.5)) { learnState.store (hasPlan() ? LearnState::ready : LearnState::idle); return; }
    if (fromLive) setStatus ("Analysing what Lacquer heard…");
    Report rep;
    std::vector<float> oL, oR;
    Plan p;
    try { p = Learner::learn (learnBuf.getReadPointer (0), learnBuf.getReadPointer (1), (size_t) n, sr, currentOptions(), &rep, &oL, &oR, cancelCheck()); }
    catch (const Cancelled&) { return; }
    json::InputSummary s;
    s.valid = true; s.lufs = rep.lufsI; s.tp = rep.truePeakDb; s.lra = rep.lra; s.plr = rep.plr; s.seconds = rep.seconds; s.correlation = rep.correlation; s.mid = rep.mid;
    s.groove = grooveOf (learnBuf.getReadPointer (0), learnBuf.getReadPointer (1), (size_t) n, sr);
    {
        const juce::ScopedLock sl (uiLock);
        input = s; outGroove = grooveOf (oL.data(), oR.data(), oL.size(), sr);
        if (! oL.empty()) { outMid = Analyzer::run (oL.data(), oR.data(), oL.size(), sr).mid; haveOutMid = true; }
    }
    learnAudioValid = true; stale.store (false);
    installPlan (p);
    learnState.store (LearnState::ready);
    juce::String msg = "Learned " + juce::String (juce::roundToInt (rep.seconds)) + " s: " + juce::String (rep.lufsI, 1) + " LUFS in, " + juce::String (p.outLufs, 1) + " LUFS out.";
    if (! p.notes.empty()) msg += " " + juce::String (p.notes.front());
    setStatus (msg);
}

void LacquerProcessor::doLoadFile (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples < 1 || r->lengthInSamples > (int64_t) (r->sampleRate * 60 * 30))
    {
        fileState.store (FileState::failed);
        setStatus ("Could not open " + f.getFileName() + " (WAV, AIFF, FLAC, Ogg or MP3, up to 30 minutes).");
        return;
    }
    setStatus ("Reading " + f.getFileName() + "…");
    waitForAudioOutOfFile();
    const int len = (int) r->lengthInSamples;
    fileOrig.setSize (2, len + 4, false, true, false);
    r->read (&fileOrig, 0, len, 0, true, r->numChannels > 1);
    if (r->numChannels == 1) fileOrig.copyFrom (1, 0, fileOrig, 0, 0, len);
    fileRate = r->sampleRate; fileLen = len;
    { const juce::ScopedLock sl (uiLock); loadedFile = f; }
    doRenderFile();
}

void LacquerProcessor::doRenderFile()
{
    if (fileLen <= 0) return;
    fileState.store (FileState::rendering); filePlaying.store (false);
    setStatus ("Mastering " + fileName() + "…");
    Options o = currentOptions(); o.constantLatency = false;
    const size_t n = (size_t) fileLen;
    Report rep;
    Plan p;
    try
    {
        p = Learner::learn (fileOrig.getReadPointer (0), fileOrig.getReadPointer (1), n, fileRate, o, &rep, nullptr, nullptr,
                            [this] (double v) { if (worker->threadShouldExit()) throw Cancelled(); fileProgress.store ((float) (0.9 * v)); });
    }
    catch (const Cancelled&) { return; }
    // Render canónico (el mismo que lacquerrender): cadena completa por bloques, latencia compensada.
    juce::AudioBuffer<float> out (2, (int) n + 4);
    out.clear();
    {
        Chain c; c.prepare (fileRate, p);
        const int lat = c.latency();
        std::vector<float> bl (4096), br (4096);
        for (size_t pos = 0; pos < n + (size_t) lat; pos += 4096)
        {
            if (worker->threadShouldExit()) return;
            const size_t k = std::min ((size_t) 4096, n + (size_t) lat - pos);
            for (size_t i = 0; i < k; ++i) { const size_t s = pos + i; bl[i] = s < n ? fileOrig.getSample (0, (int) s) : 0.0f; br[i] = s < n ? fileOrig.getSample (1, (int) s) : 0.0f; }
            c.process (bl.data(), br.data(), (int) k);
            for (size_t i = 0; i < k; ++i) { const long long s = (long long) (pos + i) - lat; if (s >= 0 && (size_t) s < n) { out.setSample (0, (int) s, bl[i]); out.setSample (1, (int) s, br[i]); } }
            fileProgress.store ((float) (0.9 + 0.1 * (double) pos / (double) (n + (size_t) lat)));
        }
    }
    {
        const juce::ScopedLock fl (fileLock);     // la exportación lee fileMaster
        waitForAudioOutOfFile();
        fileMaster = std::move (out);
    }
    fileReport = rep;
    fileAbGainDb = p.outLufs - rep.lufsI;
    fileOutLufs.store (p.outLufs);
    json::InputSummary s;
    s.valid = true; s.lufs = rep.lufsI; s.tp = rep.truePeakDb; s.lra = rep.lra; s.plr = rep.plr; s.seconds = rep.seconds; s.correlation = rep.correlation; s.mid = rep.mid;
    s.groove = grooveOf (fileOrig.getReadPointer (0), fileOrig.getReadPointer (1), n, fileRate);
    {
        const juce::ScopedLock sl (uiLock);
        input = s; outGroove = grooveOf (fileMaster.getReadPointer (0), fileMaster.getReadPointer (1), n, fileRate);
        plan = p; planValid = true;
        outMid = Analyzer::run (fileMaster.getReadPointer (0), fileMaster.getReadPointer (1), n, fileRate).mid; haveOutMid = true;
    }
    fileProgress.store (1.0f);
    fileState.store (FileState::ready);
    juce::String msg = fileName() + ": " + juce::String (rep.lufsI, 1) + " LUFS in, " + juce::String (p.outLufs, 1) + " LUFS / " + juce::String (p.outTruePeakDb, 1) + " dBTP out.";
    if (! p.notes.empty()) msg += " " + juce::String (p.notes.front());
    setStatus (msg);
}

bool LacquerProcessor::exportMaster (const juce::File& dest, int bits, juce::String& error)
{
    if (fileState.load() != FileState::ready) { error = "No master to export yet."; return false; }
    const juce::ScopedLock fl (fileLock);         // un re-render no reemplaza el master mientras se exporta
    dest.deleteFile();
    std::unique_ptr<juce::OutputStream> os (dest.createOutputStream());
    if (os == nullptr) { error = "Cannot write " + dest.getFullPathName(); return false; }
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (fileRate).withNumChannels (2).withBitsPerSample (bits));
    if (w == nullptr) { error = "Cannot create the WAV writer."; return false; }
    juce::AudioBuffer<float> b (2, (int) fileLen);
    b.copyFrom (0, 0, fileMaster, 0, 0, (int) fileLen); b.copyFrom (1, 0, fileMaster, 1, 0, (int) fileLen);
    if (bits == 16)
    {
        // TPDF ±1 LSB, semilla fija (el mismo archivo da los mismos bytes).
        uint32_t s = 0x9E3779B9u;
        auto rnd = [&] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) ((double) s / 4294967296.0); };
        for (int i = 0; i < (int) fileLen; ++i) for (int c = 0; c < 2; ++c) b.setSample (c, i, b.getSample (c, i) + (rnd() - rnd()) / 32768.0f);
    }
    if (! w->writeFromAudioSampleBuffer (b, 0, (int) fileLen)) { error = "Writing the WAV failed."; return false; }
    w.reset();
    // Informe al lado.
    std::vector<float> L (fileMaster.getReadPointer (0), fileMaster.getReadPointer (0) + fileLen), R (fileMaster.getReadPointer (1), fileMaster.getReadPointer (1) + fileLen);
    OutMeas m = measure (L, R, fileRate);
    const Plan p = planCopy();
    {
        const Report ro = Analyzer::run (L.data(), R.data(), L.size(), fileRate);
        m.distIn = spectralDistance (fileReport.mid, styleDef (p.opt.style).target);
        m.distOut = spectralDistance (ro.mid, styleDef (p.opt.style).target);
    }
    const auto js = reportJson (loadedFile.getFullPathName().toStdString(), dest.getFullPathName().toStdString(), fileReport, p, m, 0, 0.0);
    dest.withFileExtension ("json").replaceWithText (js);
    return true;
}

// Bypass: el seco sale retrasado la latencia informada (el host la sigue compensando).
void LacquerProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples(), L = bypassDelay.getNumSamples();
    if (n == 0 || L < 1) return;
    for (int c = 0; c < std::min (2, buffer.getNumChannels()); ++c)
    {
        auto* x = buffer.getWritePointer (c); auto* d = bypassDelay.getWritePointer (c);
        int p = bypassPos;
        for (int i = 0; i < n; ++i) { const float y = d[p]; d[p] = x[i]; x[i] = y; if (++p >= L) p = 0; }
    }
    bypassPos = (bypassPos + n) % L;
}

void LacquerProcessor::saveLive()
{
    const juce::ScopedLock sl (uiLock);
    if (live.valid) return;                      // ya hay un archivo abierto: lo guardado es lo en vivo
    live = { plan, planValid, input, outGroove, outMid, haveOutMid, abGainDb.load(), true };
}

void LacquerProcessor::restoreLive()
{
    const juce::ScopedLock sl (uiLock);
    if (! live.valid) return;
    plan = live.plan; planValid = live.planValid; input = live.input; outGroove = live.outGroove; outMid = live.outMid; haveOutMid = live.haveOutMid;
    abGainDb.store (live.abGainDb);
    live.valid = false;
}

void LacquerProcessor::waitForAudioOutOfFile() const
{
    for (int i = 0; i < 2000 && audioInFile.load(); ++i) juce::Thread::sleep (1);
}

// ---------------------------------------------------------------------------------------------------------------
void LacquerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree xtra ("LACQUER_PLAN");
    {
        // Con un archivo abierto, lo que se guarda es el modo en vivo (el plan del bus master), no el del archivo.
        const juce::ScopedLock sl (uiLock);
        const bool inFile = live.valid;
        const auto& P = inFile ? live.plan : plan; const bool pv = inFile ? live.planValid : planValid;
        if (pv)
        {
            xtra.setProperty ("plan", juce::JSON::toString (json::toVar (P), true), nullptr);
            xtra.setProperty ("input", juce::JSON::toString (json::toVar (inFile ? live.input : input), true), nullptr);
            const auto& G = inFile ? live.outGroove : outGroove;
            juce::MemoryBlock mb (G.data(), G.size() * sizeof (float));
            xtra.setProperty ("outGroove", mb.toBase64Encoding(), nullptr);
            if (inFile ? live.haveOutMid : haveOutMid) xtra.setProperty ("outMid", juce::JSON::toString (json::arr (inFile ? live.outMid : outMid), true), nullptr);
        }
    }
    state.removeChild (state.getChildWithName ("LACQUER_PLAN"), nullptr);
    state.appendChild (xtra, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void LacquerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    const auto xtra = state.getChildWithName ("LACQUER_PLAN");
    apvts.replaceState (state);
    paramsDirtyAt.store (0.0);
    if (xtra.isValid() && xtra.hasProperty ("plan"))
    {
        const Plan p = json::fromVar (juce::JSON::parse (xtra.getProperty ("plan").toString()));
        {
            const juce::ScopedLock sl (uiLock);
            input = json::summaryFromVar (juce::JSON::parse (xtra.getProperty ("input").toString()));
            juce::MemoryBlock mb; mb.fromBase64Encoding (xtra.getProperty ("outGroove").toString());
            outGroove.resize (mb.getSize() / sizeof (float));
            if (! outGroove.empty()) std::memcpy (outGroove.data(), mb.getData(), outGroove.size() * sizeof (float));
            haveOutMid = xtra.hasProperty ("outMid");
            if (haveOutMid) outMid = json::curve (juce::JSON::parse (xtra.getProperty ("outMid").toString()));
        }
        learnAudioValid = false;
        installPlan (p);
        learnState.store (LearnState::ready);
        setStatus ("Restored the learned master (" + juce::String (p.outLufs, 1) + " LUFS). Press LEARN to analyse again.");
    }
}

juce::AudioProcessorEditor* LacquerProcessor::createEditor() { return new LacquerEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LacquerProcessor(); }
