#include <set>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

using namespace dh;

AT700Processor::AT700Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AT700", createLayout())
{
    for (int i = 0; i < kNumP700; ++i) { panelRaw[(size_t) i] = apvts.getRawParameterValue (panelParam700 (i).id); allParamIds.add (panelParam700 (i).id); }
    for (auto& e : extParams700()) { extRaw.push_back (apvts.getRawParameterValue (e.id)); allParamIds.add (e.id); }
    for (auto& a : patchAtomic) a.store (-1);
    for (auto& c : ccMap) c.store (-1);
    loadFactoryPresets();
    rescanUserPresets();
    if (! presets.empty()) loadPreset (0);
}

juce::AudioProcessorValueTreeState::ParameterLayout AT700Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumP700; ++i)
    {
        const auto& pp = panelParam700 (i);
        if (pp.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pp.id, 1 }, pp.name, panelChoices700 (i), (int) pp.def));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pp.id, 1 }, pp.name, juce::NormalisableRange<float> (0.0f, 1.0f), pp.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([i] (float v, int) { return formatPanel700 (i, v); })));
    }
    for (auto& e : extParams700())
    {
        if (e.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { e.id, 1 }, e.name, extChoices700 (e), (int) e.def));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { e.id, 1 }, e.name, juce::NormalisableRange<float> (e.lo, e.hi), e.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([&e] (float v, int) { return formatExt700 (e, v); })));
    }
    return layout;
}

bool AT700Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AT700Processor::setValue (int i, float v)
{
    if (auto* prm = apvts.getParameter (panelParam700 (i).id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void AT700Processor::setExtValue (const juce::String& id, float v)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void AT700Processor::sendCommand (int cmd, int arg)
{
    const auto w = cmdFifo.write (1);
    if (w.blockSize1 > 0) cmdBuf[(size_t) w.startIndex1] = { cmd, arg };
    else if (w.blockSize2 > 0) cmdBuf[(size_t) w.startIndex2] = { cmd, arg };
}

void AT700Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;
    currentBlock = std::max (64, samplesPerBlock);
    engine.prepare (sampleRate, currentBlock, kOversampling700[juce::jlimit (0, 2, (int) apvts.getRawParameterValue ("quality")->load())]);
    for (int i = 0; i < kNumP700; ++i) smoothed[(size_t) i] = panelRaw[(size_t) i]->load();
    outL.assign ((size_t) currentBlock + 64, 0.0f);
    outR.assign ((size_t) currentBlock + 64, 0.0f);
    seenPatchVersion = -1;
}

void AT700Processor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) { const int n = m.getNoteNumber(); midiNote[(size_t) n] = true; engine.kb.noteOn (n, m.getFloatVelocity()); }
    else if (m.isNoteOff()) { const int n = m.getNoteNumber(); midiNote[(size_t) n] = false; if (! uiNote[(size_t) n]) engine.kb.noteOff (n); }
    else if (m.isPitchWheel())
    {
        midiBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;     // si MIDI Bend está apagado no actúa (ver processBlock)
    }
    else if (m.isChannelPressure()) engine.kb.aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch()) engine.kb.aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        if (cc == 120 || cc == 123) { engine.kb.allOff(); midiNote.fill (false); return; }     // antes que un CC aprendido
        if (cc == 121) { midiBend = 0.0f; engine.kb.modWheel = 0.0f; engine.kb.aftertouch = 0.0f; return; }
        // El aprendizaje no toma el pedal ni los mensajes de modo de canal (los hosts los mandan al parar).
        if (learning.load() && cc != 64 && cc < 120) { pendingLearnCc.store (cc); learning.store (false); return; }
        const int target = ccMap[(size_t) cc].load();
        if (target >= 0 && target < allParamIds.size())
        {
            if (auto* prm = apvts.getParameter (allParamIds[target])) prm->setValueNotifyingHost ((float) val / 127.0f);
            return;
        }
        if (cc == 1) engine.kb.modWheel = (float) val / 127.0f;
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { engine.kb.allOff(); midiNote.fill (false); }
}

void AT700Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    wasBypassed = false;
    const int wantOs = kOversampling700[juce::jlimit (0, 2, (int) apvts.getRawParameterValue ("quality")->load())];
    if (wantOs != engine.oversampling()) engine.prepare (currentSr, currentBlock, wantOs);
    if ((int) outL.size() < n) { outL.resize ((size_t) n); outR.resize ((size_t) n); }
    if (! midi.isEmpty()) midiInFlash.store (true);

    if (const int cc = pendingLearnCc.exchange (-1); cc >= 0)
    {
        const int idx = learnIndex.load();
        if (idx >= 0 && idx < allParamIds.size()) { for (auto& c : ccMap) if (c.load() == idx) c.store (-1); ccMap[(size_t) cc].store (idx); }
    }

    auto get = [this] (const char* id) {
        const auto& ps = extParams700();
        for (size_t k = 0; k < ps.size(); ++k) if (std::strcmp (ps[k].id, id) == 0) return extRaw[k]->load();
        return 0.0f;
    };
    const int prevMode = engine.kb.mode, prevVoices = engine.kb.voices();
    applyExt700 (get, engine);
    if (engine.kb.mode != prevMode || engine.kb.voices() != prevVoices) { engine.kb.allOff(); uiNote.fill (false); midiNote.fill (false); }
    if (const int pv = patchVersion.load(); pv != seenPatchVersion)
    {
        seenPatchVersion = pv;
        for (int j = 0; j < jk700::kNumJacks; ++j) engine.patch.src[(size_t) j] = patchAtomic[(size_t) j].load (std::memory_order_relaxed);
        engine.computePoly();
    }
    engine.manualGate = manualGate.load();
    const bool bendOn = apvts.getRawParameterValue ("midi_bend")->load() > 0.5f;    // apagado: el bend MIDI no queda pegado
    engine.bender = juce::jlimit (-1.0f, 1.0f, uiBender.load() + (bendOn ? midiBend : 0.0f));

    // Comandos del editor.
    {
        const auto r = cmdFifo.read (cmdFifo.getNumReady());
        auto run = [this] (std::pair<int, int> c) {
            switch (c.first)
            {
                case CmdNoteOn: uiNote[(size_t) (c.second & 127)] = true; engine.kb.noteOn (c.second & 127, 0.8f); break;
                case CmdNoteOff: uiNote[(size_t) (c.second & 127)] = false; if (! midiNote[(size_t) (c.second & 127)]) engine.kb.noteOff (c.second & 127); break;
                case CmdSeqStart: engine.seqStart(); break;
                case CmdSeqContinue: engine.seqContinue(); break;
                case CmdSeqStop: engine.seqStop(); break;
                case CmdSeqEnd: engine.seqEnd(); break;
                case CmdSeqStep: engine.seqManual (c.second); break;
                case CmdSeqChan: engine.seqManualChan (c.second); break;
                case CmdPanic: engine.kb.allOff(); uiNote.fill (false); midiNote.fill (false); break;
                default: break;
            }
        };
        for (int i = 0; i < r.blockSize1; ++i) run (cmdBuf[(size_t) (r.startIndex1 + i)]);
        for (int i = 0; i < r.blockSize2; ++i) run (cmdBuf[(size_t) (r.startIndex2 + i)]);
    }

    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    engine.st.bpm = bpm; engine.st.ppq = ppq; engine.st.hostPlaying = playing;

    auto it = midi.cbegin();
    int pos = 0;
    while (pos < n)
    {
        int end = std::min (n, pos + 32);
        while (it != midi.cend() && (*it).samplePosition <= pos) { handleMidi ((*it).getMessage()); ++it; }
        if (it != midi.cend()) end = std::min (end, std::max (pos + 1, (*it).samplePosition));
        engine.bender = juce::jlimit (-1.0f, 1.0f, uiBender.load() + (bendOn ? midiBend : 0.0f));
        for (int i = 0; i < kNumP700; ++i)
        {
            const float target = panelRaw[(size_t) i]->load();
            if (panelParam700 (i).steps > 0) smoothed[(size_t) i] = target;
            else smoothed[(size_t) i] += (target - smoothed[(size_t) i]) * 0.3f;
            if (! dh::isFiniteF (smoothed[(size_t) i])) smoothed[(size_t) i] = dh::isFiniteF (target) ? target : 0.0f;
        }
        engine.p = smoothed;
        engine.render (outL.data() + pos, outR.data() + pos, end - pos);
        pos = end;
    }
    while (it != midi.cend()) { handleMidi ((*it).getMessage()); ++it; }
    midi.clear();

    auto out = getBusBuffer (buffer, false, 0);
    if (out.getNumChannels() >= 2) { out.copyFrom (0, 0, outL.data(), n); out.copyFrom (1, 0, outR.data(), n); }
    else if (out.getNumChannels() == 1) { out.copyFrom (0, 0, outL.data(), n, 0.5f); out.addFrom (0, 0, outR.data(), n, 0.5f); }
    for (int c = out.getNumChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    // Estado visible.
    const auto& v = engine.view;
    for (int k = 0; k < 2; ++k)
    {
        live.vcfOver[(size_t) k].store (v.vcfOver[(size_t) k]); live.vcfSig[(size_t) k].store (v.vcfSig[(size_t) k]);
        live.vcaOver[(size_t) k].store (v.vcaOver[(size_t) k]); live.vcaSig[(size_t) k].store (v.vcaSig[(size_t) k]);
    }
    live.rmSig.store (v.rmSig); live.ef.store (v.ef); live.lfo1.store (v.lfo1); live.lfo2.store (v.lfo2);
    live.vuL.store (v.vuL); live.vuR.store (v.vuR); live.pshaper.store (v.pshaper); live.clock.store (v.clock);
    live.step.store (v.step); live.chan.store (v.chan); live.running.store (v.running); live.power.store (v.power);
    live.bender.store (engine.bender);
    for (int k = 0; k < 128; ++k) live.keyDown[(size_t) k].store (uiNote[(size_t) k] || midiNote[(size_t) k]);

    const double used = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    cpuLoad.store (cpuLoad.load() * 0.9f + (float) (used / (n / currentSr)) * 0.1f);
}

void AT700Processor::notifyUi (std::function<void()>& cb)
{
    if (! cb) return;
    if (juce::MessageManager::existsAndIsCurrentThread()) { cb(); return; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<AT700Processor> (this), &cb] { if (safe != nullptr && cb) cb(); });
}

// En bypass: silencio; al entrar se sueltan las teclas (el note-off puede llegar durante el bypass).
void AT700Processor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    if (! wasBypassed) { engine.kb.allOff(); midiNote.fill (false); wasBypassed = true; }
    midi.clear();
}

void AT700Processor::setCables (std::vector<Cable700> c)
{
    { const juce::SpinLock::ScopedLockType l (cableLock); cables = std::move (c); }
    const auto ps = patchFromCables700 (getCables());
    for (int j = 0; j < jk700::kNumJacks; ++j) patchAtomic[(size_t) j].store (ps.src[(size_t) j]);
    patchVersion.fetch_add (1);
    notifyUi (onCablesChanged);
}

// ───────────── MIDI learn ─────────────
void AT700Processor::clearMidiMapping (const juce::String& paramId)
{
    const int idx = allParamIds.indexOf (paramId);
    for (auto& c : ccMap) if (c.load() == idx) c.store (-1);
}

int AT700Processor::ccForParam (const juce::String& paramId) const
{
    const int idx = allParamIds.indexOf (paramId);
    for (int c = 0; c < 128; ++c) if (idx >= 0 && ccMap[(size_t) c].load() == idx) return c;
    return -1;
}

// ───────────── Presets ─────────────
void AT700Processor::loadFactoryPresets()
{
   #if AT700_HAS_ASSETS
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

juce::File AT700Processor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/AT700/Presets");
   #else
    return base.getChildFile ("Audio Tools/AT700/Presets");
   #endif
}

void AT700Processor::rescanUserPresets()
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

int AT700Processor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void AT700Processor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String AT700Processor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void AT700Processor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    loadPresetData (presetFromJson700 (presets[(size_t) index].data));
}

void AT700Processor::loadPresetData (const Preset700& p)
{
    for (int i = 0; i < kNumP700; ++i) setValue (i, p.panel[(size_t) i]);
    for (auto& e : extParams700())
    {
        if (e.global) continue;
        setExtValue (e.id, p.settings.contains (e.id) ? (float) p.settings[e.id] : e.def);
    }
    currentPresetName = p.name;
    setCables (p.cables);
    sendCommand (CmdSeqEnd);
    if (p.run) sendCommand (CmdSeqStart);
    notifyUi (onPresetChanged);
}

Preset700 AT700Processor::currentAsPreset (const juce::String& name, const juce::String& category) const
{
    Preset700 p;
    p.name = name; p.category = category;
    for (int i = 0; i < kNumP700; ++i) p.panel[(size_t) i] = panelRaw[(size_t) i]->load();
    const auto& ps = extParams700();
    for (size_t k = 0; k < ps.size(); ++k)
        if (! ps[k].global && std::abs (extRaw[k]->load() - ps[k].def) > 1e-4f) p.settings.set (ps[k].id, extRaw[k]->load());
    p.cables = cables;
    return p;
}

bool AT700Processor::saveUserPreset (const juce::String& name)
{
    const auto dir = userPresetDir();
    dir.createDirectory();
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".json");
    const bool ok = file.replaceWithText (juce::JSON::toString (presetToJson700 (currentAsPreset (name, "User"))));
    rescanUserPresets();
    currentPresetName = name;
    for (int i = 0; i < (int) presets.size(); ++i) if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) currentProgram = i;
    notifyUi (onPresetChanged);
    return ok;
}

// Patch al azar: mueve oscilador, filtros, envolventes y moduladores; deja cables y mezcla de salida.
void AT700Processor::randomizePanel()
{
    juce::Random r;
    Preset700 p = currentAsPreset ("Random", "");
    auto set = [&] (const char* id, float v) { if (const int i = panelIndex700 (id); i >= 0) p.panel[(size_t) i] = v; };
    for (int n = 1; n <= 3; ++n)
    {
        const juce::String b = "vco" + juce::String (n) + "_";
        set ((b + "wave").toRawUTF8(), (float) r.nextInt (4));
        set ((b + "range").toRawUTF8(), (float) (1 + r.nextInt (5)));
        set ((b + "pitch").toRawUTF8(), 0.5f + (float) (r.nextInt (25) - 12) / 24.0f * (r.nextFloat() < 0.6f ? 0.0f : 1.0f));
        set ((b + "pw").toRawUTF8(), r.nextFloat());
        set ((b + "mod1").toRawUTF8(), r.nextFloat() < 0.3f ? r.nextFloat() * 0.3f : 0.0f);
    }
    for (int n = 1; n <= 2; ++n)
    {
        const juce::String b = "vcf" + juce::String (n) + "_";
        set ((b + "cutoff").toRawUTF8(), 0.3f + r.nextFloat() * 0.6f);
        set ((b + "res").toRawUTF8(), r.nextFloat() * 0.8f);
        set ((b + "mod2").toRawUTF8(), r.nextFloat() * 0.6f);
        for (int k = 1; k <= 3; ++k) set ((b + "lvl" + juce::String (k)).toRawUTF8(), r.nextFloat() < 0.5f ? r.nextFloat() : 0.0f);
    }
    for (int a = 1; a <= 2; ++a)
        for (auto s : { "_a", "_d", "_s", "_r" })
            set (("adsr" + juce::String (a) + s).toRawUTF8(), r.nextFloat() * (juce::String (s) == "_a" ? 0.6f : 1.0f));
    set ("lfo1_freq", r.nextFloat()); set ("lfo2_freq", r.nextFloat());
    set ("lfo1_wave", (float) r.nextInt (4)); set ("lfo2_wave", (float) r.nextInt (4));
    loadPresetData (p);
}

// ───────────── Estado ─────────────
void AT700Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const auto pr = presetToJson700 (currentAsPreset (currentPresetName, ""));
    state.setProperty ("presetName", currentPresetName.get(), nullptr);
    state.setProperty ("cables", juce::JSON::toString (pr.getProperty ("cables", {}), true), nullptr);
    juce::StringArray map;
    for (int c = 0; c < 128; ++c) if (const int t = ccMap[(size_t) c].load(); t >= 0) map.add (juce::String (c) + "=" + allParamIds[t]);
    state.setProperty ("midiMap", map.joinIntoString (","), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AT700Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    auto* o = new juce::DynamicObject();
    o->setProperty ("cables", juce::JSON::parse (state.getProperty ("cables", "[]").toString()));
    const auto extra = presetFromJson700 (juce::var (o));
    for (auto& c : ccMap) c.store (-1);
    for (auto& kv : juce::StringArray::fromTokens (state.getProperty ("midiMap", "").toString(), ",", ""))
    {
        const int cc = kv.upToFirstOccurrenceOf ("=", false, false).getIntValue();
        const int idx = allParamIds.indexOf (kv.fromFirstOccurrenceOf ("=", false, false));
        if (idx >= 0 && juce::isPositiveAndBelow (cc, 128)) ccMap[(size_t) cc].store (idx);
    }
    for (auto id : { "presetName", "cables", "midiMap" }) state.removeProperty (id, nullptr);
    std::set<juce::String> present;
    for (const auto& c : state) if (c.hasProperty ("id")) present.insert (c.getProperty ("id").toString());
    apvts.replaceState (state);
    // Parámetros que faltan (sesiones viejas) o llegaron como NaN: a su valor por defecto.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! dh::isFiniteF (rp->getValue()) || present.count (rp->paramID) == 0) rp->setValueNotifyingHost (rp->getDefaultValue());
    setCables (extra.cables);
    notifyUi (onPresetChanged);
}

juce::AudioProcessorEditor* AT700Processor::createEditor() { return new AT700Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AT700Processor(); }
