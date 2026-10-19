#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dh::fs;

ATFS1Processor::ATFS1Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ATFS1", createLayout())
{
    for (auto& p : hostParams()) raw.push_back (apvts.getRawParameterValue (p.id));
    loadFactory();
    rescanUserPresets();
    if (! presets.empty()) loadPreset (0);
    else { voiceSlot.publish (editVoice); }
}

juce::AudioProcessorValueTreeState::ParameterLayout ATFS1Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& e : hostParams())
    {
        const juce::ParameterID pid { e.id, 1 };
        const auto* ep = &e;
        if (e.choices != nullptr)
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, e.name, hostChoices (e), (int) e.def));
        else if (e.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterInt> (pid, e.name, (int) e.lo, (int) e.hi, (int) e.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction ([ep] (int v, int) { return formatHost (*ep, (float) v); })));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, e.name, juce::NormalisableRange<float> (e.lo, e.hi), e.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([ep] (float v, int) { return formatHost (*ep, v); })));
    }
    return layout;
}

bool ATFS1Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void ATFS1Processor::setValue (const juce::String& id, float v)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void ATFS1Processor::sendCommand (int cmd, int arg, int arg2)
{
    const auto w = cmdFifo.write (1);
    if (w.blockSize1 > 0) cmdBuf[(size_t) w.startIndex1] = { cmd, arg, arg2 };
    else if (w.blockSize2 > 0) cmdBuf[(size_t) w.startIndex2] = { cmd, arg, arg2 };
}

void ATFS1Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;
    synth.prepare (sampleRate, samplesPerBlock);
    outL.assign ((size_t) std::max (64, samplesPerBlock) + 64, 0.0f);
    outR.assign ((size_t) std::max (64, samplesPerBlock) + 64, 0.0f);
}

// En bypass: silencio; al entrar se sueltan las voces (el note-off puede llegar durante el bypass).
void ATFS1Processor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    if (! wasBypassed) { synth.engine.allNotesOff(); midiNote.fill (false); wasBypassed = true; }
    midi.clear();
}

void ATFS1Processor::handleCc (int cc, int val)
{
    if (const char* id = Synth::ccToParam (cc))
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost ((float) val / 127.0f);
}

void ATFS1Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    wasBypassed = false;
    if ((int) outL.size() < n) { outL.resize ((size_t) n); outR.resize ((size_t) n); }
    if (! midi.isEmpty()) midiInFlash.store (true);

    const auto& ps = hostParams();
    synth.setParams ([this, &ps] (const char* id) {
        for (size_t k = 0; k < ps.size(); ++k) if (std::strcmp (ps[k].id, id) == 0) return raw[k]->load();
        return 0.0f;
    });
    synth.part.muteMode = muteMode.load();
    const VoiceData& v = voiceSlot.audio();
    const FseqData& f = fseqSlot.audio();

    {
        const auto r = cmdFifo.read (cmdFifo.getNumReady());
        auto run = [&] (const std::array<int, 3>& c) {
            const int note = c[1] & 127;
            switch (c[0])
            {
                case CmdNoteOn: uiNote[(size_t) note] = true; synth.engine.noteOn (note, c[2], v, synth.part); break;
                case CmdNoteOff: uiNote[(size_t) note] = false; if (! midiNote[(size_t) note]) synth.engine.noteOff (note, synth.part); break;
                case CmdPanic: synth.engine.panic(); synth.fx.reset(); uiNote.fill (false); midiNote.fill (false); break;
                default: break;
            }
        };
        for (int i = 0; i < r.blockSize1; ++i) run (cmdBuf[(size_t) (r.startIndex1 + i)]);
        for (int i = 0; i < r.blockSize2; ++i) run (cmdBuf[(size_t) (r.startIndex2 + i)]);
    }

    auto it = midi.cbegin();
    int pos = 0;
    while (pos < n)
    {
        while (it != midi.cend() && (*it).samplePosition <= pos)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn()) midiNote[(size_t) m.getNoteNumber()] = true;
            else if (m.isNoteOff()) midiNote[(size_t) m.getNoteNumber()] = false;
            if (m.isController() && Synth::ccToParam (m.getControllerNumber()) != nullptr) handleCc (m.getControllerNumber(), m.getControllerValue());
            else synth.handleMidi (m, v);
            ++it;
        }
        int end = n;
        if (it != midi.cend()) end = std::min (end, std::max (pos + 1, (*it).samplePosition));
        synth.engine.checkMode (synth.part);
        synth.render (outL.data() + pos, outR.data() + pos, end - pos, v, f.empty() ? nullptr : &f);
        pos = end;
    }
    // Eventos en la última muestra o después (algunos hosts los mandan así): un note-off perdido era una nota colgada.
    for (; it != midi.cend(); ++it)
    {
        const auto m = (*it).getMessage();
        if (m.isNoteOn()) midiNote[(size_t) m.getNoteNumber()] = true;
        else if (m.isNoteOff()) midiNote[(size_t) m.getNoteNumber()] = false;
        if (m.isController() && Synth::ccToParam (m.getControllerNumber()) != nullptr) handleCc (m.getControllerNumber(), m.getControllerValue());
        else synth.handleMidi (m, v);
    }
    midi.clear();

    // Guardia: si algo llegó a NaN/Inf, silencio y voces/efectos reiniciados.
    {
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) sum += outL[(size_t) i] + outR[(size_t) i];
        if (! dh::isFiniteF (sum))
        {
            std::fill (outL.begin(), outL.begin() + n, 0.0f); std::fill (outR.begin(), outR.begin() + n, 0.0f);
            synth.engine.panic(); synth.fx.reset();
        }
    }

    if (! power.load()) { std::fill (outL.begin(), outL.begin() + n, 0.0f); std::fill (outR.begin(), outR.begin() + n, 0.0f); }
    auto out = getBusBuffer (buffer, false, 0);
    if (out.getNumChannels() >= 2) { out.copyFrom (0, 0, outL.data(), n); out.copyFrom (1, 0, outR.data(), n); }
    else if (out.getNumChannels() == 1) { out.copyFrom (0, 0, outL.data(), n, 0.5f); out.addFrom (0, 0, outR.data(), n, 0.5f); }
    for (int c = out.getNumChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    {
        int sp = live.scopePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) { live.scope[(size_t) sp] = 0.5f * (outL[(size_t) i] + outR[(size_t) i]); sp = (sp + 1) & 4095; }
        live.scopePos.store (sp, std::memory_order_release);
    }
    for (int k = 0; k < 16; ++k) live.opMeter[(size_t) k].store (synth.engine.opMeter[(size_t) k]);
    live.voices.store (synth.engine.activeVoices());
    live.fseqPos.store (synth.engine.fseqPosition());
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < n; ++i) { pl = std::max (pl, std::abs (outL[(size_t) i])); pr = std::max (pr, std::abs (outR[(size_t) i])); }
    live.outL.store (std::max (pl, live.outL.load() * 0.85f)); live.outR.store (std::max (pr, live.outR.load() * 0.85f));
    for (int k = 0; k < 128; ++k) live.keyDown[(size_t) k].store (uiNote[(size_t) k] || midiNote[(size_t) k]);

    const double used = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    cpuLoad.store (cpuLoad.load() * 0.9f + (float) (used / (n / currentSr)) * 0.1f);
}

void ATFS1Processor::notifyUi (std::function<void()>& cb)
{
    if (! cb) return;
    if (juce::MessageManager::existsAndIsCurrentThread()) { cb(); return; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<ATFS1Processor> (this), &cb] { if (safe != nullptr && cb) cb(); });
}

// ───────────── Fseq ─────────────
void ATFS1Processor::setFseq (const FseqData& f, const juce::String& ref)
{
    editFseq = f;
    currentFseqRef = ref;
    fseqSlot.publish (editFseq);
    const int n = (int) f.frames.size();
    if (n > 0 && ref.isEmpty())
    {
        setValue ("fseq_lp_start", (float) juce::jlimit (0, n - 1, f.loopStart));
        setValue ("fseq_lp_end", (float) juce::jlimit (0, n - 1, f.loopEnd));
    }
    notifyUi (onFseqChanged);
}

void ATFS1Processor::setFseqFromLibrary (int index)
{
    if (juce::isPositiveAndBelow (index, (int) library.size())) setFseq (library[(size_t) index], library[(size_t) index].name);
}

int ATFS1Processor::libraryIndexOf (const juce::String& name) const
{
    for (size_t i = 0; i < library.size(); ++i) if (name == juce::String (library[i].name)) return (int) i;
    return -1;
}

juce::String ATFS1Processor::importSyx (const juce::File& file)
{
    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb)) return "Could not read " + file.getFileName();
    std::vector<uint8_t> raw8 ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
    // Un .mid guarda cada sysex como F0 <largo> datos: se leen con MidiFile y se rearman como F0 ... F7.
    if (mb.getSize() >= 4 && std::memcmp (mb.getData(), "MThd", 4) == 0)
    {
        juce::MemoryInputStream in (mb, false);
        juce::MidiFile mf;
        raw8.clear();
        if (mf.readFrom (in))
            for (int t = 0; t < mf.getNumTracks(); ++t)
                for (auto* e : *mf.getTrack (t))
                    if (e->message.isSysEx())
                    {
                        const auto* d = e->message.getSysExData();
                        raw8.push_back (0xF0);
                        raw8.insert (raw8.end(), d, d + e->message.getSysExDataSize());
                        if (raw8.back() != 0xF7) raw8.push_back (0xF7);
                    }
    }
    const auto bulks = syx::parse (raw8);
    std::vector<VoiceData> vs; int fseqs = 0;
    for (auto& b : bulks)
    {
        if (syx::isVoice (b)) vs.push_back (syx::readVoice (b.data.data()));
        else if (syx::isFseq (b))
        {
            auto f = syx::readFseq (b.data);
            if (f.name.empty()) f.name = file.getFileNameWithoutExtension().substring (0, 8).toStdString();
            setFseq (f);
            setValue ("fseq", 1.0f);
            ++fseqs;
        }
    }
    if (! vs.empty())
    {
        importedVoices = vs;
        setVoice (vs.front());
        currentPresetName = juce::String (vs.front().name);
        notifyUi (onPresetChanged);
    }
    if (vs.empty() && fseqs == 0) return "No FS1R voice or Fseq found in " + file.getFileName();
    juce::String s = "Imported";
    if (! vs.empty()) s << " " << (int) vs.size() << (vs.size() == 1 ? " voice" : " voices");
    if (fseqs > 0) s << (vs.empty() ? " " : " and ") << fseqs << " Fseq";
    return s;
}

bool ATFS1Processor::exportVoiceSyx (const juce::File& f) const
{
    const auto d = syx::voiceSyx (editVoice);
    return f.replaceWithData (d.data(), d.size());
}

bool ATFS1Processor::exportFseqSyx (const juce::File& f) const
{
    if (editFseq.empty()) return false;
    const auto d = syx::fseqSyx (editFseq);
    return f.replaceWithData (d.data(), d.size());
}

void ATFS1Processor::analyzeAudioAsync (const juce::File& file)
{
    if (analyzing.exchange (true)) return;
    postMessage ("Analyzing " + file.getFileName() + "...");
    pool.addJob ([file, safe = juce::WeakReference<ATFS1Processor> (this)] {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (file));
        FseqData f; juce::String msg;
        if (rd == nullptr) msg = "Unsupported audio file: " + file.getFileName();
        else
        {
            // Hasta 512 cuadros (4 s a 126 cuadros/s): el largo máximo de un Fseq del FS1R.
            const int len = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 4.2));
            juce::AudioBuffer<float> b ((int) rd->numChannels, len);
            rd->read (&b, 0, len, 0, true, true);
            std::vector<float> mono ((size_t) len);
            for (int c = 0; c < b.getNumChannels(); ++c)
                for (int i = 0; i < len; ++i) mono[(size_t) i] += b.getSample (c, i) / (float) b.getNumChannels();
            f = analyze (mono, (float) rd->sampleRate, file.getFileNameWithoutExtension().substring (0, 8).toStdString());
            msg = f.empty() ? "The recording is too short to analyze" : "Fseq from " + file.getFileName() + ": " + juce::String ((int) f.frames.size()) + " frames";
        }
        juce::MessageManager::callAsync ([safe, f, msg] {
            if (safe == nullptr) return;
            safe->analyzing.store (false);
            if (! f.empty())
            {
                auto g = f;
                g.loopStart = 0; g.loopEnd = 0;
                safe->setFseq (g);
                safe->setValue ("fseq", 1.0f);
                safe->setValue ("fseq_loop", 0.0f);
            }
            safe->postMessage (msg);
        });
    });
}

// ───────────── Presets ─────────────
void ATFS1Processor::loadFactory()
{
   #if ATFS1_HAS_ASSETS
    juce::StringArray pnames, fnames;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String nm (BinaryData::namedResourceList[i]);
        if (! nm.endsWith ("_json")) continue;
        if (nm.startsWith ("p") && nm.substring (1, 3).containsOnly ("0123456789")) pnames.add (nm);
        if (nm.startsWith ("f") && nm.substring (1, 3).containsOnly ("0123456789")) fnames.add (nm);
    }
    fnames.sort (true);
    for (auto& nm : fnames)
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource (nm.toRawUTF8(), size);
        const auto v = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        auto f = fseqFromBase64 (v.getProperty ("data", "").toString());
        f.name = v.getProperty ("name", nm).toString().toStdString();
        if (! f.empty()) library.push_back (f);
    }
    pnames.sort (true);
    for (auto& nm : pnames)
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource (nm.toRawUTF8(), size);
        const auto v = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        presets.push_back ({ v.getProperty ("name", nm).toString(), v.getProperty ("category", "").toString(), v, true, {} });
    }
   #endif
}

juce::File ATFS1Processor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/ATFS1/Presets");
   #else
    return base.getChildFile ("Audio Tools/ATFS1/Presets");
   #endif
}

void ATFS1Processor::rescanUserPresets()
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

int ATFS1Processor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void ATFS1Processor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String ATFS1Processor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void ATFS1Processor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    loadPresetData (presetFromJson (presets[(size_t) index].data));
}

void ATFS1Processor::loadPresetData (const Preset& p)
{
    for (auto& e : hostParams())
    {
        if (e.global) continue;
        setValue (e.id, p.params.contains (e.id) ? (float) p.params[e.id] : e.def);
    }
    if (p.fseqRef.isNotEmpty() && libraryIndexOf (p.fseqRef) >= 0) setFseqFromLibrary (libraryIndexOf (p.fseqRef));
    else if (! p.fseqInline.empty()) setFseq (p.fseqInline);
    else setFseq ({});
    // Los puntos de lazo del preset pisan los del Fseq.
    for (auto id : { "fseq_lp_start", "fseq_lp_end" }) if (p.params.contains (id)) setValue (id, (float) p.params[id]);
    currentPresetName = p.name;
    voiceDirty = false;
    setVoice (p.voice);
    sendCommand (CmdPanic);
    notifyUi (onPresetChanged);
}

Preset ATFS1Processor::currentAsPreset (const juce::String& name, const juce::String& category) const
{
    Preset p;
    p.name = name; p.category = category;
    p.voice = editVoice;
    p.voice.name = name.substring (0, 10).toStdString();
    const auto& ps = hostParams();
    for (size_t k = 0; k < ps.size(); ++k)
        if (! ps[k].global && std::abs (raw[k]->load() - ps[k].def) > 1e-4f) p.params.set (ps[k].id, raw[k]->load());
    if (currentFseqRef.isNotEmpty()) p.fseqRef = currentFseqRef;
    else p.fseqInline = editFseq;
    return p;
}

bool ATFS1Processor::saveUserPreset (const juce::String& name)
{
    const auto dir = userPresetDir();
    dir.createDirectory();
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".json");
    const bool ok = file.replaceWithText (juce::JSON::toString (presetToJson (currentAsPreset (name, "User"))));
    rescanUserPresets();
    currentPresetName = name;
    voiceDirty = false;
    for (int i = 0; i < (int) presets.size(); ++i) if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) currentProgram = i;
    notifyUi (onPresetChanged);
    return ok;
}

// ───────────── Estado ─────────────
void ATFS1Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("voice", juce::JSON::toString (voiceToJson (editVoice), true), nullptr);
    if (currentFseqRef.isNotEmpty()) state.setProperty ("fseqRef", currentFseqRef, nullptr);
    else if (! editFseq.empty()) state.setProperty ("fseqData", fseqToBase64 (editFseq), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ATFS1Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    currentProgram = state.getProperty ("program", 0);
    const auto voiceJson = state.getProperty ("voice", "").toString();
    const auto ref = state.getProperty ("fseqRef", "").toString();
    const auto fdata = state.getProperty ("fseqData", "").toString();
    for (auto id : { "presetName", "program", "voice", "fseqRef", "fseqData" }) state.removeProperty (id, nullptr);
    apvts.replaceState (state);
    if (voiceJson.isNotEmpty()) setVoice (voiceFromJson (juce::JSON::parse (voiceJson)));
    if (ref.isNotEmpty() && libraryIndexOf (ref) >= 0) { editFseq = library[(size_t) libraryIndexOf (ref)]; currentFseqRef = ref; fseqSlot.publish (editFseq); }
    else if (fdata.isNotEmpty()) { editFseq = fseqFromBase64 (fdata); currentFseqRef = {}; fseqSlot.publish (editFseq); }
    else { editFseq = {}; currentFseqRef = {}; fseqSlot.publish (editFseq); }
    notifyUi (onFseqChanged);
    notifyUi (onPresetChanged);
}

juce::AudioProcessorEditor* ATFS1Processor::createEditor() { return new ATFS1Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ATFS1Processor(); }
