#include <set>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

using namespace dh;

namespace {
// Ajustes globales del usuario que un preset no pisa.
bool isPresetScoped (const juce::String& id)
{
    return id != "quality" && id != "output" && id != "midi_out" && id != "bend_range";
}

// Ableton Live llena el panel del dispositivo solo con hasta 64 parámetros automatizables (QA en Live 2026-10-22,
// H2: el AT20V tenía 68). Cinco ajustes que no se automatizan dejan de ofrecerse al host; siguen en la página de
// extensiones, en los presets y en las sesiones, con los mismos ids.
bool isAutomatableExt (const juce::String& id)
{
    return id != "quality" && id != "midi_out" && id != "bend_range" && id != "voc_bands" && id != "priority";
}

// AT20V FX: sólo los presets que trabajan con la entrada (efectos, vocoder, guitar synth) y los que tocan solos; los
// de sinte necesitan notas y como efecto callaban la pista (QA en REAPER 2026-10-22, A1: 38 de 55 en silencio).
// Los efectos primero: el programa 0 es Filter FX, lo mismo que una instancia nueva.
int fxOrder (const juce::String& name, const juce::String& category)
{
    if (category == "Effect") return 0;
    if (category == "Vocoder" || name.endsWith ("(Audio In)")) return 1;
    if (category == "Self-Playing") return 2;
    return -1;
}

}

juce::AudioProcessor::BusesProperties AT20VProcessor::makeBuses()
{
   #if AT20V_IS_FX
    // Variante efecto: la entrada principal es el modulador del vocoder y la entrada de la ESP.
    return BusesProperties()
        .withInput ("Audio In", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #else
    return BusesProperties()
        .withInput ("Audio In (Vocoder / ESP)", juce::AudioChannelSet::stereo(), false)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #endif
}

AT20VProcessor::AT20VProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "AT20V", createLayout())
{
    for (int i = 0; i < kNumKnobs; ++i)
    {
        knobParams[(size_t) i] = apvts.getRawParameterValue (paramId (i));
        apvts.addParameterListener (paramId (i), this);
    }
    for (auto& e : extParams()) extRaw.push_back (apvts.getRawParameterValue (e.id));
    pQuality = apvts.getRawParameterValue ("quality");
    pMidiOut = apvts.getRawParameterValue ("midi_out");
    for (auto& a : patchAtomic) a.store (-1);
    loadFactoryPresets();
    rescanUserPresets();
    // Una instancia nueva es el programa 0. En AT20V FX el programa 0 es Filter FX (la pista pasa por los filtros sin
    // MIDI; con "Init" no sonaba nada hasta tocar notas).
    if (! presets.empty()) loadPreset (0);
}

AT20VProcessor::~AT20VProcessor()
{
    for (int i = 0; i < kNumKnobs; ++i) apvts.removeParameterListener (paramId (i), this);
}

juce::AudioProcessorValueTreeState::ParameterLayout AT20VProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto normal = Engine::normalSetting();
    for (int i = 0; i < kNumKnobs; ++i)
    {
        const auto choices = choicesFor (i);
        if (! choices.isEmpty())
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { paramId (i), 1 }, paramName (i),
                                                                      choices, (int) normal[(size_t) i]));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { paramId (i), 1 }, paramName (i), juce::NormalisableRange<float> (0.0f, 1.0f),
                normal[(size_t) i],
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([i] (float v, int) { return formatKnob (i, v); })));
    }
    for (auto& e : extParams())
    {
        if (e.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { e.id, 1 }, e.name, extChoices (e), (int) e.def,
                juce::AudioParameterChoiceAttributes().withAutomatable (isAutomatableExt (e.id))));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { e.id, 1 }, e.name, juce::NormalisableRange<float> (e.lo, e.hi), e.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([&e] (float v, int) { return formatExt (e, v); })
                                                     .withAutomatable (isAutomatableExt (e.id))));
    }
    return layout;
}

void AT20VProcessor::parameterChanged (const juce::String&, float) {}

bool AT20VProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void AT20VProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate;
    currentBlock = std::max (samplesPerBlock, 64);
    engine.prepare (sampleRate, 64, (int) pQuality->load() == 1 ? 8 : 4);
    latency.setNow ((int) std::lround (dh::halfbandChainDelay (engine.oversampling()) * (AT20V_IS_FX ? 2.0 : 1.0)));      // el retardo de los filtros del sobremuestreo, para que el host lo compense
    hostGap.prepare (sampleRate);
    midiOutDelay.prepare(); midiOutDelay.setDelay (latencyNow());
    if (patchNote >= 0) pendingPatchOff = patchNote;     // su note-off sale en el próximo bloque
    patchNote = -1;
    for (int i = 0; i < kNumKnobs; ++i) smoothed[(size_t) i] = knobParams[(size_t) i]->load();
    espMono.assign ((size_t) currentBlock + 64, 0.0f);
    monoOut.assign ((size_t) currentBlock + 64, 0.0f);
    rightOut.assign ((size_t) currentBlock + 64, 0.0f);
}

void AT20VProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
    {
        // El pitch bend mueve la rueda de control del MS-20 (que sólo actúa si está patcheada);
        // además puede doblar la afinación si se elige un rango en los ajustes.
        const float v = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        bendWheel = v;
        wheel.store (juce::jlimit (-1.0f, 1.0f, bendWheel + modWheel));
        engine.pitchBend (v);
    }
    else if (m.isController())
    {
        const int c = m.getControllerNumber();
        if (c == 64) momentaryPedal.store (m.getControllerValue() >= 64);
        else if (c == 1) { modWheel = m.getControllerValue() / 127.0f; wheel.store (juce::jlimit (-1.0f, 1.0f, bendWheel + modWheel)); }
        else if (c == 121) { modWheel = bendWheel = 0.0f; wheel.store (0.0f); engine.pitchBend (0.0f); momentaryPedal.store (false); }
        else if (c == 123 || c == 120) engine.allNotesOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

void AT20VProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    wasBypassed = false;
    auto out = getBusBuffer (buffer, false, 0);

    // El host dejó de llamar (REAPER con el plugin en bypass) y vuelve: un note-off pudo perderse en el hueco. Se
    // sueltan las voces y las teclas en pantalla, y el MIDI de salida cierra sus notas (QA en DAWs 2026-10-22, X1).
    const bool gap = hostGap.check (getPlayHead(), n, isNonRealtime());
    juce::MidiBuffer gapOffs;
    if (gap)
    {
        engine.allNotesOff();
        keyboardState.reset();
        if (patchNote >= 0) { pendingPatchOff = patchNote; patchNote = -1; }
        midiOutDelay.flushNoteOffs (gapOffs);
    }

    const int wantOs = (int) pQuality->load() == 1 ? 8 : 4;
    { if (wantOs != engine.oversampling()) engine.prepare (currentSr, 64, wantOs, true); latency.request ((int) std::lround (dh::halfbandChainDelay (engine.oversampling()) * (AT20V_IS_FX ? 2.0 : 1.0))); }
    if ((int) monoOut.size() < n) { monoOut.resize ((size_t) n); espMono.resize ((size_t) n); rightOut.resize ((size_t) n); }

    // Teclado en pantalla → MIDI (queda mezclado en el buffer de entrada).
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (! midi.isEmpty()) midiInFlash.store (true);

    // Ajustes desde los parámetros.
    auto get = [this] (const char* id) {
        const auto& ps = extParams();
        for (size_t k = 0; k < ps.size(); ++k) if (std::strcmp (ps[k].id, id) == 0) return extRaw[k]->load();
        return 0.0f;
    };
    applyExtParams (get, engine.settings, engine.fx);
    auto& st = engine.settings;
    st.wheel = wheel.load();
    st.momentary = momentary.load() || momentaryPedal.load();
    for (int j = 0; j < kNumJacks; ++j) engine.patch.src[(size_t) j] = patchAtomic[(size_t) j].load (std::memory_order_relaxed);

    // Tempo y posición del host (MG sincronizado y delay).
    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    if (! playing) { ppq = freePpq; freePpq += n * bpm / 60.0 / currentSr; }
    st.bpm = bpm; st.playing = playing;
    const double ppqPerSample = bpm / 60.0 / currentSr;

    // Entrada de audio (ESP y modulador del vocoder), sumada a mono.
    const float* espPtr = nullptr;
    if (getBusCount (true) > 0 && getBus (true, 0)->isEnabled())
    {
        auto in = getBusBuffer (buffer, true, 0);
        if (in.getNumChannels() > 0)
        {
            juce::FloatVectorOperations::copy (espMono.data(), in.getReadPointer (0), n);
            for (int c = 1; c < in.getNumChannels(); ++c) juce::FloatVectorOperations::add (espMono.data(), in.getReadPointer (c), n);
            juce::FloatVectorOperations::multiply (espMono.data(), 1.0f / (float) in.getNumChannels(), n);
            espPtr = espMono.data();
            inputLevel.store (juce::FloatVectorOperations::findMaximum (espMono.data(), n));
        }
    }

    // MIDI OUT: Thru copia la entrada; Patch convierte los disparos del bus de teclado (incluidos
    // los que genera la patchera, p. ej. MG → TRIG IN) en notas con la altura de VCO1.
    const int midiMode = juce::jlimit (0, 3, (int) pMidiOut->load());
    juce::MidiBuffer outMidi;
    if (pendingPatchOff >= 0) { outMidi.addEvent (juce::MidiMessage::noteOff (1, pendingPatchOff), 0); pendingPatchOff = -1; }
    if (gap) { const juce::MidiBuffer none; forwardThru (none, outMidi, n, false); }   // lo que pasó en Thru se cierra
    forwardThru (midi, outMidi, n, midiMode == 1 || midiMode == 3);

    auto it = midi.cbegin();
    int pos = 0;
    while (pos < n)
    {
        int end = std::min (n, pos + 64);
        while (it != midi.cend() && (*it).samplePosition <= pos) { handleMidi ((*it).getMessage()); ++it; }
        if (it != midi.cend()) end = std::min (end, std::max (pos + 1, (*it).samplePosition));

        for (int i = 0; i < kNumKnobs; ++i)
        {
            const float target = knobParams[(size_t) i]->load();
            if (layout::knobs[i].steps > 0) smoothed[(size_t) i] = target;
            else smoothed[(size_t) i] += (target - smoothed[(size_t) i]) * 0.35f;
            if (! dh::isFiniteF (smoothed[(size_t) i])) smoothed[(size_t) i] = dh::isFiniteF (target) ? target : 0.0f;
        }
        engine.knobs = smoothed;
        st.ppq = ppq + pos * ppqPerSample;
        engine.render (monoOut.data() + pos, rightOut.data() + pos, espPtr != nullptr ? espPtr + pos : nullptr, end - pos);

        if (midiMode >= 2)
            for (int e = 0; e < engine.numMidiEvents; ++e)
            {
                const auto& ev = engine.midiEvents[(size_t) e];
                const int at = juce::jlimit (0, n - 1, pos + ev.pos);
                if (ev.on)
                {
                    const int note = juce::jlimit (0, 127, (int) std::lround (69.0 + 12.0 * std::log2 (std::max (8.0f, ev.hz) / 440.0)));
                    if (patchNote >= 0) outMidi.addEvent (juce::MidiMessage::noteOff (1, patchNote), at);
                    outMidi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), at);
                    patchNote = note;
                }
                else if (patchNote >= 0)
                {
                    outMidi.addEvent (juce::MidiMessage::noteOff (1, patchNote), at);
                    patchNote = -1;
                }
            }
        pos = end;
    }
    while (it != midi.cend()) { handleMidi ((*it).getMessage()); ++it; }
    if (midiMode < 2 && patchNote >= 0) { outMidi.addEvent (juce::MidiMessage::noteOff (1, patchNote), 0); patchNote = -1; }
    midi.swapWith (outMidi);
    // El audio sale atrasado la latencia del sobremuestreo y el host lo compensa; el MIDI de salida no lo compensa
    // nadie, así que sale atrasado lo mismo (si no, una pista que dobla por MIDI From quedaba adelantada 27 muestras).
    midiOutDelay.setDelay (latencyNow());
    midiOutDelay.process (midi, n);
    for (const auto md : gapOffs) midi.addEvent (md.getMessage(), 0);
    if (! midi.isEmpty()) midiOutFlash.store (true);

    if (out.getNumChannels() >= 2) { out.copyFrom (0, 0, monoOut.data(), n); out.copyFrom (1, 0, rightOut.data(), n); }
    else if (out.getNumChannels() == 1)
    {
        out.copyFrom (0, 0, monoOut.data(), n, 0.5f);
        out.addFrom (0, 0, rightOut.data(), n, 0.5f);
    }
    for (int c = out.getNumChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    const auto& v0 = engine.voices[0];
    ledMg.store (v0.ledMg); ledEg.store (v0.ledEg);
    ledEspPeak.store (v0.ledEspPeak); ledEspTrig.store (v0.ledEspTrig);
    activeVoices.store (engine.activeVoiceCount());

    // Carga: tiempo de proceso sobre la duración del bloque (suavizado).
    const double used = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    const float load = (float) (used / (n / currentSr));
    cpuLoad.store (cpuLoad.load() * 0.9f + load * 0.1f);
}

// En bypass: silencio, las voces se sueltan al entrar (el note-off puede llegar durante el bypass y perderse),
// la nota del modo Patch se cierra, y el MIDI de entrada sólo pasa si la salida está en Thru.
void AT20VProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
   #if AT20V_IS_FX
    // El efecto en bypass deja pasar la entrada, retrasada la latencia que el host compensa.
    {
        const int n = buffer.getNumSamples(), L = getLatencySamples();
        if ((int) bypassL.size() != L) { bypassL.assign ((size_t) std::max (1, L), 0.0f); bypassR = bypassL; bypassPos = 0; }
        if (L > 0 && n > 0)
        {
            if (buffer.getNumChannels() > 1 && getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
            float* d[2] = { bypassL.data(), bypassR.data() };
            for (int c = 0; c < std::min (2, buffer.getNumChannels()); ++c)
            {
                auto* x = buffer.getWritePointer (c); int p = bypassPos;
                for (int i = 0; i < n; ++i) { const float y = d[c][p]; d[c][p] = x[i]; x[i] = y; if (++p >= L) p = 0; }
            }
            bypassPos = (bypassPos + n) % L;
        }
    }
   #else
    buffer.clear();
   #endif
    juce::MidiBuffer outMidi;
    if (! wasBypassed)
    {
        engine.allNotesOff();
        if (patchNote >= 0) { outMidi.addEvent (juce::MidiMessage::noteOff (1, patchNote), 0); patchNote = -1; }
        if (pendingPatchOff >= 0) { outMidi.addEvent (juce::MidiMessage::noteOff (1, pendingPatchOff), 0); pendingPatchOff = -1; }
        midiOutDelay.flushNoteOffs (outMidi);
        wasBypassed = true;
    }
    const int midiMode = juce::jlimit (0, 3, (int) pMidiOut->load());
    forwardThru (midi, outMidi, std::max (1, buffer.getNumSamples()), midiMode == 1 || midiMode == 3);
    midi.swapWith (outMidi);
}

// MIDI Thru: copia la entrada y recuerda qué notas quedaron prendidas; si la salida deja de estar en Thru con
// teclas apretadas, cierra esas notas (si no, quedaban colgadas en el sinte de destino).
void AT20VProcessor::forwardThru (const juce::MidiBuffer& in, juce::MidiBuffer& out, int n, bool thru)
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

// Los callbacks de la UI corren siempre en el hilo de mensajes (el host puede restaurar el
// estado o cambiar de programa desde otro hilo).
void AT20VProcessor::notifyUi (std::function<void()>& cb)
{
    if (! cb) return;
    if (juce::MessageManager::existsAndIsCurrentThread()) { cb(); return; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<AT20VProcessor> (this), &cb] {
        if (safe != nullptr && cb) cb();
    });
}

// ───────────── Cables ─────────────
void AT20VProcessor::setCables (std::vector<Cable> newCables)
{
    { const juce::SpinLock::ScopedLockType l (cableLock); cables = std::move (newCables); }
    const auto ps = patchFromCables (getCables());
    for (int j = 0; j < kNumJacks; ++j) patchAtomic[(size_t) j].store (ps.src[(size_t) j]);
    notifyUi (onCablesChanged);
}

// ───────────── Presets ─────────────
void AT20VProcessor::loadFactoryPresets()
{
   #if AT20V_HAS_ASSETS
    juce::StringArray names;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        // JUCE antepone "_" a los recursos cuyo nombre empieza con un dígito.
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
   #if AT20V_IS_FX
    presets.erase (std::remove_if (presets.begin(), presets.end(), [] (auto& p) { return fxOrder (p.name, p.category) < 0; }), presets.end());
    std::stable_sort (presets.begin(), presets.end(), [] (auto& a, auto& b) { return fxOrder (a.name, a.category) < fxOrder (b.name, b.category); });
   #endif
   #endif
}

juce::File AT20VProcessor::userPresetDir()
{
    // macOS: ~/Library/Application Support/…; Windows: %APPDATA%\…
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/AT20V/Presets");
   #else
    return base.getChildFile ("Audio Tools/AT20V/Presets");
   #endif
}

void AT20VProcessor::rescanUserPresets()
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

int AT20VProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }

void AT20VProcessor::setCurrentProgram (int index) { loadPreset (index); }

const juce::String AT20VProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void AT20VProcessor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    loadPresetData (presetFromJson (presets[(size_t) index].data));
}

void AT20VProcessor::loadPresetData (const Preset& p)
{
    for (int i = 0; i < kNumKnobs; ++i)
        if (auto* prm = apvts.getParameter (paramId (i)))
            prm->setValueNotifyingHost (prm->convertTo0to1 (p.knobs[(size_t) i]));
    // Ajustes de la página de extensiones: lo que el preset no trae vuelve a su valor por defecto
    // (si no, la reverb o el unísono de un preset quedarían pegados en el siguiente).
    for (auto& e : extParams())
    {
        if (! isPresetScoped (e.id)) continue;
        auto* prm = apvts.getParameter (e.id);
        if (prm == nullptr) continue;
        const float v = p.settings.contains (e.id) ? (float) p.settings[e.id] : e.def;
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }
    currentPresetName = p.name;
    tracker.capture (*this);
    setCables (p.cables);
    notifyUi (onPresetChanged);
}

Preset AT20VProcessor::currentAsPreset (const juce::String& name, const juce::String& category) const
{
    Preset p;
    p.name = name; p.category = category;
    for (int i = 0; i < kNumKnobs; ++i) p.knobs[(size_t) i] = knobParams[(size_t) i]->load();
    p.cables = getCables();
    const auto& ps = extParams();
    for (size_t k = 0; k < ps.size(); ++k)
        if (isPresetScoped (ps[k].id) && std::abs (extRaw[k]->load() - ps[k].def) > 1e-4f)
            p.settings.set (ps[k].id, extRaw[k]->load());
    return p;
}

bool AT20VProcessor::saveUserPreset (const juce::String& name)
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

// ───────────── Estado ─────────────
void AT20VProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName.get(), nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("wheel", wheel.load(), nullptr);       // la rueda fijada con Alt (un drone por VCA INITIAL GAIN) vuelve al reabrir
    state.setProperty ("cables", juce::JSON::toString (presetToJson (currentAsPreset (currentPresetName, "")).getProperty ("cables", {}), true), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AT20VProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        currentPresetName = state.getProperty ("presetName", "Init").toString();
        wheel.store (juce::jlimit (-1.0f, 1.0f, (float) state.getProperty ("wheel", 0.0)));
        const auto cablesJson = juce::JSON::parse (state.getProperty ("cables", "[]").toString());
        state.removeProperty ("cables", nullptr);
        std::set<juce::String> present;
        for (const auto& c : state) if (c.hasProperty ("id")) present.insert (c.getProperty ("id").toString());
        apvts.replaceState (state);
        // Parámetros que faltan en el estado (sesiones viejas) o que llegaron como NaN: a su valor por defecto.
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! dh::isFiniteF (rp->getValue()) || present.count (rp->paramID) == 0) rp->setValueNotifyingHost (rp->getDefaultValue());
        auto* o = new juce::DynamicObject();
        o->setProperty ("cables", cablesJson);
        setCables (presetFromJson (juce::var (o)).cables);
        // El programa se busca por nombre: la lista de AT20V FX cambió en 1.0.7 y los nombres de los presets de audio in
        // llevan "(Audio In)"; el número guardado queda para nombres repetidos o desconocidos.
        currentProgram = juce::jlimit (0, std::max (0, (int) presets.size() - 1), (int) state.getProperty ("program", 0));
        {
            const auto nm = currentPresetName.get();
            if (! juce::isPositiveAndBelow (currentProgram, (int) presets.size()) || presets[(size_t) currentProgram].name != nm)
                for (int i = 0; i < (int) presets.size(); ++i)
                    if (presets[(size_t) i].name == nm || presets[(size_t) i].name == nm + " (Audio In)") { currentProgram = i; break; }
        }
        if (state.hasProperty ("presetSnapshot")) tracker.fromString (*this, state.getProperty ("presetSnapshot").toString());
        else tracker.capture (*this);           // sesión anterior a la marca: el nombre sin "*"
        notifyUi (onPresetChanged);
    }
}

juce::AudioProcessorEditor* AT20VProcessor::createEditor() { return new AT20VEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AT20VProcessor(); }
