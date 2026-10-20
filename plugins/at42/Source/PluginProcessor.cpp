#include <set>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

using namespace dh;

AT42Processor::AT42Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Audio In (Ext / Preamp)", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AT42", createLayout())
{
    for (int i = 0; i < kNumP42; ++i) { panelRaw[(size_t) i] = apvts.getRawParameterValue (panelParam42 (i).id); allParamIds.add (panelParam42 (i).id); }
    for (auto& e : extParams42()) { extRaw.push_back (apvts.getRawParameterValue (e.id)); allParamIds.add (e.id); }
    for (auto& a : patchAtomic) a.store (-1);
    for (auto& c : ccMap) c.store (-1);
    midiPlateNote.fill (-1000);
    loadFactoryPresets();
    rescanUserPresets();
    if (! presets.empty()) loadPreset (0);
    { juce::StringArray g; for (auto& e : extParams42()) if (e.global) g.add (e.id); tracker.ignore (g); }   // ajustes de sesión: no "modifican" el preset
}

juce::AudioProcessorValueTreeState::ParameterLayout AT42Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumP42; ++i)
    {
        const auto& pp = panelParam42 (i);
        if (pp.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pp.id, 1 }, pp.name, panelChoices42 (i), (int) pp.def));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pp.id, 1 }, pp.name, juce::NormalisableRange<float> (0.0f, 1.0f), pp.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([i] (float v, int) { return formatPanel42 (i, v); })));
    }
    for (auto& e : extParams42())
    {
        if (e.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { e.id, 1 }, e.name, extChoices42 (e), (int) e.def));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { e.id, 1 }, e.name, juce::NormalisableRange<float> (e.lo, e.hi), e.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([&e] (float v, int) { return formatExt42 (e, v); })));
    }
    return layout;
}

bool AT42Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void AT42Processor::setValue (P42 p, float v)
{
    if (auto* prm = apvts.getParameter (panelParam42 ((int) p).id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void AT42Processor::setExtValue (const juce::String& id, float v)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void AT42Processor::platePress (int plate, float pressure)
{
    const auto w = plateFifo.write (1);
    if (w.blockSize1 > 0) plateBuf[(size_t) w.startIndex1] = { plate, pressure };
    else if (w.blockSize2 > 0) plateBuf[(size_t) w.startIndex2] = { plate, pressure };
}

void AT42Processor::plateRelease (int plate) { platePress (plate, -1.0f); }

void AT42Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;
    currentBlock = std::max (64, samplesPerBlock);
    engine.prepare (sampleRate, currentBlock, kOversampling42[juce::jlimit (0, 2, (int) apvts.getRawParameterValue ("quality")->load())]);
    latency.setNow ((int) std::lround (dh::halfbandChainDelay (engine.oversampling()) * 1.0));      // el retardo de los filtros del sobremuestreo, para que el host lo compense
    for (int i = 0; i < kNumP42; ++i) smoothed[(size_t) i] = panelRaw[(size_t) i]->load();
    outL.assign ((size_t) currentBlock + 64, 0.0f);
    outR.assign ((size_t) currentBlock + 64, 0.0f);
    inCopy.assign ((size_t) (currentBlock + 64) * 2, 0.0f);
}

void AT42Processor::handleMidi (const juce::MidiMessage& m)
{
    const bool droneMap = apvts.getRawParameterValue ("midi_drone")->load() > 0.5f;
    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        if (droneMap && n >= 24 && n <= 29) { midiDrone[(size_t) (n - 23)] = true; return; }
        engine.keys.press (n - 48, m.getFloatVelocity(), n >= 60, -1);
    }
    else if (m.isNoteOff())
    {
        // Se suelta por las dos vías: si "MIDI Drone Keys" cambió con la tecla apretada, el note-off igual la encuentra.
        const int n = m.getNoteNumber();
        if (n >= 24 && n <= 29) midiDrone[(size_t) (n - 23)] = false;
        engine.keys.release (n - 48);
    }
    else if (m.isPitchWheel())
    {
        const float v = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        engine.keys.bend = v * apvts.getRawParameterValue ("bend_range")->load() / 12.0f;
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        if (cc == 120 || cc == 123) { engine.keys.clear(); midiDrone.fill (false); return; }     // antes que un CC aprendido
        // El aprendizaje no toma el pedal ni los mensajes de modo de canal (los hosts los mandan al parar).
        if (learning.load() && cc != 64 && cc < 120) { pendingLearnCc.store (cc); learning.store (false); return; }
        const int target = ccMap[(size_t) cc].load();
        if (target >= 0 && target < allParamIds.size())
        {
            if (auto* prm = apvts.getParameter (allParamIds[target])) prm->setValueNotifyingHost ((float) val / 127.0f);
            return;
        }
        // Rueda = joystick Y: en reposo el joystick queda al centro y subir la rueda lo sube (Y OUT positivo). Antes,
        // CC1 en 0 lo mandaba al tope (+5 V) y subir la rueda lo bajaba.
        if (cc == 1) setValue (P42::joy_y, 0.5f - 0.5f * (float) val / 127.0f);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { engine.keys.clear(); midiDrone.fill (false); }
}

void AT42Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    wasBypassed = false;
    const int wantOs = kOversampling42[juce::jlimit (0, 2, (int) apvts.getRawParameterValue ("quality")->load())];
    { if (wantOs != engine.oversampling()) engine.prepare (currentSr, currentBlock, wantOs); latency.request ((int) std::lround (dh::halfbandChainDelay (engine.oversampling()) * 1.0)); }
    if ((int) outL.size() < n) { outL.resize ((size_t) n); outR.resize ((size_t) n); }
    if (! midi.isEmpty()) midiInFlash.store (true);

    if (const int cc = pendingLearnCc.exchange (-1); cc >= 0)
    {
        const int idx = learnIndex.load();
        if (idx >= 0 && idx < allParamIds.size()) { for (auto& c : ccMap) if (c.load() == idx) c.store (-1); ccMap[(size_t) cc].store (idx); }
    }

    auto get = [this] (const char* id) {
        const auto& ps = extParams42();
        for (size_t k = 0; k < ps.size(); ++k) if (std::strcmp (ps[k].id, id) == 0) return extRaw[k]->load();
        return 0.0f;
    };
    applyExt42 (get, engine.st, engine.keys);
    for (int j = 0; j < jk42::kNumJacks; ++j) engine.patch.src[(size_t) j] = patchAtomic[(size_t) j].load (std::memory_order_relaxed);
    for (int i = 0; i < 4; ++i) engine.shade[(size_t) i] = orbShade[(size_t) i].load();
    engine.joyX = value (P42::joy_x) * 2.0f - 1.0f;
    engine.joyY = 1.0f - value (P42::joy_y) * 2.0f;

    // Comandos y placas del editor.
    if (const int c = pendingCmd.exchange (0); c == CmdSeqRec)
    {
        engine.keys.recording = ! engine.keys.recording;
        if (engine.keys.recording) { engine.keys.seqCount = 0; engine.keys.seqNotes.fill (-1); }
    }
    else if (c == CmdPanic) { engine.keys.clear(); midiDrone.fill (false); }
    if (const int sc = pendingSeqCount.exchange (-1); sc >= 0)
    {
        engine.keys.seqCount = sc;
        for (int i = 0; i < 16; ++i) engine.keys.seqNotes[(size_t) i] = pendingSeq[(size_t) i];
    }
    {
        const auto r = plateFifo.read (plateFifo.getNumReady());
        auto run = [this] (std::pair<int, float> e) {
            const int plate = e.first;
            const int note = engine.keys.plateNote (plate);
            if (e.second >= 0.0f) { engine.keys.press (note, e.second, plate >= 6, plate); midiPlateNote[(size_t) plate] = note; }
            else { engine.keys.release (midiPlateNote[(size_t) plate] > -1000 ? midiPlateNote[(size_t) plate] : note); midiPlateNote[(size_t) plate] = -1000; }
        };
        for (int i = 0; i < r.blockSize1; ++i) run (plateBuf[(size_t) (r.startIndex1 + i)]);
        for (int i = 0; i < r.blockSize2; ++i) run (plateBuf[(size_t) (r.startIndex2 + i)]);
    }

    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    // Parado, la posición sigue desde donde quedó el host (antes saltaba a una vieja y los LFO sincronizados pegaban un salto).
    if (! playing) { ppq = freePpq; freePpq += n * bpm / 60.0 / currentSr; }
    else freePpq = ppq + n * bpm / 60.0 / currentSr;
    engine.st.bpm = bpm;
    const double ppqPerSample = bpm / 60.0 / currentSr;

    // Entrada de audio (canal EXT IN del mezclador y preamp).
    const float* inL = nullptr; const float* inR = nullptr;
    if (getBusCount (true) > 0 && getBus (true, 0)->isEnabled())
    {
        auto in = getBusBuffer (buffer, true, 0);
        if (in.getNumChannels() > 0) { inL = in.getReadPointer (0); inR = in.getNumChannels() > 1 ? in.getReadPointer (1) : inL; }
    }
    // Copia la entrada antes de escribir la salida (pueden compartir el buffer).
    std::vector<float>& tmpIn = inCopy; tmpIn.resize ((size_t) n * 2);
    if (inL != nullptr)
    {
        std::copy (inL, inL + n, tmpIn.begin()); std::copy (inR, inR + n, tmpIn.begin() + n);
        for (auto& x : tmpIn) if (! dh::isFiniteF (x)) x = 0.0f;     // un NaN en la entrada dejaba muerto el seguidor de envolvente
    }

    auto it = midi.cbegin();
    int pos = 0;
    while (pos < n)
    {
        int end = std::min (n, pos + 32);
        while (it != midi.cend() && (*it).samplePosition <= pos) { handleMidi ((*it).getMessage()); ++it; }
        if (it != midi.cend()) end = std::min (end, std::max (pos + 1, (*it).samplePosition));
        for (int k = 1; k <= 6; ++k) engine.droneKey[(size_t) k] = droneKeyUi[(size_t) k].load() || midiDrone[(size_t) k];
        for (int i = 0; i < kNumP42; ++i)
        {
            const float target = panelRaw[(size_t) i]->load();
            if (panelParam42 (i).steps > 0) smoothed[(size_t) i] = target;
            else smoothed[(size_t) i] += (target - smoothed[(size_t) i]) * 0.25f;
            if (! dh::isFiniteF (smoothed[(size_t) i])) smoothed[(size_t) i] = dh::isFiniteF (target) ? target : 0.0f;
        }
        engine.p = smoothed;
        engine.st.ppq = ppq + pos * ppqPerSample;
        engine.render (outL.data() + pos, outR.data() + pos,
                       inL != nullptr ? tmpIn.data() + pos : nullptr, inL != nullptr ? tmpIn.data() + n + pos : nullptr, end - pos);
        pos = end;
    }
    while (it != midi.cend()) { handleMidi ((*it).getMessage()); ++it; }
    midi.clear();

    // Guardia: si algo llegó a NaN/Inf, silencio y estados reiniciados (en vez de quedar trabado hasta recargar).
    {
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) sum += outL[(size_t) i] + outR[(size_t) i];
        if (! dh::isFiniteF (sum))
        {
            std::fill (outL.begin(), outL.begin() + n, 0.0f); std::fill (outR.begin(), outR.begin() + n, 0.0f);
            engine.prepare (currentSr, currentBlock, engine.oversampling());
        }
    }

    auto out = getBusBuffer (buffer, false, 0);
    if (out.getNumChannels() >= 2) { out.copyFrom (0, 0, outL.data(), n); out.copyFrom (1, 0, outR.data(), n); }
    else if (out.getNumChannels() == 1) { out.copyFrom (0, 0, outL.data(), n, 0.5f); out.addFrom (0, 0, outR.data(), n, 0.5f); }
    for (int c = out.getNumChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    // Estado visible.
    auto& v = engine.view;
    for (int k = 0; k < 7; ++k)
    {
        live.droneEnv[(size_t) k].store (v.droneEnv[(size_t) k]);
        live.orbLight[(size_t) k].store (v.orbLight[(size_t) k]);
        live.orbLed[(size_t) k].store (v.orbLed[(size_t) k]);
        if (v.shFlash[(size_t) k]) live.shFlash[(size_t) k].store (true);
        v.shFlash[(size_t) k] = false;
        live.droneKeyDown[(size_t) k].store (engine.droneKey[(size_t) k]);
    }
    live.egA.store (v.egA); live.egB.store (v.egB); live.lfoA.store (v.lfoA); live.lfoB.store (v.lfoB);
    live.efEnv.store (v.efEnv); live.efGate.store (v.efGate);
    if (v.preClip) live.preClip.store (true);
    v.preClip = false;
    live.seqStep.store (v.seqStep); live.seqGate.store (v.seqGate);
    live.pressure.store (engine.keys.pressure);
    live.recording.store (engine.keys.recording);
    live.kbStep.store (engine.keys.seqPos);
    live.kbSeqCount.store (engine.keys.seqCount);
    for (int i = 0; i < 12; ++i) live.plateDown[(size_t) i].store (midiPlateNote[(size_t) i] > -1000);

    const double used = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    cpuLoad.store (cpuLoad.load() * 0.9f + (float) (used / (n / currentSr)) * 0.1f);
}

// En bypass: silencio; al entrar se sueltan las teclas (el note-off puede llegar durante el bypass).
void AT42Processor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    if (! wasBypassed) { engine.keys.clear(); midiDrone.fill (false); wasBypassed = true; }
    midi.clear();
}

void AT42Processor::notifyUi (std::function<void()>& cb)
{
    if (! cb) return;
    if (juce::MessageManager::existsAndIsCurrentThread()) { cb(); return; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<AT42Processor> (this), &cb] { if (safe != nullptr && cb) cb(); });
}

void AT42Processor::setCables (std::vector<Cable42> c)
{
    { const juce::SpinLock::ScopedLockType l (cableLock); cables = std::move (c); }
    const auto ps = patchFromCables42 (getCables());
    for (int j = 0; j < jk42::kNumJacks; ++j) patchAtomic[(size_t) j].store (ps.src[(size_t) j]);
    notifyUi (onCablesChanged);
}

// ───────────── MIDI learn ─────────────
void AT42Processor::clearMidiMapping (const juce::String& paramId)
{
    const int idx = allParamIds.indexOf (paramId);
    for (auto& c : ccMap) if (c.load() == idx) c.store (-1);
}

int AT42Processor::ccForParam (const juce::String& paramId) const
{
    const int idx = allParamIds.indexOf (paramId);
    for (int c = 0; c < 128; ++c) if (idx >= 0 && ccMap[(size_t) c].load() == idx) return c;
    return -1;
}

// ───────────── Presets ─────────────
void AT42Processor::loadFactoryPresets()
{
   #if AT42_HAS_ASSETS
    juce::StringArray names;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String nm (BinaryData::namedResourceList[i]);
        if (nm.endsWith ("_json") && nm.trimCharactersAtStart ("_").substring (0, 2).containsOnly ("0123456789")) names.add (nm);
    }
    names.sort (true);
    for (auto& nm : names)
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource (nm.toRawUTF8(), size);
        const auto v = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        presets.push_back ({ v.getProperty ("name", nm).toString(), v.getProperty ("category", "").toString(), v, true, {} });
    }
   #endif
}

juce::File AT42Processor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/AT42/Presets");
   #else
    return base.getChildFile ("Audio Tools/AT42/Presets");
   #endif
}

void AT42Processor::rescanUserPresets()
{
    presets.erase (std::remove_if (presets.begin(), presets.end(), [] (auto& p) { return ! p.factory; }), presets.end());
    const auto dir = userPresetDir();
    if (! dir.isDirectory()) return;
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    for (auto& f : files)
    {
        const auto v = juce::JSON::parse (f);
        if (v.isObject()) presets.push_back ({ v.getProperty ("name", f.getFileNameWithoutExtension()).toString(), "User", v, false, f });
    }
}

int AT42Processor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void AT42Processor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String AT42Processor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void AT42Processor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    loadPresetData (presetFromJson42 (presets[(size_t) index].data));
}

void AT42Processor::loadPresetData (const Preset42& p)
{
    for (int i = 0; i < kNumP42; ++i) setValue ((P42) i, p.panel[(size_t) i]);
    for (auto& e : extParams42())
    {
        if (e.global) continue;
        setExtValue (e.id, p.settings.contains (e.id) ? (float) p.settings[e.id] : e.def);
    }
    for (int i = 0; i < 16; ++i) pendingSeq[(size_t) i] = p.keySeq[(size_t) i];
    pendingSeqCount.store (p.keySeqCount);
    currentPresetName = p.name;
    tracker.capture (*this);
    setCables (p.cables);
    notifyUi (onPresetChanged);
}

Preset42 AT42Processor::currentAsPreset (const juce::String& name, const juce::String& category) const
{
    Preset42 p;
    p.name = name; p.category = category;
    for (int i = 0; i < kNumP42; ++i) p.panel[(size_t) i] = panelRaw[(size_t) i]->load();
    const auto& ps = extParams42();
    for (size_t k = 0; k < ps.size(); ++k)
        if (! ps[k].global && std::abs (extRaw[k]->load() - ps[k].def) > 1e-4f) p.settings.set (ps[k].id, extRaw[k]->load());
    p.cables = getCables();                      // bajo el candado: el host puede guardar mientras se patchea
    // Si hay una secuencia restaurada que el audio todavía no tomó (setState sin procesar), es esa la vigente.
    if (const int pend = pendingSeqCount.load(); pend >= 0)
    {
        p.keySeqCount = pend;
        for (int i = 0; i < 16; ++i) p.keySeq[(size_t) i] = pendingSeq[(size_t) i];
    }
    else
    {
        p.keySeqCount = live.kbSeqCount.load();
        for (int i = 0; i < 16; ++i) p.keySeq[(size_t) i] = engine.keys.seqNotes[(size_t) i];
    }
    return p;
}

bool AT42Processor::saveUserPreset (const juce::String& name)
{
    const auto dir = userPresetDir();
    dir.createDirectory();
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".json");
    const bool ok = file.replaceWithText (juce::JSON::toString (presetToJson42 (currentAsPreset (name, "User"))));
    rescanUserPresets();
    currentPresetName = name;
    tracker.capture (*this);
    for (int i = 0; i < (int) presets.size(); ++i) if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) currentProgram = i;
    notifyUi (onPresetChanged);
    return ok;
}

// Patch al azar: afina los drones en acordes, mueve el carácter y los efectos; deja cables y mezcla.
void AT42Processor::randomizePanel()
{
    juce::Random r;
    Preset42 p = currentAsPreset ("Random", "");
    auto set = [&] (const char* id, float v) { if (const int i = panelIndex42 (id); i >= 0) p.panel[(size_t) i] = v; };
    for (int v : { 1, 2, 4, 5 })
        for (int o = 1; o <= 5; ++o)
        {
            set (juce::String ("d" + juce::String (v) + "_tune" + juce::String (o)).toRawUTF8(), r.nextFloat());
            set (juce::String ("d" + juce::String (v) + "_mute" + juce::String (o)).toRawUTF8(), r.nextFloat() < 0.3f ? 1.0f : 0.0f);
            set (juce::String ("d" + juce::String (v) + "_mod" + juce::String (o)).toRawUTF8(), r.nextFloat() < 0.25f ? 1.0f : 0.0f);
        }
    for (const char* id : { "d1_volt", "d2_volt", "d4_volt", "d5_volt" }) set (id, r.nextFloat() * 0.7f);
    for (const char* id : { "d3_rate", "d3_fmamt", "d3_div", "d3_pitch", "d6_rate", "d6_fmamt", "d6_div", "d6_pitch" }) set (id, r.nextFloat());
    set ("vcoa_wave", (float) r.nextInt (6)); set ("vcob_wave", (float) r.nextInt (6));
    set ("flt_freq_l", 0.45f + r.nextFloat() * 0.5f); set ("flt_freq_r", 0.45f + r.nextFloat() * 0.5f);
    set ("flt_res_l", r.nextFloat() * 0.7f); set ("flt_res_r", r.nextFloat() * 0.7f);
    set ("fx_cart_l", (float) r.nextInt (13)); set ("fx_cart_r", (float) r.nextInt (13));
    set ("fx_prog_l", (float) r.nextInt (3)); set ("fx_prog_r", (float) r.nextInt (3));
    set ("fx_x", r.nextFloat()); set ("fx_y", r.nextFloat()); set ("fx_z", r.nextFloat()); set ("fx_blend", 0.2f + r.nextFloat() * 0.6f);
    set ("lfoa_rate", r.nextFloat()); set ("lfob_rate", r.nextFloat());
    loadPresetData (p);
}

// ───────────── Estado ─────────────
void AT42Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const auto pr = presetToJson42 (currentAsPreset (currentPresetName, ""));
    state.setProperty ("presetName", currentPresetName.get(), nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("cables", juce::JSON::toString (pr.getProperty ("cables", {}), true), nullptr);
    state.setProperty ("keyseq", juce::JSON::toString (pr.getProperty ("keyseq", {}), true), nullptr);
    juce::StringArray map;
    for (int c = 0; c < 128; ++c) if (const int t = ccMap[(size_t) c].load(); t >= 0) map.add (juce::String (c) + "=" + allParamIds[t]);
    state.setProperty ("midiMap", map.joinIntoString (","), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AT42Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    auto* o = new juce::DynamicObject();
    o->setProperty ("cables", juce::JSON::parse (state.getProperty ("cables", "[]").toString()));
    o->setProperty ("keyseq", juce::JSON::parse (state.getProperty ("keyseq", "[]").toString()));
    const auto extra = presetFromJson42 (juce::var (o));
    for (auto& c : ccMap) c.store (-1);
    for (auto& kv : juce::StringArray::fromTokens (state.getProperty ("midiMap", "").toString(), ",", ""))
    {
        const int cc = kv.upToFirstOccurrenceOf ("=", false, false).getIntValue();
        const int idx = allParamIds.indexOf (kv.fromFirstOccurrenceOf ("=", false, false));
        if (idx >= 0 && juce::isPositiveAndBelow (cc, 128)) ccMap[(size_t) cc].store (idx);
    }
    for (auto id : { "presetName", "cables", "keyseq", "midiMap" }) state.removeProperty (id, nullptr);
    std::set<juce::String> present;
    for (const auto& c : state) if (c.hasProperty ("id")) present.insert (c.getProperty ("id").toString());
    apvts.replaceState (state);
    // Parámetros que faltan (sesiones viejas) o llegaron como NaN: a su valor por defecto.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! dh::isFiniteF (rp->getValue()) || present.count (rp->paramID) == 0) rp->setValueNotifyingHost (rp->getDefaultValue());
    for (int i = 0; i < 16; ++i) pendingSeq[(size_t) i] = extra.keySeq[(size_t) i];
    pendingSeqCount.store (extra.keySeqCount);
    setCables (extra.cables);
    currentProgram = juce::jlimit (0, std::max (0, (int) presets.size() - 1), (int) state.getProperty ("program", 0));
    if (state.hasProperty ("presetSnapshot")) tracker.fromString (*this, state.getProperty ("presetSnapshot").toString());
    else tracker.capture (*this);           // sesión anterior a la marca: el nombre sin "*"
    notifyUi (onPresetChanged);
}

juce::AudioProcessorEditor* AT42Processor::createEditor() { return new AT42Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AT42Processor(); }
