#include <set>
#include "PluginProcessor.h"
#if ! ATMD_NO_EDITOR
 #include "PluginEditor.h"
#endif

using namespace dh::md;

// La entrada es opcional: es la EXTERNAL INPUT del mezclador (una pista, o la salida del propio plugin
// devuelta por un envío para el lazo de realimentación que usaban los dueños del original).
juce::AudioProcessor::BusesProperties AtmdProcessor::makeBuses()
{
    // En la app independiente la entrada viene encendida (si no, JUCE no abre ningún canal de entrada); en un DAW
    // queda apagada hasta que el usuario la rutea, como corresponde a un instrumento.
    const bool standalone = juce::PluginHostType::getPluginLoadedAs() == juce::AudioProcessor::wrapperType_Standalone;
    return BusesProperties()
        .withInput ("External Input", juce::AudioChannelSet::stereo(), standalone)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

AtmdProcessor::AtmdProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "ATMD", createLayout())
{
    for (auto& p : params()) raw.push_back (apvts.getRawParameterValue (p.id));
    vals.assign (raw.size(), 0.0f);
    loadFactory();
    rescanUserPresets();
}

juce::AudioProcessorValueTreeState::ParameterLayout AtmdProcessor::createLayout()
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
        {
            juce::NormalisableRange<float> range (e.lo, e.hi);
            if (e.skew != 1.0f) range.setSkewForCentre (e.lo + (e.hi - e.lo) * std::pow (0.5f, 1.0f / e.skew));
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, e.name, range, e.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([ep] (float v, int) { return formatParam (*ep, v); })));
        }
    }
    return layout;
}

void AtmdProcessor::setValue (const juce::String& id, float v)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

bool AtmdProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void AtmdProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (64, samplesPerBlock);
    osNow = oversamplingFactor (value ("oversampling"));
    eng.prepare (sampleRate, maxBlock, osNow);
    extBuf.assign ((size_t) maxBlock + 64, 0.0f);
    tmpR.assign ((size_t) maxBlock + 64, 0.0f);
    keyboardState.reset();
}

void AtmdProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) eng.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) eng.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel()) pitchWheel.store (juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8191.0f));
    else if (m.isChannelPressure()) eng.aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch()) eng.aftertouch = (float) m.getAfterTouchValue() / 127.0f;     // polifónico: se toma el último
    else if (m.isController())
    {
        const int c = m.getControllerNumber(), val = m.getControllerValue();
        if (c == 1) modWheel.store ((float) val / 127.0f);
        else if (c == 64) eng.setSustain (val >= 64);
        else if (c == 120 || c == 123) eng.allNotesOff (c == 120);
        else if (c == 121) { pitchWheel.store (0.0f); modWheel.store (0.0f); eng.aftertouch = 0.0f; eng.setSustain (false); }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) eng.allNotesOff (m.isAllSoundOff());
}

void AtmdProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    wasBypassed = false;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (size_t k = 0; k < raw.size(); ++k) vals[k] = raw[k]->load();

    // El sobremuestreo cambia el motor entero: se prepara de nuevo (corta las notas, es un ajuste de sistema).
    const int os = oversamplingFactor (vals[(size_t) Ix::get().oversampling]);
    if (os != osNow) { osNow = os; eng.prepare (sr, std::max (maxBlock, n), osNow); }
    applyParams (vals.data(), eng);
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) eng.bpm = *b;

    // Entrada externa (mono): el promedio de los canales de entrada, si el bus está activo.
    const int ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    if ((int) extBuf.size() < n) { extBuf.resize ((size_t) n); tmpR.resize ((size_t) n); }
    const bool hasExt = ins > 0;
    if (hasExt)
    {
        juce::FloatVectorOperations::copy (extBuf.data(), buffer.getReadPointer (0), n);
        if (ins > 1) { juce::FloatVectorOperations::add (extBuf.data(), buffer.getReadPointer (1), n); juce::FloatVectorOperations::multiply (extBuf.data(), 0.5f, n); }
    }

    // Teclas del mouse en el editor → al buffer MIDI; las que llegan del host quedan marcadas para dibujarlas.
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    float* L = buffer.getWritePointer (0);
    float* R = outs > 1 && buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : tmpR.data();
    auto it = midi.cbegin();
    int pos = 0;
    while (pos < n)
    {
        while (it != midi.cend() && (*it).samplePosition <= pos) { handleMidi ((*it).getMessage()); ++it; }
        int end = n;
        if (it != midi.cend()) end = std::min (end, std::max (pos + 1, (*it).samplePosition));
        eng.bend = pitchWheel.load(); eng.modWheel = modWheel.load();
        eng.process (hasExt ? extBuf.data() + pos : nullptr, L + pos, R + pos, end - pos);
        pos = end;
    }
    for (; it != midi.cend(); ++it) handleMidi ((*it).getMessage());
    midi.clear();
    if (outs < 2) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
    for (int c = std::max (outs, 2); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    float outPk = 0.0f; for (int i = 0; i < n; ++i) outPk = std::max (outPk, std::abs (L[i]));
    outLevel.store (std::max (outPk, outLevel.load() * 0.9f));
    // La lámpara retiene ~0.3 s después del último recorte, como la del original.
    if (eng.overload) overloadHold = (int) (0.3 * sr);
    else overloadHold = std::max (0, overloadHold - n);
    overload.store (overloadHold > 0);
    voicesActive.store (eng.activeVoices());
    const double used = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    cpuLoad.store (cpuLoad.load() * 0.9f + (float) (used / (n / sr)) * 0.1f);
}

// En bypass: silencio y se sueltan las notas (al volver no queda nada colgado).
void AtmdProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (! wasBypassed) { eng.allNotesOff (false); wasBypassed = true; }
    buffer.clear();
    midi.clear();
}

// ───────────── presets ─────────────
int AtmdProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void AtmdProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String AtmdProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void AtmdProcessor::loadPreset (int index)
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
    // Presets cuyo sonido es la modulación traen la rueda MOD ya levantada (no es un parámetro: es la mano).
    modWheel.store (juce::jlimit (0.0f, 1.0f, (float) v.getProperty ("mod_wheel", 0.0)));
    currentPresetName = presets[(size_t) index].name;
    notifyUi (onPresetChanged);
}

void AtmdProcessor::loadFactory()
{
   #if ATMD_HAS_ASSETS
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

juce::File AtmdProcessor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/ATMD/Presets");
   #else
    return base.getChildFile ("Audio Tools/ATMD/Presets");
   #endif
}

void AtmdProcessor::rescanUserPresets()
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

bool AtmdProcessor::saveUserPreset (const juce::String& name)
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
    for (int i = 0; i < (int) presets.size(); ++i) if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) currentProgram = i;
    notifyUi (onPresetChanged);
    return ok;
}

void AtmdProcessor::notifyUi (std::function<void()>& cb)
{
    if (! cb) return;
    if (juce::MessageManager::existsAndIsCurrentThread()) { cb(); return; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<AtmdProcessor> (this), &cb] { if (safe != nullptr && cb) cb(); });
}

void AtmdProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AtmdProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || ! state.hasType (apvts.state.getType())) return;
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    currentProgram = state.getProperty ("program", 0);
    for (auto id : { "presetName", "program" }) state.removeProperty (id, nullptr);
    std::set<juce::String> present;
    for (const auto& c : state) if (c.hasProperty ("id")) present.insert (c.getProperty ("id").toString());
    apvts.replaceState (state);
    // Parámetros que faltan (sesiones viejas) o llegaron como NaN: a su valor por defecto.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! dh::isFiniteF (rp->getValue()) || present.count (rp->paramID) == 0) rp->setValueNotifyingHost (rp->getDefaultValue());
    notifyUi (onPresetChanged);
}

#if ! ATMD_NO_EDITOR
juce::AudioProcessorEditor* AtmdProcessor::createEditor() { return new AtmdEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AtmdProcessor(); }
#else
juce::AudioProcessorEditor* AtmdProcessor::createEditor() { return nullptr; }
#endif
