#include "PluginProcessor.h"
#if ! ORRERY_NO_EDITOR
 #include "PluginEditor.h"
#endif

using namespace dh::orr;

// ───────────── salidas MIDI directas ─────────────
juce::MidiOutput* DirectMidi::outputFor (int slot) const
{
    if (juce::isPositiveAndBelow (slot, kSlots)) return slots[(size_t) slot].get();
    if (juce::isPositiveAndBelow (slot - kSlots, kLanes)) return virt[(size_t) (slot - kSlots)].get();
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
    // Al cerrar, cada sinte recibe All Notes Off en el canal de su carril (si no, las notas quedaban colgadas).
    const juce::ScopedLock sl (sendLock);
    for (int l = 0; l < kLanes; ++l) allNotesOffOn (laneSlot[(size_t) l].load(), laneChannel[(size_t) l].load());
    for (int l = 0; l < kLanes; ++l) { if (virtOn[(size_t) l].load()) allNotesOffOn (kSlots + l, laneChannel[(size_t) l].load()); virtOn[(size_t) l].store (false); virt[(size_t) l].reset(); }
}

// Un puerto virtual por carril. Si un nombre ya existe (otra Orrery, otro programa), CoreMIDI no lo crea: el que
// llama elige nombres libres. Los que se cierran reciben antes All Notes Off en el canal del carril.
int DirectMidi::setVirtualPorts (const juce::StringArray& names)
{
    const juce::ScopedLock sl (sendLock);
    for (int l = 0; l < kLanes; ++l)
        if (virt[(size_t) l] != nullptr)
        {
            virtOn[(size_t) l].store (false);
            allNotesOffOn (kSlots + l, laneChannel[(size_t) l].load());
            virt[(size_t) l].reset();
        }
    virtNames.clear();
    int made = 0;
    if (names.size() >= kLanes)
        for (int l = 0; l < kLanes; ++l)
        {
            virt[(size_t) l] = juce::MidiOutput::createNewDevice (names[l]);
            virtNames.add (virt[(size_t) l] != nullptr ? names[l] : juce::String());
            if (virt[(size_t) l] != nullptr) { virtOn[(size_t) l].store (true); ++made; }
        }
    return made;
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
        if (auto* o = outputFor ((int) m.slot)) o->sendMessageNow (juce::MidiMessage (m.data, m.size));
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
    // Una instancia nueva es su programa 0 (lo que el host informa como programa actual; QA en REAPER, X2).
    if (! presets.empty()) loadPreset (0); else tracker.capture (*this);
    instanceId = juce::Uuid().toString();
    genEvents.reserve (8192);
    regSlot = Constellation::get().join();
    { const std::lock_guard<std::mutex> lk (Constellation::get().lock); allInstances().push_back (this); }
    startTimerHz (4);
}

namespace {
// Nombres de los puertos virtuales en uso en este proceso (dos Orrery no pueden publicar el mismo).
juce::CriticalSection portNamesLock;
juce::StringArray& portNamesInUse() { static juce::StringArray a; return a; }
}

OrreryProcessor::~OrreryProcessor()
{
    stopTimer();
    if (linkMode == 1) LinkHub::get().leaders.fetch_sub (1);
    { const std::lock_guard<std::mutex> lk (Constellation::get().lock); auto& v = allInstances(); v.erase (std::remove (v.begin(), v.end(), this), v.end()); }
    Constellation::get().leave (regSlot);
    const auto names = direct.virtualPortNames();
    direct.setVirtualPorts ({});
    const juce::ScopedLock sl (portNamesLock);
    for (auto& n : names) portNamesInUse().removeString (n);
}

juce::AudioProcessorValueTreeState::ParameterLayout OrreryProcessor::createLayout()
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
    const bool rateChanged = ! preparedOnce || sampleRate != sr;
    preparedOnce = true;
    sr = sampleRate;
    gen.prepare (sampleRate);
    preview.prepare ((float) sampleRate, std::max (64, samplesPerBlock) + 64);
    monoR.assign ((size_t) std::max (64, samplesPerBlock) + 64, 0.0f);
    directQ.reserve (8192);
    updateRole (true);
    // Un Main que se vuelve a preparar a otra frecuencia empieza otra generación: sus satélites se resincronizan. Un
    // satélite que se prepara a otra frecuencia también, y suelta antes lo que suena en sus carriles. Con la misma
    // frecuencia (otro tamaño de bloque, el pase a render offline de REAPER) la generadora no cambia y los dos siguen a
    // la par: antes cada render perdía la sincronía, el satélite dejaba el acorde del drone sonando sin note-off y lo
    // volvía a atacar uno a tres bloques tarde (QA en REAPER 2026-10-22, re-test: 3 ataques en 21 ms, 2 de largo cero).
    if (rateChanged)
    {
        if (effRole.load() == RoleMain) Constellation::get().bumpGeneration (regSlot);
        satRelease.store (true);
    }
    if (juce::MessageManager::existsAndIsCurrentThread()) housekeeping();
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
// Un satélite suelta las de sus carriles y, al volver, se sincroniza de nuevo.
void OrreryProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();
    if (wasBypassed) return;
    wasBypassed = true;
    if (effRole.load() == RoleSatellite) { events.clear(); sat.releaseAll (events); }
    else gen.panic (events);
    preview.reset();
    const double now = juce::Time::getMillisecondCounterHiRes();
    for (auto& e : events)
    {
        midi.addEvent (e.data, e.size, 0);
        if (e.lane < 0) queueDirect (now, -1, e.data, e.size);
        else
        {
            if (const int slot = direct.slotForLane (e.lane); slot >= 0) queueDirect (now, slot, e.data, e.size);
            if (const int vs = direct.virtualSlotForLane (e.lane); vs >= 0) queueDirect (now, vs, e.data, e.size);
        }
    }
    flushDirect();
}

void OrreryProcessor::updateTrackProperties (const TrackProperties& p)
{
    trackName = p.name.value_or (juce::String());
    auto& reg = Constellation::get();
    const std::lock_guard<std::mutex> lk (reg.lock);
    if (regSlot >= 0) trackName.copyToUTF8 (reg.members[(size_t) regSlot].name, sizeof (reg.members[0].name) - 1);
}

// El rol efectivo: el elegido, o el que Auto resolvió (la primera del proceso es Main; las siguientes, satélites en
// el próximo carril libre). Con blocking = false (hilo de audio) sólo intenta tomar el registro.
void OrreryProcessor::updateRole (bool blocking)
{
    if (regSlot < 0) { effRole.store (RoleIndependent); return; }
    const auto& X = Ix::get();
    const int setting = juce::jlimit (0, 3, (int) std::lround (raw[(size_t) X.role]->load()));
    const uint32_t lp = (uint32_t) juce::jlimit (0, 255, (int) std::lround (raw[(size_t) X.satLanes]->load()));
    int want = setting == RoleAuto ? autoRole.load() : setting;
    uint32_t lanes = want == RoleSatellite ? (lp != 0 ? lp : autoLanes.load()) : 0u;
    if (want != 0 && want == regRole.load() && (want != RoleSatellite || (lanes != 0 && lanes == regLanes.load()))) return;
    auto& reg = Constellation::get();
    std::unique_lock<std::mutex> lk (reg.lock, std::defer_lock);
    if (blocking) lk.lock(); else if (! lk.try_lock()) return;
    bool becameSatellite = false;
    if (want == 0)
    {
        want = reg.resolveAutoLocked (regSlot);
        autoRole.store (want);
        becameSatellite = want == RoleSatellite;
    }
    if (want == RoleSatellite && lanes == 0)
    {
        lanes = lp != 0 ? lp : reg.pickLanesLocked (regSlot);
        if (lp == 0) autoLanes.store (lanes);
    }
    if (want == RoleMain)
    {
        uint32_t on = 0;
        for (int l = 0; l < kLanes; ++l) if (raw[(size_t) X.lane[(size_t) l].on]->load() > 0.5f) on |= 1u << l;
        reg.members[(size_t) regSlot].laneOn.store (on);
    }
    reg.setRoleLocked (regSlot, want, lanes);
    regRole.store (want); regLanes.store (lanes);
    effLanes.store (lanes);
    effRole.store (want);
    lk.unlock();
    // Una Orrery nueva que se vuelve satélite empieza sin el sonido de prueba: suena el sinte de su pista.
    if (becameSatellite && ! restoring.load()) setValue ("preview", 0.0f);
}

void OrreryProcessor::setRoleFromUi (int r)
{
    r = juce::jlimit (0, 3, r);
    if (r == RoleAuto) { autoRole.store (0); autoLanes.store (0); }
    if (r == RoleSatellite && effRole.load() != RoleSatellite) setValue ("preview", 0.0f);
    setValue ("role", (float) r);
    updateRole (true);
}

bool OrreryProcessor::virtualPortsSupported()
{
   #if JUCE_MAC || JUCE_LINUX
    return true;
   #else
    return false;
   #endif
}

// Hilo de mensajes: el rol y los puertos virtuales.
void OrreryProcessor::housekeeping()
{
    updateRole (true);
    const auto& X = Ix::get();
    const bool want = virtualPortsSupported() && raw[(size_t) X.midiPorts]->load() > 0.5f;
    const bool have = direct.virtualPortsOpen() || direct.virtualPortNames().size() > 0;
    if (want == have) return;
    const juce::ScopedLock sl (portNamesLock);
    for (auto& n : direct.virtualPortNames()) portNamesInUse().removeString (n);
    if (! want) { direct.setVirtualPorts ({}); return; }
    // "Orrery Drone", "Orrery Pad"...; dos carriles con el mismo rol llevan el número; otra Orrery que también
    // publica usa "Orrery 2 ...". Si el sistema no acepta un nombre (ya existe en otro programa), el siguiente.
    for (int k = 1; k <= 16; ++k)
    {
        juce::StringArray names;
        const juce::String prefix = k == 1 ? juce::String ("Orrery") : "Orrery " + juce::String (k);
        for (int l = 0; l < kLanes; ++l)
        {
            const int r = (int) std::lround (raw[(size_t) X.lane[(size_t) l].role]->load());
            int same = 0; for (int o = 0; o < kLanes; ++o) same += (int) std::lround (raw[(size_t) X.lane[(size_t) o].role]->load()) == r;
            names.add (prefix + " " + roleName (r) + (same > 1 ? " " + juce::String (l + 1) : juce::String()));
        }
        bool clash = false;
        for (auto& n : names) clash = clash || portNamesInUse().contains (n);
        if (clash) continue;
        const int made = direct.setVirtualPorts (names);
        if (made == kLanes) { portNamesInUse().addArray (names); return; }
        direct.setVirtualPorts ({});
        if (made == 0 && k > 2) break;      // el sistema no crea puertos (sin servidor MIDI): no seguir probando
    }
}

std::vector<OrreryProcessor::SatInfo> OrreryProcessor::satellites() const
{
    std::vector<SatInfo> v;
    auto& reg = Constellation::get();
    const std::lock_guard<std::mutex> lk (reg.lock);
    if (regSlot < 0) return v;
    auto* b = reg.members[(size_t) regSlot].bus.load();
    if (b == nullptr) return v;
    const double now = juce::Time::getMillisecondCounterHiRes();
    std::vector<std::pair<int, int>> order;
    for (int i = 0; i < Constellation::kMax; ++i)
    {
        const auto& m = reg.members[(size_t) i];
        if (i != regSlot && m.used.load() && m.role.load() == RoleSatellite && (m.follows.load() == b || m.follows.load() == nullptr)) order.push_back ({ m.order.load(), i });
    }
    std::sort (order.begin(), order.end());
    for (auto& [o, i] : order)
    {
        const auto& m = reg.members[(size_t) i];
        SatInfo s; s.lanes = m.lanes.load(); s.sync = m.sync.load();
        s.alive = m.follows.load() == b && now - m.seenMs.load() < 1500.0;
        s.name = juce::String::fromUTF8 (m.name);      // vacío si el host no lo dijo (Live sólo avisa al renombrar la pista)
        v.push_back (s);
    }
    return v;
}

juce::String OrreryProcessor::mainName() const
{
    auto& reg = Constellation::get();
    const std::lock_guard<std::mutex> lk (reg.lock);
    const int mi = reg.mainIndex (regSlot);
    if (mi < 0) return {};
    const auto n = juce::String::fromUTF8 (reg.members[(size_t) mi].name);
    return n.isNotEmpty() ? n : juce::String ("the Main Orrery");
}

bool OrreryProcessor::otherMainActive() const
{
    if (effRole.load() != RoleMain) return false;
    const int mi = Constellation::get().mainIndex();
    return mi >= 0 && mi != regSlot;
}

juce::String OrreryProcessor::displayName() const { return trackName.isNotEmpty() ? trackName : juce::String ("Orrery"); }

// Satélite → independiente: toma los ajustes musicales del Main (sólo sus carriles encendidos), así sigue sonando
// lo mismo y se puede editar todo.
void OrreryProcessor::makeIndependentFromMain()
{
    std::vector<float> mv;
    {
        auto& reg = Constellation::get();
        const std::lock_guard<std::mutex> lk (reg.lock);
        const int mi = reg.mainIndex (regSlot);
        if (mi >= 0)
            for (auto* p : allInstances())
                if (p != this && p->regSlot == mi) { mv.reserve (raw.size()); for (auto* r : p->raw) mv.push_back (r->load()); }
    }
    const uint32_t lanes = effLanes.load();
    const auto& ps = params();
    if (mv.size() == ps.size())
        for (size_t k = 0; k < ps.size(); ++k) if (! ps[k].global) setValue (ps[k].id, mv[k]);
    for (int l = 0; l < kLanes; ++l) if (lanes != 0 && ! ((lanes >> l) & 1u)) setValue ("l" + juce::String (l + 1) + "_on", 0.0f);
    setValue ("preview", 0.0f);
    setRoleFromUi (RoleIndependent);
    tracker.capture (*this);
    notifyUi (onPresetChanged);
}

std::vector<OrreryProcessor*>& OrreryProcessor::allInstances()
{
    static std::vector<OrreryProcessor*> v;     // con el lock del registro
    return v;
}

void OrreryProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const double blockStartMs = juce::Time::getMillisecondCounterHiRes();
    midi.clear();   // la entrada MIDI no se usa (por ahora) y no se reenvía
    if (n <= 0) return;
    wasBypassed = false;

    runDemo (demoBefore, n);
    const auto& X = Ix::get();
    for (size_t k = 0; k < raw.size(); ++k) vals[k] = raw[k]->load();
    updateRole (false);
    const int role = effRole.load();
    auto& reg = Constellation::get();
    // Play y Stop: cada cambio del parámetro (botón, automatización, estado cargado) es una orden para la generadora.
    // Lo que hace el transporte (con el reloj del host, arrancarlo es Play y pararlo es Stop) lo hace la generadora
    // misma, en el bloque en que pasa, y se refleja en el parámetro (mirrorRun) sin que ese eco vuelva como orden.
    int runCmd = 0;
    if (const bool rp = vals[(size_t) X.run] > 0.5f; rp != lastRunParam) { runCmd = rp ? 1 : 2; lastRunParam = rp; }

    bool playing = false; double ppq = 0.0, bpm = 120.0;
    Stamp stamp;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto t = pos->getTimeInSamples(); t && playing) { stamp.hostSample = *t; stamp.sampleValid = true; }
            if (auto h = pos->getHostTimeNs()) stamp.ns = (double) *h;
        }
    // Sin hora del host por ciclo: la de la máquina (ver Constellation.h).
    if (stamp.ns == 0.0) stamp.ns = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks()) * 1.0e9;

    // Cambio de rol: lo que sonaba se suelta.
    events.clear();
    if (role != lastRole)
    {
        if (lastRole == RoleSatellite) sat.releaseAll (events);
        else if ((lastRole == RoleIndependent || lastRole == RoleMain) && role == RoleSatellite) { gen.panic (genEvents); events.insert (events.end(), genEvents.begin(), genEvents.end()); }
        if (role != RoleMain) mainCore.attach (nullptr);
        lastRole = role;
    }

    // Vínculo entre instancias (sólo Independent sigue a una líder; un Main puede liderar).
    int lm = (int) std::lround (vals[(size_t) X.link]);
    if (role == RoleSatellite || role == 0 || (role == RoleMain && lm == 2)) lm = 0;
    if (lm != linkMode) { if (linkMode == 1) LinkHub::get().leaders.fetch_sub (1); if (lm == 1) LinkHub::get().leaders.fetch_add (1); linkMode = lm; }

    genEvents.clear();
    uint32_t skip = 0;
    if (role == RoleSatellite)
    {
        auto& m = reg.members[(size_t) regSlot];
        MainBus* b = reg.mainBus();
        m.follows.store (b); m.seenMs.store (blockStartMs);
        if (satRelease.exchange (false)) sat.releaseAll (events);      // preparado a otra frecuencia: sus notas se sueltan
        sat.process (b, n, sr, stamp, playing, ppq, bpm, effLanes.load(), genEvents, isNonRealtime() ? -1.0 : blockStartMs);
        m.sync.store (sat.state.load());
        resetPending.store (false);
        if (panicRequest.exchange (false))
        {
            // Sólo en sus canales y sin tocar la réplica (sigue al Main).
            preview.reset();
            for (int l = 0; l < kLanes; ++l)
                if ((effLanes.load() >> l) & 1u)
                {
                    const int ch = juce::jlimit (1, 16, sat.rep.lanes[(size_t) l].channel) - 1;
                    for (int cc : { 64, 123, 120 }) { OutEvent e; e.offset = n - 1; e.data[0] = (uint8_t) (0xB0 | ch); e.data[1] = (uint8_t) cc; e.data[2] = 0; e.size = 3; e.lane = (int8_t) l; genEvents.push_back (e); }
                }
        }
    }
    else if (role == RoleMain)
    {
        Controls rawc;
        applyParams (vals.data(), rawc.g, rawc.lanes);
        rawc.g.cmdMode = true; rawc.g.runCmd = runCmd;
        rawc.reset = resetPending.exchange (false);
        rawc.panic = panicRequest.exchange (false);
        MainBus* b = reg.members[(size_t) regSlot].bus.load();
        if (! mainCore.attachedTo (b)) mainCore.attach (b);
        const Controls& c = mainCore.begin (rawc);
        gen.g = c.g; gen.lanes = c.lanes; gen.following = false;
        if (c.reset) gen.resetRequest.store (true);
        if (c.panic) { gen.panic (genEvents); preview.reset(); }
        else gen.process (n, playing, ppq, bpm, genEvents);
        if (gen.lastHostEdge() != 0) mirrorRun (gen.lastHostEdge() > 0);
        mainCore.end (n, stamp, genEvents, gen, isNonRealtime() ? -1.0 : blockStartMs);
        uint32_t on = 0; for (int l = 0; l < kLanes; ++l) if (c.lanes[(size_t) l].on) on |= 1u << l;
        reg.members[(size_t) regSlot].laneOn.store (on);
        if (linkMode == 1) LinkHub::get().publish (gen.published());
        // Los carriles que tocan los satélites no salen también por acá.
        if (vals[(size_t) X.skipSat] > 0.5f) skip = reg.takenLanes (b, blockStartMs, regSlot);
    }
    else if (role == RoleIndependent)
    {
        applyParams (vals.data(), gen);
        gen.g.cmdMode = true; gen.g.runCmd = runCmd;
        gen.following = false;
        if (linkMode == 2 && LinkHub::get().leaders.load() > 0)
        {
            Harmony h;
            if (LinkHub::get().read (h) && h.serial != 0) { gen.following = true; gen.external = h; gen.g.key = h.key; gen.g.scale = h.scale; }
        }
        if (resetPending.exchange (false)) gen.resetRequest.store (true);
        if (panicRequest.exchange (false)) { gen.panic (genEvents); preview.reset(); }
        else gen.process (n, playing, ppq, bpm, genEvents);
        if (gen.lastHostEdge() != 0) mirrorRun (gen.lastHostEdge() > 0);
        if (linkMode == 1) LinkHub::get().publish (gen.published());
    }
    // Un carril que un satélite acaba de tomar: lo que sonaba acá se suelta (si no, su note-off ya no saldría).
    if (const uint32_t newly = skip & ~skipped.load(); newly != 0)
        gen.forEachSounding ([&] (int ch, int note, int lane, int) {
            if (lane >= 0 && ((newly >> lane) & 1u)) { OutEvent e; e.offset = 0; e.data[0] = (uint8_t) (0x80 | ch); e.data[1] = (uint8_t) note; e.data[2] = 0; e.size = 3; e.lane = (int8_t) lane; events.push_back (e); }
        });
    skipped.store (skip);
    for (auto& e : genEvents) if (e.lane < 0 || ! ((skip >> e.lane) & 1u)) events.push_back (e);
    auto& G = active();

    // Salidas: host, puertos directos, puertos virtuales, sonido de prueba, chispas del editor.
    std::array<int, kLanes> roles {};
    for (int l = 0; l < kLanes; ++l) roles[(size_t) l] = G.lanes[(size_t) l].role;
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
        const double at = blockStartMs + off * 1000.0 / sr;
        if (e.lane >= 0)
        {
            if (const int ds = direct.slotForLane (e.lane); ds >= 0) queueDirect (at, ds, e.data, e.size);
            if (const int vs = direct.virtualSlotForLane (e.lane); vs >= 0) queueDirect (at, vs, e.data, e.size);
        }
        else
        {
            queueDirect (at, -1, e.data, e.size);
            for (int l = 0; l < kLanes; ++l)       // pánico y demás: también a los puertos virtuales de ese canal
                if (const int vs = direct.virtualSlotForLane (l); vs >= 0 && juce::jlimit (1, 16, G.lanes[(size_t) l].channel) - 1 == (e.data[0] & 0x0F)) queueDirect (at, vs, e.data, e.size);
        }
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
    const bool running = G.isRunning();
    if (vals[(size_t) X.clockOut] > 0.5f && direct.anyOpen())
    {
        const double perSample = (playing && G.g.hostClock ? bpm : (double) G.g.tempo) / 60.0 / sr;
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
    for (int l = 0; l < kLanes; ++l) direct.setLaneChannel (l, G.lanes[(size_t) l].channel);
    for (auto& d : demoAfter) runDemo (d, n);
}

// El transporte arrancó o paró a la generadora: el botón Play (y el host) lo muestran. Desde el hilo de audio, en el
// mismo bloque, así el valor nuevo ya no se lee como un cambio (una orden) en el bloque siguiente.
void OrreryProcessor::mirrorRun (bool on)
{
    lastRunParam = on;
    if (auto* p = apvts.getParameter ("run"); p != nullptr && (p->getValue() > 0.5f) != on) p->setValueNotifyingHost (on ? 1.0f : 0.0f);
}

void OrreryProcessor::runDemo (std::atomic<OrreryProcessor*>& which, int n)
{
    auto* d = which.load();
    if (d == nullptr || n > demoBuf.getNumSamples()) return;
    juce::AudioBuffer<float> b (demoBuf.getArrayOfWritePointers(), 2, n);
    d->setPlayHead (getPlayHead());
    d->processBlock (b, demoMidi);
}

void OrreryProcessor::addDemoInstance (std::unique_ptr<OrreryProcessor> p, bool before)
{
    p->setPlayConfigDetails (0, 2, getSampleRate() > 0 ? getSampleRate() : 48000.0, std::max (64, getBlockSize()));
    p->prepareToPlay (getSampleRate() > 0 ? getSampleRate() : 48000.0, std::max (64, getBlockSize()));
    demoMidi.ensureSize (8192);
    auto* raw_ = p.get();
    demoOwned.push_back (std::move (p));
    if (before) demoBefore.store (raw_);
    else for (auto& d : demoAfter) if (d.load() == nullptr) { d.store (raw_); break; }
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
    resetPending.store (true);          // la sesión nueva la arma el hilo de audio (un Main la pasa también a sus satélites)
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
    // Lo que Auto resolvió: al reabrir el proyecto, el Main sigue siendo Main y cada satélite su carril.
    state.setProperty ("autoRole", autoRole.load(), nullptr);
    state.setProperty ("autoLanes", (int) autoLanes.load(), nullptr);
    // Para reconocer una copia (Duplicate Track en Live copia el estado a una instancia nueva): ver setStateInformation.
    { const std::lock_guard<std::mutex> lk (Constellation::get().lock); state.setProperty ("instanceId", instanceId, nullptr); }
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
    // Proyectos de 1.0.4 o antes: no tienen rol. Se abren como Independent, con su Link, y suenan como antes.
    const bool legacy = ! state.getChildWithProperty ("id", "role").isValid();
    const int savedAutoRole = state.getProperty ("autoRole", 0);
    const int savedAutoLanes = state.getProperty ("autoLanes", 0);
    const juce::String savedId = state.getProperty ("instanceId").toString();
    // Un archivo de preset (.vstpreset, para el navegador de Live, que no muestra las listas de programas de VST3):
    // el estado de un programa sin autoRole/autoLanes/instanceId (las herramientas que los escriben los sacan). Se
    // aplica como un preset del menú: cambia la música y no toca Play, Clock, el rol, los puertos ni nada global.
    if (! legacy && ! state.hasProperty ("autoRole"))
    {
        for (auto& e : params())
        {
            if (e.global) continue;
            const auto c = state.getChildWithProperty ("id", e.id);
            setValue (e.id, c.isValid() ? (float) c.getProperty ("value", e.def) : e.def);
        }
        tracker.capture (*this);
        resetPending.store (true);
        notifyUi (onPresetChanged);
        return;
    }
    for (auto id : { "presetName", "program", "ports", "presetSnapshot", "autoRole", "autoLanes", "instanceId" }) state.removeProperty (id, nullptr);
    // ¿Es una copia? Si otra Orrery viva ya tiene ese id, este estado se copió de ella (Duplicate Track en Live, copiar
    // la pista o el efecto en REAPER): la copia no hereda su rol, vuelve a resolver Auto (satélite en el próximo carril
    // libre). Si no, es la misma instancia (deshacer) o un proyecto que se abre: se queda con su id y su rol.
    bool copy = false;
    {
        const std::lock_guard<std::mutex> lk (Constellation::get().lock);
        if (savedId.isNotEmpty())
            for (auto* p : allInstances()) if (p != this && p->instanceId == savedId) copy = true;
        if (savedId.isNotEmpty() && ! copy) instanceId = savedId;
        else if (copy) instanceId = juce::Uuid().toString();
    }
    restoring.store (true);
    apvts.replaceState (state);
    autoRole.store (legacy || copy ? 0 : juce::jlimit (0, 3, savedAutoRole));
    autoLanes.store (copy ? 0u : (uint32_t) juce::jlimit (0, 255, savedAutoLanes));
    if (legacy) setValue ("role", (float) RoleIndependent);
    if (copy && (int) std::lround (value ("role")) != RoleIndependent) { setValue ("role", (float) RoleAuto); setValue ("sat_lanes", 0.0f); }
    if (copy) restoring.store (false);       // una copia que se vuelve satélite apaga su sonido de prueba, como una nueva
    updateRole (true);
    restoring.store (false);
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
