#include "PluginProcessor.h"
#if ! VALENCE_NO_EDITOR
 #include "PluginEditor.h"
#endif

using namespace dh::val;

// ───────────── salidas MIDI directas ─────────────
juce::MidiOutput* DirectMidi::outputFor (int slot) const
{
    if (juce::isPositiveAndBelow (slot, kSlots)) return slots[(size_t) slot].get();
    if (juce::isPositiveAndBelow (slot - kSlots, kLayers)) return virt[(size_t) (slot - kSlots)].get();
    return nullptr;
}

void DirectMidi::allNotesOffOn (int slot, int channel)
{
    auto* o = outputFor (slot);
    if (o == nullptr) return;
    const int ch = juce::jlimit (1, 16, channel);
    o->sendMessageNow (juce::MidiMessage::controllerEvent (ch, 64, 0));
    o->sendMessageNow (juce::MidiMessage::allNotesOff (ch));
    o->sendMessageNow (juce::MidiMessage::allSoundOff (ch));
}

DirectMidi::~DirectMidi()
{
    stopThread (2000);
    // Al cerrar, cada sinte recibe All Notes Off en el canal de su capa (si no, las notas quedaban colgadas).
    const juce::ScopedLock sl (sendLock);
    for (int l = 0; l < kLayers; ++l) allNotesOffOn (layerSlot[(size_t) l].load(), layerChannel[(size_t) l].load());
    for (int l = 0; l < kLayers; ++l) { if (virtOn[(size_t) l].load()) allNotesOffOn (kSlots + l, layerChannel[(size_t) l].load()); virtOn[(size_t) l].store (false); virt[(size_t) l].reset(); }
}

// Un puerto virtual por capa. Si un nombre ya existe (otra Valence, otro programa), el sistema no lo crea: el que llama
// elige nombres libres. Los que se cierran reciben antes All Notes Off en el canal de su capa.
int DirectMidi::setVirtualPorts (const juce::StringArray& names)
{
    const juce::ScopedLock sl (sendLock);
    for (int l = 0; l < kLayers; ++l)
        if (virt[(size_t) l] != nullptr)
        {
            virtOn[(size_t) l].store (false);
            allNotesOffOn (kSlots + l, layerChannel[(size_t) l].load());
            virt[(size_t) l].reset();
        }
    virtNames.clear();
    int made = 0;
    if (names.size() >= kLayers)
        for (int l = 0; l < kLayers; ++l)
        {
            virt[(size_t) l] = juce::MidiOutput::createNewDevice (names[l]);
            virtNames.add (virt[(size_t) l] != nullptr ? names[l] : juce::String());
            if (virt[(size_t) l] != nullptr) { virtOn[(size_t) l].store (true); ++made; }
        }
    return made;
}

void DirectMidi::setLayerPort (int layer, const juce::String& identifier)
{
    if (! juce::isPositiveAndBelow (layer, kLayers)) return;
    layerPorts[(size_t) layer] = identifier;
    const juce::ScopedLock sl (sendLock);
    if (const int old = layerSlot[(size_t) layer].load(); old >= 0 && slotIds[(size_t) old] != identifier) allNotesOffOn (old, layerChannel[(size_t) layer].load());
    std::array<bool, kSlots> used {};
    int slot = -1;
    if (identifier.isNotEmpty())
    {
        for (int s = 0; s < kSlots; ++s) if (slotIds[(size_t) s] == identifier && slots[(size_t) s] != nullptr) slot = s;
        if (slot < 0)
            for (int s = 0; s < kSlots && slot < 0; ++s)
            {
                bool inUse = false;
                for (int l = 0; l < kLayers; ++l) if (l != layer && layerSlot[(size_t) l].load() == s) inUse = true;
                if (! inUse)
                {
                    slots[(size_t) s] = juce::MidiOutput::openDevice (identifier);
                    slotIds[(size_t) s] = slots[(size_t) s] != nullptr ? identifier : juce::String();
                    if (slots[(size_t) s] != nullptr) slot = s;
                }
            }
    }
    layerSlot[(size_t) layer].store (slot);
    for (int l = 0; l < kLayers; ++l) if (layerSlot[(size_t) l].load() >= 0) used[(size_t) layerSlot[(size_t) l].load()] = true;
    for (int s = 0; s < kSlots; ++s) if (! used[(size_t) s] && slots[(size_t) s] != nullptr) { slots[(size_t) s].reset(); slotIds[(size_t) s] = {}; }
}

void DirectMidi::reopenMissing()
{
    for (int l = 0; l < kLayers; ++l)
        if (layerPorts[(size_t) l].isNotEmpty() && layerSlot[(size_t) l].load() < 0) setLayerPort (l, layerPorts[(size_t) l]);
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
    for (int l = 0; l < kLayers; ++l)
    {
        const int s = layerSlot[(size_t) l].load (std::memory_order_relaxed);
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
        for (double wait_ = m.at - juce::Time::getMillisecondCounterHiRes(); wait_ > 1.0 && ! threadShouldExit();
             wait_ = m.at - juce::Time::getMillisecondCounterHiRes())
            juce::Thread::sleep (juce::jmin (50, (int) wait_));
        const juce::ScopedLock sl (sendLock);
        if (auto* o = outputFor ((int) m.slot)) o->sendMessageNow (juce::MidiMessage (m.data, m.size));
    }
}

// ───────────── procesador ─────────────
namespace {
// Instancias vivas (para reconocer una copia de estado: Duplicate Track en Live) y nombres de puertos en uso.
juce::CriticalSection registryLock;
juce::StringArray& liveIds() { static juce::StringArray a; return a; }
juce::StringArray& portNamesInUse() { static juce::StringArray a; return a; }
}

juce::AudioProcessor::BusesProperties ValenceProcessor::buses()
{
   #if JucePlugin_IsMidiEffect
    return BusesProperties();          // AU MIDI FX (Logic): sin audio; el tiempo sale del playhead
   #else
    return BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #endif
}

const juce::StringArray& ValenceProcessor::groups()
{
    static juce::StringArray g { "Minimal Techno", "Minimal House", "Drum and Bass", "Acid", "Dub", "Ambient", "Classics" };
    return g;
}

ValenceProcessor::ValenceProcessor()
    : AudioProcessor (buses()), apvts (*this, nullptr, "Valence", createLayout())
{
    for (auto& p : params()) raw.push_back (apvts.getRawParameterValue (p.id));
    vals.assign (raw.size(), 0.0f);
    events.reserve (8192); inEvents.reserve (2048); directQ.reserve (8192);
    loadFactory();
    rescanUserPresets();
    { juce::StringArray g; for (auto& e : params()) if (e.global) g.add (e.id); tracker.ignore (g); }
    // Una instancia nueva es su programa 0 (lo que el host informa como programa actual; QA en REAPER, X2).
    if (! presets.empty()) loadPreset (0); else tracker.capture (*this);
    { const juce::ScopedLock sl (registryLock); instanceId = juce::Uuid().toString(); liveIds().add (instanceId); }
    startTimerHz (4);
}

ValenceProcessor::~ValenceProcessor()
{
    stopTimer();
    if (linkMode == 1) LinkHub::get().leaders.fetch_sub (1);
    const juce::ScopedLock sl (registryLock);
    liveIds().removeString (instanceId);
    const auto names = direct.virtualPortNames();
    direct.setVirtualPorts ({});
    for (auto& n : names) portNamesInUse().removeString (n);
}

juce::AudioProcessorValueTreeState::ParameterLayout ValenceProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& e : params())
    {
        const juce::ParameterID pid { e.id, 1 };
        const auto* ep = &e;
        if (e.choices.isNotEmpty())
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, e.name, juce::StringArray::fromTokens (e.choices, "|", ""), (int) (e.def - e.lo),
                juce::AudioParameterChoiceAttributes().withAutomatable (e.automatable)));
        else if (e.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterInt> (pid, e.name, (int) e.lo, (int) e.hi, (int) e.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction ([ep] (int v, int) { return formatParam (*ep, (float) v); }).withAutomatable (e.automatable)));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, e.name, juce::NormalisableRange<float> (e.lo, e.hi), e.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([ep] (float v, int) { return formatParam (*ep, v); }).withAutomatable (e.automatable)));
    }
    return layout;
}

bool ValenceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    return layouts.getMainOutputChannelSet().isDisabled() || layouts.getMainOutputChannels() == 0;
   #else
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
   #endif
}

void ValenceProcessor::setValue (const juce::String& id, float v)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void ValenceProcessor::setRows (const Rows& r)
{
    { const juce::SpinLock::ScopedLockType l (rowsLock); rows = r; }
    rowsVersion.fetch_add (1);
}

void ValenceProcessor::setStep (int layer, int step, const Step& s)
{
    if (! juce::isPositiveAndBelow (layer, kLayers) || ! juce::isPositiveAndBelow (step, kMaxSteps)) return;
    { const juce::SpinLock::ScopedLockType l (rowsLock); rows[(size_t) layer][(size_t) step] = s; }
    rowsVersion.fetch_add (1);
    rowsEdited.store (true);
}

// Preparar (y el reset() del host) deja el plugin como recién creado: lo que sonaba sale con note-off en el bloque
// siguiente, el sonido de prueba sin cola. Así un render suena igual que una instancia nueva (athosttest repro).
void ValenceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    engine.prepare (sr);
    engine.reset();
    preview.prepare ((float) sr, std::max (64, samplesPerBlock) + 64);
    hostGap.prepare (sr);
    lastRunParam = false;          // Play encendido vuelve a ser una orden en el bloque siguiente
    housekeeping();
   #if JucePlugin_Build_AU
    // El AU arranca con el preset "Untitled" del host aunque la instancia nueva sea el programa 0: que muestre su nombre.
    if (wrapperType == wrapperType_AudioUnit) updateHostDisplay (ChangeDetails().withProgramChanged (true));
   #endif
}

void ValenceProcessor::reset()
{
    engine.reset();
    preview.reset();
    hostGap.reset();
    lastRunParam = false;
}

void ValenceProcessor::queueDirect (double at, int slot, const uint8_t* d, int size)
{
    if (directQ.size() >= directQ.capacity()) return;      // sin crecer en el hilo de audio
    DirectItem it; it.at = at; it.slot = slot; it.size = (uint8_t) std::min (3, size); std::memcpy (it.data, d, it.size);
    directQ.push_back (it);
}

// Encola los mensajes del bloque en orden de hora: la cola del hilo de salida es estrictamente FIFO.
void ValenceProcessor::flushDirect()
{
    std::stable_sort (directQ.begin(), directQ.end(), [] (const DirectItem& a, const DirectItem& b) { return a.at < b.at; });
    for (auto& it : directQ)
    {
        if (it.slot >= 0) direct.push (it.at, it.slot, it.data, it.size);
        else direct.pushToAll (it.at, it.data, it.size);
    }
    directQ.clear();
}

void ValenceProcessor::sendOut (juce::MidiBuffer& midi, const OutEvent& e, int n, double blockStartMs)
{
    const int off = juce::jlimit (0, n - 1, e.offset);
    midi.addEvent (e.data, e.size, off);
    const double at = blockStartMs + off * 1000.0 / sr;
    if (e.layer >= 0)
    {
        if (const int ds = direct.slotForLayer (e.layer); ds >= 0) queueDirect (at, ds, e.data, e.size);
        if (const int vs = direct.virtualSlotForLayer (e.layer); vs >= 0) queueDirect (at, vs, e.data, e.size);
    }
    else
    {
        queueDirect (at, -1, e.data, e.size);
        for (int l = 0; l < kLayers; ++l)        // pánico y reenvío: también a los puertos virtuales de ese canal
            if (const int vs = direct.virtualSlotForLayer (l); vs >= 0 && (engine.g.perLayerChannels ? juce::jlimit (1, 16, engine.layers[(size_t) l].channel) - 1 : 0) == (e.data[0] & 0x0F))
                queueDirect (at, vs, e.data, e.size);
    }
}

// En bypass: al entrar se cierran las notas (al host y a los puertos directos); la entrada no pasa.
void ValenceProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();
    if (wasBypassed) return;
    wasBypassed = true;
    engine.releaseNow (events);
    preview.reset();
    const double now = juce::Time::getMillisecondCounterHiRes();
    for (auto& e : events) sendOut (midi, e, std::max (1, buffer.getNumSamples()), now);
    flushDirect();
}

void ValenceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const double blockStartMs = juce::Time::getMillisecondCounterHiRes();
    if (n <= 0) { midi.clear(); return; }
    wasBypassed = false;
    // REAPER no llama a un plugin en bypass: los note-off de la entrada que llegaron mientras tanto se perdieron.
    if (hostGap.check (getPlayHead(), n, isNonRealtime())) { engine.releaseAll(); preview.reset(); }

    const auto& X = Ix::get();
    for (size_t k = 0; k < raw.size(); ++k) vals[k] = raw[k]->load();
    // Play y Stop: cada cambio del parámetro es una orden; lo que hace el transporte (con el reloj del host, arrancarlo
    // es Play y pararlo es Stop) lo hace el motor y se refleja en el parámetro sin volver como orden.
    int runCmd = 0;
    if (const bool rp = vals[(size_t) X.run] > 0.5f; rp != lastRunParam) { runCmd = rp ? 1 : 2; lastRunParam = rp; }
    applyParams (vals.data(), engine.g, engine.layers);
    engine.g.cmdMode = true; engine.g.runCmd = runCmd;
    if (const uint32_t v = rowsVersion.load(); v != rowsApplied)
    {
        const juce::SpinLock::ScopedTryLockType l (rowsLock);
        if (l.isLocked())
        {
            for (int k = 0; k < kLayers; ++k) { engine.layers[(size_t) k].steps = rows[(size_t) k]; engine.layers[(size_t) k].rowsVersion = v; }
            rowsApplied = v;
        }
    }

    Transport tr;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            tr.playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) tr.ppq = *q;
            if (auto b = pos->getBpm()) tr.bpm = lastHostBpm = *b;
        }
    // Sin tempo del host en este bloque (sin playhead, o un AU MIDI FX al que Logic no se lo da): el último conocido;
    // si nunca hubo, el tempo propio (Transport::bpm 0).
    if (tr.bpm <= 0.0) tr.bpm = lastHostBpm;

    // La entrada: notas, pedal y lo demás, cada una en su muestra. No pasa tal cual: el motor reenvía lo que no son notas.
    inEvents.clear();
    for (const auto meta : midi)
    {
        if (inEvents.size() >= inEvents.capacity()) break;
        const auto* d = meta.data;
        if (meta.numBytes < 1 || meta.numBytes > 3 || d[0] < 0x80 || d[0] >= 0xF0) continue;
        InEvent e; e.offset = juce::jlimit (0, n - 1, meta.samplePosition); e.size = (uint8_t) meta.numBytes;
        e.data[0] = d[0]; e.data[1] = meta.numBytes > 1 ? d[1] : 0; e.data[2] = meta.numBytes > 2 ? d[2] : 0;
        inEvents.push_back (e);
    }
    midi.clear();

    // Link: la líder publica su acorde; una seguidora toca sobre él.
    const int lm = (int) std::lround (vals[(size_t) X.link]);
    if (lm != linkMode) { if (linkMode == 1) LinkHub::get().leaders.fetch_sub (1); if (lm == 1) LinkHub::get().leaders.fetch_add (1); linkMode = lm; }
    engine.following = false;
    if (linkMode == 2 && LinkHub::get().leaders.load() > 0)
    {
        ChordShare c;
        if (LinkHub::get().read (c) && c.serial != 0) { engine.following = true; engine.external = c; }
    }
    if (resetPending.exchange (false)) engine.resetRequest.store (true);

    events.clear();
    if (panicRequest.exchange (false))
    {
        engine.panic (events);
        preview.reset();
        for (auto& e : events) sendOut (midi, e, n, blockStartMs);
    }
    engine.process (n, tr, inEvents.data(), (int) inEvents.size(), events);
    if (engine.lastHostEdge() != 0) mirrorRun (engine.lastHostEdge() > 0);
    if (linkMode == 1) LinkHub::get().publish (engine.published());

    // Salidas: host, puertos directos y virtuales, sonido de prueba, chispas de la interfaz.
    // El AU MIDI FX no suena: su buffer (el wrapper igual lo pasa) sale en silencio.
    const bool previewOn = ! JucePlugin_IsMidiEffect && vals[(size_t) X.preview] > 0.5f && buffer.getNumChannels() > 0;
    if (! previewOn && previewWasOn) preview.reset();
    previewWasOn = previewOn;
    buffer.clear();
    const auto voices = voicesOf (vals.data());
    float* L = buffer.getNumChannels() > 0 ? buffer.getWritePointer (0) : nullptr;
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    int done = 0;
    const float pvol = vals[(size_t) X.previewVol], pspace = vals[(size_t) X.previewSpace];
    for (auto& e : events)
    {
        sendOut (midi, e, n, blockStartMs);
        const int off = juce::jlimit (0, n - 1, e.offset);
        if ((e.data[0] & 0xF0) == 0x90 && e.data[2] > 0 && e.layer >= 0)
        {
            const auto w = sparkFifo.write (1);
            const Spark s { e.layer, e.data[1], e.data[2], e.nucleon, e.src, e.step, e.flags };
            if (w.blockSize1 > 0) sparks[(size_t) w.startIndex1] = s; else if (w.blockSize2 > 0) sparks[(size_t) w.startIndex2] = s;
            midiOutFlash.store (3);
        }
        if (previewOn)
        {
            if (off > done) { preview.render (L, R != nullptr ? R : L, done, off, pvol, pspace); done = off; }
            preview.handle (e, voices);
        }
    }
    if (previewOn && done < n) preview.render (L, R != nullptr ? R : L, done, n, pvol, pspace);
    if (previewOn && R == nullptr) for (int i = 0; i < n; ++i) L[i] *= 0.5f;     // pista mono: la suma de los dos lados

    // Reloj MIDI (24 por negra, start y stop) a los puertos directos, en la muestra que da el motor.
    if (vals[(size_t) X.clockOut] > 0.5f && direct.anyOpen())
        for (auto& c : engine.clock) queueDirect (blockStartMs + c.offset * 1000.0 / sr, -1, &c.byte, 1);
    flushDirect();
    for (int l = 0; l < kLayers; ++l) direct.setLayerChannel (l, engine.g.perLayerChannels ? engine.layers[(size_t) l].channel : 1);
}

// El transporte arrancó o paró el motor: el botón Play (y el host) lo muestran, desde el hilo de audio en el mismo
// bloque, así el valor nuevo no se lee como una orden en el bloque siguiente.
void ValenceProcessor::mirrorRun (bool on)
{
    lastRunParam = on;
    if (auto* p = apvts.getParameter ("run"); p != nullptr && (p->getValue() > 0.5f) != on) p->setValueNotifyingHost (on ? 1.0f : 0.0f);
}

bool ValenceProcessor::virtualPortsSupported()
{
   #if JUCE_MAC || JUCE_LINUX
    return true;
   #else
    return false;
   #endif
}

// Hilo de mensajes: los puertos virtuales ("Valence Layer 1".. "Valence Layer 4"; otra Valence que también publica usa
// "Valence 2 Layer 1"...).
void ValenceProcessor::housekeeping()
{
    if (! juce::MessageManager::existsAndIsCurrentThread()) return;
    const bool want = virtualPortsSupported() && raw[(size_t) Ix::get().midiPorts]->load() > 0.5f;
    const bool have = direct.virtualPortsOpen() || direct.virtualPortNames().size() > 0;
    if (want == have) return;
    const juce::ScopedLock sl (registryLock);
    for (auto& nm : direct.virtualPortNames()) portNamesInUse().removeString (nm);
    if (! want) { direct.setVirtualPorts ({}); return; }
    for (int k = 1; k <= 16; ++k)
    {
        juce::StringArray names;
        const juce::String prefix = k == 1 ? juce::String ("Valence") : "Valence " + juce::String (k);
        for (int l = 0; l < kLayers; ++l) names.add (prefix + " Layer " + juce::String (l + 1));
        bool clash = false;
        for (auto& nm : names) clash = clash || portNamesInUse().contains (nm);
        if (clash) continue;
        const int made = direct.setVirtualPorts (names);
        if (made == kLayers) { portNamesInUse().addArray (names); return; }
        direct.setVirtualPorts ({});
        if (made == 0 && k > 2) break;      // el sistema no crea puertos (sin servidor MIDI)
    }
}

void ValenceProcessor::notifyUi (std::function<void()>& cb)
{
    if (! cb) return;
    if (juce::MessageManager::existsAndIsCurrentThread()) { cb(); return; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<ValenceProcessor> (this), &cb] { if (safe != nullptr && cb) cb(); });
}

bool ValenceProcessor::newVariation()
{
    setValue ("seed", (float) juce::Random::getSystemRandom().nextInt ({ 1, 9999 }));
    return true;
}

juce::File ValenceProcessor::captureToFile (double minutes, const juce::File& dir) const
{
    const int head = engine.captureHead.load();
    const int cap = (int) engine.capture.size();
    const double bpm = engine.captureBpm;
    if (head == 0) return {};
    double last = 0.0;
    for (int i = std::max (0, head - cap); i < head; ++i) last = std::max (last, engine.capture[(size_t) (i % cap)].sec);
    const double from = last - minutes * 60.0;
    double first = 1e18;
    for (int i = std::max (0, head - cap); i < head; ++i) { const auto& c = engine.capture[(size_t) (i % cap)]; if (c.sec >= from) first = std::min (first, c.sec); }
    juce::MidiFile mf; mf.setTicksPerQuarterNote (480);
    std::array<juce::MidiMessageSequence, kLayers> tracks;
    for (int i = std::max (0, head - cap); i < head; ++i)
    {
        const auto& c = engine.capture[(size_t) (i % cap)];
        if (c.sec < from || c.ch < 1 || c.ch > 16 || c.layer < 0 || c.layer >= kLayers) continue;
        const double t0 = (c.sec - first) * bpm / 60.0 * 480.0, t1 = t0 + c.durSec * bpm / 60.0 * 480.0;
        tracks[(size_t) c.layer].addEvent (juce::MidiMessage::noteOn (c.ch, c.note, (juce::uint8) c.vel), t0);
        tracks[(size_t) c.layer].addEvent (juce::MidiMessage::noteOff (c.ch, c.note), t1);
    }
    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);
    mf.addTrack (tempo);
    for (int l = 0; l < kLayers; ++l)
        if (tracks[(size_t) l].getNumEvents() > 0)
        {
            tracks[(size_t) l].addEvent (juce::MidiMessage::textMetaEvent (3, "Layer " + juce::String (l + 1)), 0.0);
            tracks[(size_t) l].updateMatchedPairs(); mf.addTrack (tracks[(size_t) l]);
        }
    dir.createDirectory();
    const auto file = dir.getNonexistentChildFile ("Valence " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M"), ".mid", false);
    juce::FileOutputStream os (file);
    if (! os.openedOk() || ! mf.writeTo (os)) return {};
    return file;
}

// ───────────── presets ─────────────
void ValenceProcessor::loadFactory()
{
   #if VALENCE_HAS_ASSETS
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

juce::File ValenceProcessor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/Valence/Presets");
   #else
    return base.getChildFile ("Audio Tools/Valence/Presets");
   #endif
}

void ValenceProcessor::rescanUserPresets()
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

int ValenceProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void ValenceProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String ValenceProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void ValenceProcessor::loadPreset (int index)
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
    setRows (rowsFromVar (v.getProperty ("rows", {})));
    rowsEdited.store (false);
    currentPresetName = presets[(size_t) index].name;
    tracker.capture (*this);
    resetPending.store (true);
    notifyUi (onPresetChanged);
}

bool ValenceProcessor::saveUserPreset (const juce::String& name)
{
    const auto dir = userPresetDir();
    dir.createDirectory();
    auto* o = new juce::DynamicObject(); auto* prm = new juce::DynamicObject(); auto* rv = new juce::DynamicObject();
    o->setProperty ("name", name); o->setProperty ("category", "User");
    const auto& ps = params();
    for (size_t k = 0; k < ps.size(); ++k) if (! ps[k].global && std::abs (raw[k]->load() - ps[k].def) > 1e-4f) prm->setProperty (ps[k].id, raw[k]->load());
    o->setProperty ("params", juce::var (prm));
    const auto r = getRows();
    for (int l = 0; l < kLayers; ++l)
    {
        auto* lo = new juce::DynamicObject();
        auto row = [&] (const char* id, auto&& get) { juce::Array<juce::var> a; for (int k = 0; k < kMaxSteps; ++k) a.add (get (r[(size_t) l][(size_t) k])); lo->setProperty (id, a); };
        row ("on", [] (const Step& s) { return (int) s.on; }); row ("vel", [] (const Step& s) { return (int) s.vel; });
        row ("accent", [] (const Step& s) { return (int) s.accent; }); row ("tie", [] (const Step& s) { return (int) s.tie; });
        row ("ratchet", [] (const Step& s) { return (int) s.ratchet; }); row ("prob", [] (const Step& s) { return (int) s.prob; });
        row ("oct", [] (const Step& s) { return (int) s.oct; }); row ("fixed", [] (const Step& s) { return (int) s.fixed; });
        row ("cc", [] (const Step& s) { return (int) s.cc; }); row ("finger", [] (const Step& s) { return (int) s.finger; });
        rv->setProperty ("l" + juce::String (l + 1), juce::var (lo));
    }
    o->setProperty ("rows", juce::var (rv));
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".json");
    const bool ok = file.replaceWithText (juce::JSON::toString (juce::var (o)));
    rescanUserPresets();
    currentPresetName = name;
    tracker.capture (*this);
    rowsEdited.store (false);
    for (int i = 0; i < (int) presets.size(); ++i) if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) currentProgram = i;
    notifyUi (onPresetChanged);
    return ok;
}

// ───────────── estado ─────────────
void ValenceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    juce::StringArray ports;
    for (int l = 0; l < kLayers; ++l) ports.add (direct.layerPort (l));
    state.setProperty ("ports", ports.joinIntoString ("\n"), nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    state.setProperty ("rowsEdited", rowsEdited.load() ? 1 : 0, nullptr);
    const auto r = getRows();
    for (int l = 0; l < kLayers; ++l) state.setProperty ("rows" + juce::String (l + 1), rowsToString (r[(size_t) l]), nullptr);
    { const juce::ScopedLock sl (registryLock); state.setProperty ("instanceId", instanceId, nullptr); }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ValenceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    Rows r = defaultRows();
    for (int l = 0; l < kLayers; ++l)
        if (state.hasProperty ("rows" + juce::String (l + 1))) r[(size_t) l] = rowsFromString (state.getProperty ("rows" + juce::String (l + 1)).toString());
    // Un archivo de preset (.vstpreset, para el navegador de Live): el estado de un programa sin instanceId. Se aplica
    // como un preset del menú: cambia la música y no toca Play, el reloj, los puertos ni lo global.
    if (! state.hasProperty ("instanceId"))
    {
        for (auto& e : params())
        {
            if (e.global) continue;
            const auto c = state.getChildWithProperty ("id", e.id);
            setValue (e.id, c.isValid() ? (float) c.getProperty ("value", e.def) : e.def);
        }
        setRows (r);
        rowsEdited.store (false);
        tracker.capture (*this);
        resetPending.store (true);
        notifyUi (onPresetChanged);
        return;
    }
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    currentProgram = state.getProperty ("program", 0);
    const auto ports = juce::StringArray::fromLines (state.getProperty ("ports", "").toString());
    const auto snapshot = state.getProperty ("presetSnapshot").toString();
    const bool edited = (int) state.getProperty ("rowsEdited", 0) != 0;
    const juce::String savedId = state.getProperty ("instanceId").toString();
    for (auto id : { "presetName", "program", "ports", "presetSnapshot", "rowsEdited", "instanceId", "rows1", "rows2", "rows3", "rows4" }) state.removeProperty (id, nullptr);
    // ¿Es una copia? Si otra Valence viva ya tiene ese id, este estado se copió de ella (Duplicate Track): la copia
    // recibe un id nuevo. Si no, es la misma instancia (deshacer) o un proyecto que se abre: se queda con su id.
    {
        const juce::ScopedLock sl (registryLock);
        const bool copy = savedId.isNotEmpty() && savedId != instanceId && liveIds().contains (savedId);
        if (savedId.isNotEmpty() && ! copy) { liveIds().removeString (instanceId); instanceId = savedId; liveIds().add (instanceId); }
    }
    apvts.replaceState (state);
    setRows (r);
    rowsEdited.store (edited);
    juce::MessageManager::callAsync ([safe = juce::WeakReference<ValenceProcessor> (this), ports] {
        if (safe == nullptr) return;
        for (int l = 0; l < kLayers && l < ports.size(); ++l) safe->direct.setLayerPort (l, ports[l]);
    });
    if (snapshot.isNotEmpty()) tracker.fromString (*this, snapshot);
    else tracker.capture (*this);
    notifyUi (onPresetChanged);
}

#if ! VALENCE_NO_EDITOR
juce::AudioProcessorEditor* ValenceProcessor::createEditor() { return new ValenceEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ValenceProcessor(); }
#else
juce::AudioProcessorEditor* ValenceProcessor::createEditor() { return nullptr; }
#endif
