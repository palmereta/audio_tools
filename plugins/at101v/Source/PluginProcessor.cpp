#include <set>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

using namespace dh;

AT101VProcessor::AT101VProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AT101V", createLayout())
{
    for (int i = 0; i < kNumP; ++i)
    {
        panelRaw[(size_t) i] = apvts.getRawParameterValue (panelParam (i).id);
        allParamIds.add (panelParam (i).id);
    }
    for (auto& e : extParams()) { extRaw.push_back (apvts.getRawParameterValue (e.id)); allParamIds.add (e.id); }
    pQuality = apvts.getRawParameterValue ("quality");
    pMidiOut = apvts.getRawParameterValue ("midi_out");
    for (auto& c : ccMap) c.store (-1);
    loadFactoryPresets();
    rescanUserPresets();
    if (! presets.empty()) loadPreset (0);
    { juce::StringArray g; for (auto& e : extParams()) if (e.global) g.add (e.id); tracker.ignore (g); }   // ajustes de sesión: no "modifican" el preset
}

juce::AudioProcessorValueTreeState::ParameterLayout AT101VProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumP; ++i)
    {
        const auto& pp = panelParam (i);
        if (pp.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pp.id, 1 }, panelName (i), panelChoices (i), (int) pp.def));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { pp.id, 1 }, panelName (i), juce::NormalisableRange<float> (0.0f, 1.0f), pp.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([i] (float v, int) { return formatPanel (i, v); })));
    }
    for (auto& e : extParams())
    {
        if (e.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { e.id, 1 }, e.name, extChoices (e), (int) e.def));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { e.id, 1 }, e.name, juce::NormalisableRange<float> (e.lo, e.hi), e.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([&e] (float v, int) { return formatExt (e, v); })));
    }
    return layout;
}

bool AT101VProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AT101VProcessor::setPanelValue (P p, float v)
{
    if (auto* prm = apvts.getParameter (panelParam ((int) p).id))
    {
        gesture.touch (prm);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }
}

void AT101VProcessor::setExtValue (const juce::String& id, float v)
{
    if (auto* prm = apvts.getParameter (id)) { gesture.touch (prm); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
}

void AT101VProcessor::sendCommand (Command c)
{
    const auto w = cmdFifo.write (1);
    if (w.blockSize1 > 0) cmdBuf[(size_t) w.startIndex1] = (int) c;
    else if (w.blockSize2 > 0) cmdBuf[(size_t) w.startIndex2] = (int) c;
}

Sequence AT101VProcessor::getSequence() const
{
    const juce::SpinLock::ScopedLockType sl (seqLock);
    return sharedSeq;
}

void AT101VProcessor::editSequence (const std::function<void (Sequence&)>& fn)
{
    {
        const juce::SpinLock::ScopedLockType sl (seqLock);
        fn (sharedSeq);
        sharedSeq.length = juce::jlimit (0, Sequence::kMaxSteps, sharedSeq.length);
    }
    ++seqVersion;
}

void AT101VProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;
    currentBlock = std::max (samplesPerBlock, 64);
    engine.prepare (sampleRate, currentBlock, kOversampling[juce::jlimit (0, 2, (int) pQuality->load())]);
    latency.setNow ((int) std::lround (dh::halfbandChainDelay (engine.oversampling()) * 1.0));      // el retardo de los filtros del sobremuestreo, para que el host lo compense
    for (int i = 0; i < kNumP; ++i) smoothed[(size_t) i] = panelRaw[(size_t) i]->load();
    outL.assign ((size_t) currentBlock + 64, 0.0f);
    outR.assign ((size_t) currentBlock + 64, 0.0f);
    engineSeqVersion = -1;
}

void AT101VProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) engine.keyDown (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine.keyUp (m.getNoteNumber());
    else if (m.isPitchWheel())
        bender.store (juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8192.0f));
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        // El pánico va antes que un CC aprendido, y el aprendizaje no toma el pedal ni los mensajes de modo de canal
        // (los hosts mandan CC121/123 al parar: aprendidos, cada Stop movía el parámetro y dejaba notas colgadas).
        if (cc == 120 || cc == 123) { engine.allNotesOff(); return; }
        if (learning.load() && cc != 64 && cc < 120) { pendingLearnCc.store (cc); learning.store (false); return; }
        const int target = ccMap[(size_t) cc].load();
        if (target >= 0 && target < allParamIds.size())
        {
            if (auto* prm = apvts.getParameter (allParamIds[target])) prm->setValueNotifyingHost ((float) val / 127.0f);
            return;
        }
        if (cc == 1) lfoPush.store ((float) val / 127.0f);          // rueda = palanca empujada (LFO MOD)
        else if (cc == 64) holdPedal.store (val >= 64);              // pedal = HOLD
        else if (cc == 120 || cc == 123) engine.allNotesOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

void AT101VProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    wasBypassed = false;
    engine.numMidiEvents = 0;
    engine.curSample = 0;

    const int wantOs = kOversampling[juce::jlimit (0, 2, (int) pQuality->load())];
    { if (wantOs != engine.oversampling()) engine.prepare (currentSr, currentBlock, wantOs); latency.request ((int) std::lround (dh::halfbandChainDelay (engine.oversampling()) * 1.0)); }
    engine.flushMidiIfPending();      // notas del MIDI out que quedaron abiertas al re-preparar
    if (const int kt = pendingKeyTranspose.exchange (1000); kt != 1000) engine.keyTranspose = kt;
    if ((int) outL.size() < n) { outL.resize ((size_t) n); outR.resize ((size_t) n); }

    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (! midi.isEmpty()) midiInFlash.store (true);

    // MIDI learn: el CC capturado en el bloque anterior se asigna acá (fuera del bucle de eventos).
    if (const int cc = pendingLearnCc.exchange (-1); cc >= 0)
    {
        const int idx = learnIndex.load();
        if (idx >= 0 && idx < allParamIds.size())
        {
            for (auto& c : ccMap) if (c.load() == idx) c.store (-1);
            ccMap[(size_t) cc].store (idx);
        }
    }

    // Ajustes.
    auto get = [this] (const char* id) {
        const auto& ps = extParams();
        for (size_t k = 0; k < ps.size(); ++k) if (std::strcmp (ps[k].id, id) == 0) return extRaw[k]->load();
        return 0.0f;
    };
    applyExtParams (get, engine.st, engine.fx);

    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    if (! playing) { ppq = freePpq; freePpq += n * bpm / 60.0 / currentSr; }
    engine.st.bpm = bpm; engine.st.hostPlaying = playing;
    const double ppqPerSample = bpm / 60.0 / currentSr;

    // Secuencia editada en la UI → motor.
    if (seqVersion.load() != engineSeqVersion)
    {
        const juce::SpinLock::ScopedTryLockType tl (seqLock);
        if (tl.isLocked()) { engine.seq = sharedSeq; engineSeqVersion = seqVersion.load(); }
    }

    // Comandos de los botones.
    {
        const auto r = cmdFifo.read (cmdFifo.getNumReady());
        auto run = [this] (int c) {
            switch (c)
            {
                case CmdLoadToggle: engine.setLoading (! engine.loading); break;
                case CmdRest: engine.addRest(); break;
                case CmdTie: engine.markTie(); break;
                case CmdKeyTranspose: engine.armKeyTranspose(); break;
                case CmdKeyTransposeReset: engine.keyTranspose = 0; engine.keyTrArmed = false; break;
                case CmdPanic: engine.allNotesOff(); break;
                default: break;
            }
        };
        for (int i = 0; i < r.blockSize1; ++i) run (cmdBuf[(size_t) (r.startIndex1 + i)]);
        for (int i = 0; i < r.blockSize2; ++i) run (cmdBuf[(size_t) (r.startIndex2 + i)]);
    }

    const int midiMode = juce::jlimit (0, 2, (int) pMidiOut->load());
    juce::MidiBuffer outMidi;
    forwardThru (midi, outMidi, n, midiMode == 1);
    if (midiMode != 2 && synthOn.any())     // se salió de Synth Notes con notas abiertas: se cierran
    {
        for (int k = 0; k < 128; ++k) if (synthOn[(size_t) k]) outMidi.addEvent (juce::MidiMessage::noteOff (1, k), 0);
        synthOn.reset();
    }

    auto it = midi.cbegin();
    int pos = 0;
    while (pos < n)
    {
        int end = std::min (n, pos + 64);
        engine.curSample = 0;
        while (it != midi.cend() && (*it).samplePosition <= pos) { handleMidi ((*it).getMessage()); ++it; }
        if (it != midi.cend()) end = std::min (end, std::max (pos + 1, (*it).samplePosition));
        engine.bender = bender.load();      // por tramo: el pitch bend y la rueda actúan en su muestra, no un bloque tarde
        engine.lfoPush = lfoPush.load();

        for (int i = 0; i < kNumP; ++i)
        {
            const float target = panelRaw[(size_t) i]->load();
            if (panelParam (i).steps > 0) smoothed[(size_t) i] = target;
            else smoothed[(size_t) i] += (target - smoothed[(size_t) i]) * 0.3f;
            if (! dh::isFiniteF (smoothed[(size_t) i])) smoothed[(size_t) i] = dh::isFiniteF (target) ? target : 0.0f;
        }
        if (holdPedal.load()) smoothed[(size_t) P::hold] = 1.0f;
        engine.p = smoothed;
        engine.st.ppq = ppq + pos * ppqPerSample;
        engine.render (outL.data() + pos, outR.data() + pos, end - pos);

        for (int e = 0; e < engine.numMidiEvents; ++e)
        {
            const auto& ev = engine.midiEvents[(size_t) e];
            const int at = juce::jlimit (0, n - 1, pos + ev.pos);
            if (ev.on && midiMode == 2) { outMidi.addEvent (juce::MidiMessage::noteOn (1, ev.note, (juce::uint8) juce::jlimit (1, 127, ev.vel)), at); synthOn.set ((size_t) ev.note); }
            else if (! ev.on && synthOn[(size_t) ev.note]) { outMidi.addEvent (juce::MidiMessage::noteOff (1, ev.note), at); synthOn.reset ((size_t) ev.note); }
        }
        engine.numMidiEvents = 0;
        pos = end;
    }
    while (it != midi.cend()) { handleMidi ((*it).getMessage()); ++it; }
    midi.swapWith (outMidi);
    if (! midi.isEmpty()) midiOutFlash.store (true);

    // Carga desde el teclado → copia compartida.
    if (engine.seqChanged)
    {
        const juce::SpinLock::ScopedTryLockType tl (seqLock);
        if (tl.isLocked())
        {
            sharedSeq = engine.seq;
            engine.seqChanged = false;
            engineSeqVersion = ++seqVersion;
        }
    }

    auto out = getBusBuffer (buffer, false, 0);
    const float g = powerOn.load() ? 1.0f : 0.0f;
    if (out.getNumChannels() >= 2) { out.copyFrom (0, 0, outL.data(), n, g); out.copyFrom (1, 0, outR.data(), n, g); }
    else if (out.getNumChannels() == 1) { out.copyFrom (0, 0, outL.data(), n, 0.5f * g); out.addFrom (0, 0, outR.data(), n, 0.5f * g); }
    for (int c = out.getNumChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    rateLed.store (engine.rateLed);
    loadingLed.store (engine.loading);
    keyTrArmed.store (engine.keyTrArmed);
    keyTranspose.store (engine.keyTranspose);
    stepPos.store (engine.seqPos);
    soundingNote.store (engine.soundingNote);
    activeVoices.store (engine.activeVoiceCount());

    const double used = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    cpuLoad.store (cpuLoad.load() * 0.9f + (float) (used / (n / currentSr)) * 0.1f);
}

// En bypass: silencio; al entrar se sueltan las voces y se cierran las notas del MIDI out (el note-off de la
// entrada puede llegar durante el bypass). En Thru, el MIDI de entrada sigue pasando.
void AT101VProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    const int n = std::max (1, buffer.getNumSamples());
    juce::MidiBuffer outMidi;
    const int midiMode = juce::jlimit (0, 2, (int) pMidiOut->load());
    if (! wasBypassed)
    {
        engine.numMidiEvents = 0; engine.curSample = 0;
        engine.allNotesOff();
        engine.numMidiEvents = 0;
        for (int k = 0; k < 128; ++k) if (synthOn[(size_t) k]) outMidi.addEvent (juce::MidiMessage::noteOff (1, k), 0);
        synthOn.reset();
        wasBypassed = true;
    }
    forwardThru (midi, outMidi, n, midiMode == 1);
    midi.swapWith (outMidi);
}

void AT101VProcessor::forwardThru (const juce::MidiBuffer& in, juce::MidiBuffer& out, int n, bool thru)
{
    if (! thru)
    {
        for (int ch = 0; ch < 16; ++ch)
            if (thruOn[(size_t) ch].any())
            {
                for (int k = 0; k < 128; ++k) if (thruOn[(size_t) ch][(size_t) k]) out.addEvent (juce::MidiMessage::noteOff (ch + 1, k), 0);
                thruOn[(size_t) ch].reset();
            }
        return;
    }
    for (const auto md : in)
    {
        const auto m = md.getMessage();
        out.addEvent (m, juce::jlimit (0, n - 1, md.samplePosition));
        const int ch = juce::jlimit (1, 16, m.getChannel()) - 1;
        if (m.isNoteOn()) thruOn[(size_t) ch].set ((size_t) m.getNoteNumber());
        else if (m.isNoteOff()) thruOn[(size_t) ch].reset ((size_t) m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) thruOn[(size_t) ch].reset();
    }
}

void AT101VProcessor::notifyUi (std::function<void()>& cb)
{
    if (! cb) return;
    if (juce::MessageManager::existsAndIsCurrentThread()) { cb(); return; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<AT101VProcessor> (this), &cb] {
        if (safe != nullptr && cb) cb();
    });
}

// ───────────── MIDI learn ─────────────
void AT101VProcessor::clearMidiMapping (const juce::String& paramId)
{
    const int idx = allParamIds.indexOf (paramId);
    for (auto& c : ccMap) if (c.load() == idx) c.store (-1);
}

int AT101VProcessor::ccForParam (const juce::String& paramId) const
{
    const int idx = allParamIds.indexOf (paramId);
    for (int c = 0; c < 128; ++c) if (idx >= 0 && ccMap[(size_t) c].load() == idx) return c;
    return -1;
}

// ───────────── Presets ─────────────
void AT101VProcessor::loadFactoryPresets()
{
   #if AT101V_HAS_ASSETS
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

juce::File AT101VProcessor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/AT101V/Presets");
   #else
    return base.getChildFile ("Audio Tools/AT101V/Presets");
   #endif
}

void AT101VProcessor::rescanUserPresets()
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

int AT101VProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void AT101VProcessor::setCurrentProgram (int index) { loadPreset (index); }

const juce::String AT101VProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void AT101VProcessor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    loadPresetData (presetFromJson (presets[(size_t) index].data));
}

void AT101VProcessor::loadPresetData (const Preset& p, bool keepSequence)
{
    for (int i = 0; i < kNumP; ++i) setPanelValue ((P) i, p.panel[(size_t) i]);
    for (auto& e : extParams())
    {
        if (e.global) continue;
        const float v = p.settings.contains (e.id) ? (float) p.settings[e.id] : e.def;
        setExtValue (e.id, v);
    }
    if (! keepSequence) editSequence ([&] (Sequence& s) { s = p.hasSeq ? p.seq : Sequence(); });
    currentPresetName = p.name;
    tracker.capture (*this);
    notifyUi (onPresetChanged);
}

Preset AT101VProcessor::currentAsPreset (const juce::String& name, const juce::String& category) const
{
    Preset p;
    p.name = name; p.category = category;
    for (int i = 0; i < kNumP; ++i) p.panel[(size_t) i] = panelRaw[(size_t) i]->load();
    const auto& ps = extParams();
    for (size_t k = 0; k < ps.size(); ++k)
        if (! ps[k].global && std::abs (extRaw[k]->load() - ps[k].def) > 1e-4f)
            p.settings.set (ps[k].id, extRaw[k]->load());
    p.seq = getSequence();
    p.hasSeq = p.seq.length > 0;
    return p;
}

bool AT101VProcessor::saveUserPreset (const juce::String& name)
{
    const auto dir = userPresetDir();
    dir.createDirectory();
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".json");
    const bool ok = file.replaceWithText (juce::JSON::toString (presetToJson (currentAsPreset (name, "User"))));
    rescanUserPresets();
    currentPresetName = name;
    tracker.capture (*this);
    for (int i = 0; i < (int) presets.size(); ++i) if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) currentProgram = i;
    notifyUi (onPresetChanged);
    return ok;
}

// Patch al azar dentro de rangos musicales (no toca arpegio, secuencia ni HOLD).
void AT101VProcessor::randomizePanel()
{
    juce::Random r;
    auto u = [&] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    Preset p = currentAsPreset ("Random", "");
    auto set = [&] (P id, float v) { p.panel[(size_t) id] = v; };
    set (P::lfo_rate, u (0.2f, 0.85f)); set (P::lfo_wave, (float) r.nextInt (4));
    set (P::vco_mod, r.nextFloat() < 0.75f ? 0.0f : u (0.05f, 0.4f)); set (P::vco_range, (float) r.nextInt ({ 0, 3 }));
    set (P::pwm_src, (float) r.nextInt (3)); set (P::pw, u (0.0f, 0.8f));
    set (P::mix_pulse, r.nextBool() ? u (0.4f, 1.0f) : 0.0f); set (P::mix_saw, u (0.3f, 1.0f));
    set (P::mix_sub, r.nextBool() ? u (0.3f, 1.0f) : 0.0f); set (P::sub_wave, (float) r.nextInt (3));
    set (P::mix_noise, r.nextFloat() < 0.8f ? 0.0f : u (0.05f, 0.4f));
    set (P::vcf_freq, u (0.2f, 0.75f)); set (P::vcf_res, u (0.0f, 0.85f)); set (P::vcf_env, u (0.1f, 0.8f));
    set (P::vcf_mod, r.nextFloat() < 0.7f ? 0.0f : u (0.05f, 0.4f)); set (P::vcf_kybd, u (0.3f, 1.0f));
    set (P::vca_mode, r.nextFloat() < 0.8f ? 0.0f : 1.0f); set (P::env_trig, (float) r.nextInt (3));
    set (P::env_a, r.nextFloat() < 0.7f ? 0.0f : u (0.1f, 0.6f)); set (P::env_d, u (0.15f, 0.75f));
    set (P::env_s, u (0.0f, 0.9f)); set (P::env_r, u (0.05f, 0.6f));
    set (P::portamento, r.nextFloat() < 0.7f ? 0.0f : u (0.1f, 0.5f));
    loadPresetData (p, true);
}

// ───────────── Estado ─────────────
void AT101VProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName.get(), nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("sequence", juce::JSON::toString (sequenceToVar (getSequence()), true), nullptr);
    state.setProperty ("keyTranspose", keyTranspose.load(), nullptr);
    juce::StringArray map;
    for (int c = 0; c < 128; ++c) if (const int t = ccMap[(size_t) c].load(); t >= 0) map.add (juce::String (c) + "=" + allParamIds[t]);
    state.setProperty ("midiMap", map.joinIntoString (","), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AT101VProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    const auto seq = sequenceFromVar (juce::JSON::parse (state.getProperty ("sequence", "[]").toString()));
    for (auto& c : ccMap) c.store (-1);
    for (auto& kv : juce::StringArray::fromTokens (state.getProperty ("midiMap", "").toString(), ",", ""))
    {
        const int cc = kv.upToFirstOccurrenceOf ("=", false, false).getIntValue();
        const int idx = allParamIds.indexOf (kv.fromFirstOccurrenceOf ("=", false, false));
        if (idx >= 0 && juce::isPositiveAndBelow (cc, 128)) ccMap[(size_t) cc].store (idx);
    }
    const int kt = state.getProperty ("keyTranspose", 0);
    for (auto id : { "presetName", "sequence", "midiMap", "keyTranspose" }) state.removeProperty (id, nullptr);
    std::set<juce::String> present;
    for (const auto& c : state) if (c.hasProperty ("id")) present.insert (c.getProperty ("id").toString());
    apvts.replaceState (state);
    // Parámetros que faltan (sesiones viejas) o llegaron como NaN: a su valor por defecto.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! dh::isFiniteF (rp->getValue()) || present.count (rp->paramID) == 0) rp->setValueNotifyingHost (rp->getDefaultValue());
    editSequence ([&] (Sequence& s) { s = seq; });
    keyTranspose.store (kt);
    pendingKeyTranspose.store (juce::jlimit (-12, 24, kt));     // el motor lo toma en el hilo de audio
    currentProgram = juce::jlimit (0, std::max (0, (int) presets.size() - 1), (int) state.getProperty ("program", 0));
    if (state.hasProperty ("presetSnapshot")) tracker.fromString (*this, state.getProperty ("presetSnapshot").toString());
    else tracker.capture (*this);           // sesión anterior a la marca: el nombre sin "*"
    notifyUi (onPresetChanged);
}

juce::AudioProcessorEditor* AT101VProcessor::createEditor() { return new AT101VEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AT101VProcessor(); }
