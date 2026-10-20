#include "PluginProcessor.h"
#if ! ORRERY_NO_EDITOR
 #include "PluginEditor.h"
#endif

using namespace dh::orr;

// ───────────── salidas MIDI directas ─────────────
void DirectMidi::allNotesOffOn (int slot, int channel)
{
    if (! juce::isPositiveAndBelow (slot, kSlots) || slots[(size_t) slot] == nullptr) return;
    const int ch = juce::jlimit (1, 16, channel);
    slots[(size_t) slot]->sendMessageNow (juce::MidiMessage::controllerEvent (ch, 64, 0));
    slots[(size_t) slot]->sendMessageNow (juce::MidiMessage::allNotesOff (ch));
    slots[(size_t) slot]->sendMessageNow (juce::MidiMessage::allSoundOff (ch));
}

DirectMidi::~DirectMidi()
{
    stopThread (2000);
    // Al cerrar, cada sinte recibe All Notes Off en el canal de su carril (si no, las notas quedaban colgadas).
    const juce::ScopedLock sl (sendLock);
    for (int l = 0; l < kLanes; ++l) allNotesOffOn (laneSlot[(size_t) l].load(), laneChannel[(size_t) l].load());
}

void DirectMidi::setLanePort (int lane, const juce::String& identifier)
{
    if (! juce::isPositiveAndBelow (lane, kLanes)) return;
    lanePorts[(size_t) lane] = identifier;
    const juce::ScopedLock sl (sendLock);
    // El puerto que deja el carril recibe All Notes Off en su canal antes del cambio.
    if (const int old = laneSlot[(size_t) lane].load(); old >= 0 && slotIds[(size_t) old] != identifier) allNotesOffOn (old, laneChannel[(size_t) lane].load());
    // Puertos que ya no usa ningún carril: se cierran.
    std::array<bool, kSlots> used {};
    int slot = -1;
    if (identifier.isNotEmpty())
    {
        for (int s = 0; s < kSlots; ++s) if (slotIds[(size_t) s] == identifier && slots[(size_t) s] != nullptr) slot = s;
        if (slot < 0)
            for (int s = 0; s < kSlots && slot < 0; ++s)
            {
                bool inUse = false;
                for (int l = 0; l < kLanes; ++l) if (l != lane && laneSlot[(size_t) l].load() == s) inUse = true;
                if (! inUse)
                {
                    slots[(size_t) s] = juce::MidiOutput::openDevice (identifier);
                    slotIds[(size_t) s] = slots[(size_t) s] != nullptr ? identifier : juce::String();
                    if (slots[(size_t) s] != nullptr) slot = s;
                }
            }
    }
    laneSlot[(size_t) lane].store (slot);
    for (int l = 0; l < kLanes; ++l) if (laneSlot[(size_t) l].load() >= 0) used[(size_t) laneSlot[(size_t) l].load()] = true;
    for (int s = 0; s < kSlots; ++s) if (! used[(size_t) s] && slots[(size_t) s] != nullptr) { slots[(size_t) s].reset(); slotIds[(size_t) s] = {}; }
}

// Puertos elegidos que no se pudieron abrir (el sinte estaba apagado al cargar el proyecto): se reintentan.
void DirectMidi::reopenMissing()
{
    for (int l = 0; l < kLanes; ++l)
        if (lanePorts[(size_t) l].isNotEmpty() && laneSlot[(size_t) l].load() < 0) setLanePort (l, lanePorts[(size_t) l]);
}

void DirectMidi::push (double atMs, int slot, const uint8_t* data, int size)
{
    const auto w = fifo.write (1);
    Msg m; m.at = atMs; m.slot = (int8_t) slot; m.size = (uint8_t) size; std::memcpy (m.data, data, (size_t) std::min (3, size));
    if (w.blockSize1 > 0) buf[(size_t) w.startIndex1] = m;
    else if (w.blockSize2 > 0) buf[(size_t) w.startIndex2] = m;
}

void DirectMidi::pushToAll (double atMs, const uint8_t* data, int size)
{
    std::array<bool, kSlots> done {};
    for (int l = 0; l < kLanes; ++l)
    {
        const int s = laneSlot[(size_t) l].load (std::memory_order_relaxed);
        if (s >= 0 && ! done[(size_t) s]) { done[(size_t) s] = true; push (atMs, s, data, size); }
    }
}

void DirectMidi::run()
{
    while (! threadShouldExit())
    {
        if (fifo.getNumReady() == 0) { wait (1); continue; }
        const auto r = fifo.read (1);
        const Msg m = buf[(size_t) (r.blockSize1 > 0 ? r.startIndex1 : r.startIndex2)];
        // Espera hasta su hora (en tramos de 50 ms para poder salir); antes, lo que venía a más de 50 ms salía antes.
        for (double wait_ = m.at - juce::Time::getMillisecondCounterHiRes(); wait_ > 1.0 && ! threadShouldExit();
             wait_ = m.at - juce::Time::getMillisecondCounterHiRes())
            juce::Thread::sleep (juce::jmin (50, (int) wait_));
        const juce::ScopedLock sl (sendLock);
        if (juce::isPositiveAndBelow ((int) m.slot, kSlots) && slots[(size_t) m.slot] != nullptr)
            slots[(size_t) m.slot]->sendMessageNow (juce::MidiMessage (m.data, m.size));
    }
}

// ───────────── procesador ─────────────
OrreryProcessor::OrreryProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Orrery", createLayout())
{
    for (auto& p : params()) raw.push_back (apvts.getRawParameterValue (p.id));
    vals.assign (raw.size(), 0.0f);
    events.reserve (4096);
    loadFactory();
    rescanUserPresets();
    { juce::StringArray g; for (auto& e : params()) if (e.global) g.add (e.id); tracker.ignore (g); }   // ajustes de sesión: no "modifican" el preset
    tracker.capture (*this);
}

OrreryProcessor::~OrreryProcessor()
{
    if (linkMode == 1) LinkHub::get().leaders.fetch_sub (1);
}

juce::AudioProcessorValueTreeState::ParameterLayout OrreryProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& e : params())
    {
        const juce::ParameterID pid { e.id, 1 };
        const auto* ep = &e;
        if (e.choices.isNotEmpty())
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, e.name, juce::StringArray::fromTokens (e.choices, "|", ""), (int) (e.def - e.lo)));
        else if (e.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterInt> (pid, e.name, (int) e.lo, (int) e.hi, (int) e.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction ([ep] (int v, int) { return formatParam (*ep, (float) v); })));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, e.name, juce::NormalisableRange<float> (e.lo, e.hi), e.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([ep] (float v, int) { return formatParam (*ep, v); })));
    }
    return layout;
}

bool OrreryProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void OrreryProcessor::setValue (const juce::String& id, float v)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void OrreryProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    gen.prepare (sampleRate);
    preview.prepare ((float) sampleRate, std::max (64, samplesPerBlock) + 64);
    monoR.assign ((size_t) std::max (64, samplesPerBlock) + 64, 0.0f);
    directQ.reserve (8192);
}

void OrreryProcessor::queueDirect (double at, int slot, const uint8_t* d, int size)
{
    if (directQ.size() >= directQ.capacity()) return;      // sin crecer en el hilo de audio
    DirectItem it; it.at = at; it.slot = slot; it.size = (uint8_t) std::min (3, size); std::memcpy (it.data, d, it.size);
    directQ.push_back (it);
}

// Encola los mensajes del bloque en orden de hora: la cola del hilo de salida es estrictamente FIFO, y un
// tick de reloj al principio del bloque esperaba detrás de una nota del final (hasta un bloque de jitter).
void OrreryProcessor::flushDirect()
{
    std::stable_sort (directQ.begin(), directQ.end(), [] (const DirectItem& a, const DirectItem& b) { return a.at < b.at; });
    for (auto& it : directQ)
    {
        if (it.slot >= 0) direct.push (it.at, it.slot, it.data, it.size);
        else direct.pushToAll (it.at, it.data, it.size);
    }
    directQ.clear();
}

// En bypass: al entrar se cierran todas las notas (al host y a los puertos directos); la entrada MIDI no pasa.
void OrreryProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();
    if (wasBypassed) return;
    wasBypassed = true;
    gen.panic (events);
    preview.reset();
    const double now = juce::Time::getMillisecondCounterHiRes();
    for (auto& e : events)
    {
        midi.addEvent (e.data, e.size, 0);
        if (e.lane < 0) queueDirect (now, -1, e.data, e.size);
        else if (const int slot = direct.slotForLane (e.lane); slot >= 0) queueDirect (now, slot, e.data, e.size);
    }
    flushDirect();
}

void OrreryProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const double blockStartMs = juce::Time::getMillisecondCounterHiRes();
    midi.clear();   // la entrada MIDI no se usa (por ahora) y no se reenvía
    if (n <= 0) return;
    wasBypassed = false;

    const auto& X = Ix::get();
    for (size_t k = 0; k < raw.size(); ++k) vals[k] = raw[k]->load();
    applyParams (vals.data(), gen);

    // Vínculo entre instancias.
    const int lm = (int) std::lround (vals[(size_t) X.link]);
    if (lm != linkMode) { if (linkMode == 1) LinkHub::get().leaders.fetch_sub (1); if (lm == 1) LinkHub::get().leaders.fetch_add (1); linkMode = lm; }
    gen.following = false;
    if (linkMode == 2 && LinkHub::get().leaders.load() > 0)
    {
        Harmony h;
        if (LinkHub::get().read (h) && h.serial != 0) { gen.following = true; gen.external = h; gen.g.key = h.key; gen.g.scale = h.scale; }
    }

    bool playing = false; double ppq = 0.0, bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
            if (auto b = pos->getBpm()) bpm = *b;
        }

    if (panicRequest.exchange (false)) { gen.panic (events); preview.reset(); }
    else gen.process (n, playing, ppq, bpm, events);
    if (linkMode == 1) LinkHub::get().publish (gen.published());

    // Salidas: host, puertos directos, sonido de prueba, chispas del editor.
    std::array<int, kLanes> roles {};
    for (int l = 0; l < kLanes; ++l) roles[(size_t) l] = gen.lanes[(size_t) l].role;
    const bool previewOn = vals[(size_t) X.preview] > 0.5f;
    if (! previewOn && previewWasOn) preview.reset();     // apagado con notas sonando: no recibiría sus note-off
    previewWasOn = previewOn;
    buffer.clear();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    // Pista mono: la vista previa se mezcla a mono (antes sólo sonaba si había canal derecho).
    if ((int) monoR.size() < n) monoR.resize ((size_t) n);
    if (R == nullptr) std::fill (monoR.begin(), monoR.begin() + n, 0.0f);
    float* rr = R != nullptr ? R : monoR.data();
    int done = 0;
    const float pvol = vals[(size_t) X.previewVol], prev = vals[(size_t) X.previewRev];
    for (auto& e : events)
    {
        const int off = juce::jlimit (0, n - 1, e.offset);
        midi.addEvent (e.data, e.size, off);
        if (e.lane >= 0)
        {
            const int slot = direct.slotForLane (e.lane);
            if (slot >= 0) queueDirect (blockStartMs + off * 1000.0 / sr, slot, e.data, e.size);
        }
        else queueDirect (blockStartMs + off * 1000.0 / sr, -1, e.data, e.size);
        if ((e.data[0] & 0xF0) == 0x90 && e.data[2] > 0 && e.lane >= 0)
        {
            const auto w = sparkFifo.write (1);
            const Spark s { e.lane, e.data[1], e.data[2] };
            if (w.blockSize1 > 0) sparks[(size_t) w.startIndex1] = s; else if (w.blockSize2 > 0) sparks[(size_t) w.startIndex2] = s;
            midiOutFlash.store (3);
        }
        if (previewOn && rr != nullptr)
        {
            if (off > done) { preview.render (L, rr, done, off, pvol, prev); done = off; }
            preview.handle (e, roles);
        }
    }
    if (previewOn && rr != nullptr && done < n) preview.render (L, rr, done, n, pvol, prev);
    if (R == nullptr) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + monoR[(size_t) i]);

    // Reloj MIDI (24 por negra) y start/stop hacia los puertos directos.
    const bool running = gen.isRunning();
    if (vals[(size_t) X.clockOut] > 0.5f && direct.anyOpen())
    {
        const double perSample = (playing && gen.g.hostClock ? bpm : (double) gen.g.tempo) / 60.0 / sr;
        if (running && ! wasRunning) { const uint8_t st = 0xFA; queueDirect (blockStartMs, -1, &st, 1); clockBeat = 0.0; }
        if (! running && wasRunning) { const uint8_t st = 0xFC; queueDirect (blockStartMs, -1, &st, 1); }
        if (running)
        {
            const double b1 = clockBeat + perSample * n;
            for (double t = std::ceil (clockBeat * 24.0) / 24.0; t < b1; t += 1.0 / 24.0)
            {
                const uint8_t c = 0xF8;
                queueDirect (blockStartMs + (t - clockBeat) / perSample * 1000.0 / sr, -1, &c, 1);
            }
            clockBeat = b1;
        }
    }
    wasRunning = running;
    flushDirect();
    for (int l = 0; l < kLanes; ++l) direct.setLaneChannel (l, gen.lanes[(size_t) l].channel);
}

void OrreryProcessor::notifyUi (std::function<void()>& cb)
{
    if (! cb) return;
    if (juce::MessageManager::existsAndIsCurrentThread()) { cb(); return; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<OrreryProcessor> (this), &cb] { if (safe != nullptr && cb) cb(); });
}

bool OrreryProcessor::newVariation()
{
    setValue ("seed", (float) juce::Random::getSystemRandom().nextInt ({ 1, 9999 }));
    return true;
}

juce::File OrreryProcessor::captureToFile (double minutes, const juce::File& dir) const
{
    const int head = gen.captureHead.load();
    const int cap = (int) gen.capture.size();
    const double bpm = gen.captureBpm;
    if (head == 0) return {};
    double lastBeat = 0.0;
    for (int i = std::max (0, head - cap); i < head; ++i) lastBeat = std::max (lastBeat, gen.capture[(size_t) (i % cap)].beat);
    const double fromBeat = lastBeat - minutes * bpm;
    juce::MidiFile mf; mf.setTicksPerQuarterNote (480);
    std::array<juce::MidiMessageSequence, 16> tracks;
    double first = 1e18;
    for (int i = std::max (0, head - cap); i < head; ++i) { const auto& c = gen.capture[(size_t) (i % cap)]; if (c.beat >= fromBeat) first = std::min (first, c.beat); }
    for (int i = std::max (0, head - cap); i < head; ++i)
    {
        const auto& c = gen.capture[(size_t) (i % cap)];
        if (c.beat < fromBeat || c.ch < 1 || c.ch > 16) continue;
        const double t0 = (c.beat - first) * 480.0, t1 = t0 + c.dur * 480.0;
        tracks[(size_t) c.ch - 1].addEvent (juce::MidiMessage::noteOn (c.ch, c.note, (juce::uint8) c.vel), t0);
        tracks[(size_t) c.ch - 1].addEvent (juce::MidiMessage::noteOff (c.ch, c.note), t1);
    }
    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);
    mf.addTrack (tempo);
    for (auto& t : tracks) if (t.getNumEvents() > 0) { t.updateMatchedPairs(); mf.addTrack (t); }
    dir.createDirectory();
    const auto file = dir.getNonexistentChildFile ("Orrery " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M"), ".mid", false);
    juce::FileOutputStream os (file);
    if (! os.openedOk() || ! mf.writeTo (os)) return {};
    return file;
}

// ───────────── presets ─────────────
void OrreryProcessor::loadFactory()
{
   #if ORRERY_HAS_ASSETS
    juce::StringArray names;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String nm (BinaryData::namedResourceList[i]);
        if (nm.endsWith ("_json") && nm.startsWith ("p") && nm.substring (1, 3).containsOnly ("0123456789")) names.add (nm);
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

juce::File OrreryProcessor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/Orrery/Presets");
   #else
    return base.getChildFile ("Audio Tools/Orrery/Presets");
   #endif
}

void OrreryProcessor::rescanUserPresets()
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

int OrreryProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void OrreryProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String OrreryProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void OrreryProcessor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    const auto& v = presets[(size_t) index].data;
    const auto* obj = v.getProperty ("params", {}).getDynamicObject();
    for (auto& e : params())
    {
        if (e.global) continue;
        setValue (e.id, obj != nullptr && obj->hasProperty (e.id) ? (float) obj->getProperty (e.id) : e.def);
    }
    currentPresetName = presets[(size_t) index].name;
    tracker.capture (*this);
    gen.resetRequest.store (true);      // la sesión nueva la arma el hilo de audio (antes era una carrera con process())
    notifyUi (onPresetChanged);
}

bool OrreryProcessor::saveUserPreset (const juce::String& name)
{
    const auto dir = userPresetDir();
    dir.createDirectory();
    auto* o = new juce::DynamicObject(); auto* prm = new juce::DynamicObject();
    o->setProperty ("name", name); o->setProperty ("category", "User");
    const auto& ps = params();
    for (size_t k = 0; k < ps.size(); ++k) if (! ps[k].global && std::abs (raw[k]->load() - ps[k].def) > 1e-4f) prm->setProperty (ps[k].id, raw[k]->load());
    o->setProperty ("params", juce::var (prm));
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".json");
    const bool ok = file.replaceWithText (juce::JSON::toString (juce::var (o)));
    rescanUserPresets();
    currentPresetName = name;
    tracker.capture (*this);
    for (int i = 0; i < (int) presets.size(); ++i) if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) currentProgram = i;
    notifyUi (onPresetChanged);
    return ok;
}

// ───────────── estado ─────────────
void OrreryProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    juce::StringArray ports;
    for (int l = 0; l < kLanes; ++l) ports.add (direct.lanePort (l));
    state.setProperty ("ports", ports.joinIntoString ("\n"), nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void OrreryProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    currentProgram = state.getProperty ("program", 0);
    const auto ports = juce::StringArray::fromLines (state.getProperty ("ports", "").toString());
    const auto snapshot = state.getProperty ("presetSnapshot").toString();
    for (auto id : { "presetName", "program", "ports", "presetSnapshot" }) state.removeProperty (id, nullptr);
    apvts.replaceState (state);
    juce::MessageManager::callAsync ([safe = juce::WeakReference<OrreryProcessor> (this), ports] {
        if (safe == nullptr) return;
        for (int l = 0; l < kLanes && l < ports.size(); ++l) safe->direct.setLanePort (l, ports[l]);
    });
    if (snapshot.isNotEmpty()) tracker.fromString (*this, snapshot);
    else tracker.capture (*this);
    notifyUi (onPresetChanged);
}

#if ! ORRERY_NO_EDITOR
juce::AudioProcessorEditor* OrreryProcessor::createEditor() { return new OrreryEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OrreryProcessor(); }
#else
juce::AudioProcessorEditor* OrreryProcessor::createEditor() { return nullptr; }
#endif
