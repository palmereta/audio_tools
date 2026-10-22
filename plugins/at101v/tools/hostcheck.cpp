// Pruebas del AT101V cargado como lo carga un DAW (VST3 o AU compilados), para los hallazgos del QA en DAWs
// del 2026-10-22: lo que depende del wrapper y del procesador, no sólo del motor (eso está en dsptest).
//
//   at101vhostcheck <AT101V.vst3|AT101V.component> [--tests t1,t2,...] [--out dir]
//
// Un .component fuera de las carpetas de Plug-Ins se registra en este proceso (fabricante 'Atqa'): se prueba el
// compilado, no el instalado. Pruebas (por defecto todas):
//   T1 program0  una instancia nueva es el programa 0 (parámetros y audio)                                    (X2)
//   T2 fresh     dos instancias nuevas, una con tiempo procesado parada antes, después reset(): mismo audio  (B3)
//   T3 buffers   Acid Line con reloj del host a 64/333/512/1024 y bloques al azar: mismo MIDI out y audio  (B2)
//   T4 state     getStateInformation estable byte a byte (dos veces, y guardar → cargar → guardar)          (H4)
//   T5 gap       el host deja de llamar (bypass de REAPER) mientras llega el note-off: la nota no queda colgada (X1)
//   T6 latency   el MIDI out sale atrasado la latencia que se informa, alineado con el audio                 (MIDI out)
//   T7 zipper    escribe <out>/auto_env.wav y auto_static.wav (automatización de Mixer Saw/Pulse por bloque) (B1)
// Sale con 1 si alguna prueba falla.

#if defined (__APPLE__)
 #include <AudioToolbox/AudioToolbox.h>
#endif
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <thread>

namespace {

int failures = 0;
void result (const char* id, bool ok, const juce::String& msg)
{
    std::printf ("%s %-10s %s\n", ok ? "PASS" : "FAIL", id, msg.toRawUTF8());
    if (! ok) ++failures;
}

struct PlayHead : juce::AudioPlayHead
{
    double sr = 48000.0, bpm = 120.0;
    bool playing = false;
    juce::int64 pos = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo pi;
        const double ppq = (double) pos / sr * bpm / 60.0;
        pi.setBpm (bpm); pi.setIsPlaying (playing); pi.setPpqPosition (ppq); pi.setTimeInSamples (pos);
        pi.setTimeInSeconds ((double) pos / sr); pi.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        pi.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        return pi;
    }
};

struct Plug
{
    std::unique_ptr<juce::AudioPluginInstance> p;
    PlayHead ph;
    double sr = 48000.0;
    int maxBlock = 1024;
};

juce::AudioPluginFormatManager& formats()
{
    static juce::AudioPluginFormatManager fm;
    static bool init = false;
    if (! init) { juce::addDefaultFormatsToManager (fm); init = true; }
    return fm;
}

#if JUCE_MAC
// Igual que athosttest: el host de AU busca por tipo/subtipo/fabricante y con un path cargaría el instalado.
juce::String registerBuiltAu (const juce::String& path)
{
    CFURLRef url = CFURLCreateWithFileSystemPath (nullptr, path.toCFString(), kCFURLPOSIXPathStyle, true);
    CFBundleRef bundle = CFBundleCreate (nullptr, url);
    CFRelease (url);
    if (bundle == nullptr || ! CFBundleLoadExecutable (bundle)) return {};
    auto comps = (CFArrayRef) CFBundleGetValueForInfoDictionaryKey (bundle, CFSTR ("AudioComponents"));
    if (comps == nullptr || CFArrayGetCount (comps) < 1) return {};
    auto d = (CFDictionaryRef) CFArrayGetValueAtIndex (comps, 0);
    auto str = [d] (CFStringRef k) { auto v = (CFStringRef) CFDictionaryGetValue (d, k); return v != nullptr ? juce::String::fromCFString (v) : juce::String(); };
    auto fourCC = [] (const juce::String& s) { OSType t = 0; for (int i = 0; i < 4; ++i) t = (t << 8) | (OSType) (juce::uint8) (i < s.length() ? s[i] : ' '); return t; };
    auto fn = (AudioComponentFactoryFunction) CFBundleGetFunctionPointerForName (bundle, str (CFSTR ("factoryFunction")).toCFString());
    if (fn == nullptr) return {};
    UInt32 version = 0;
    if (auto v = (CFNumberRef) CFDictionaryGetValue (d, CFSTR ("version"))) CFNumberGetValue (v, kCFNumberSInt32Type, &version);
    AudioComponentDescription desc { fourCC (str (CFSTR ("type"))), fourCC (str (CFSTR ("subtype"))), fourCC ("Atqa"), 0, 0 };
    if (AudioComponentRegister (&desc, juce::String ("Audio Tools QA: " + str (CFSTR ("name"))).toCFString(), version, fn) == nullptr) return {};
    return "AudioUnit:Synths/" + str (CFSTR ("type")) + "," + str (CFSTR ("subtype")) + ",Atqa";
}
#endif

juce::String pluginPath;

std::unique_ptr<Plug> load (int maxBlock = 1024, double sr = 48000.0)
{
    static juce::String id;
    juce::OwnedArray<juce::PluginDescription> types;
   #if JUCE_MAC
    if (pluginPath.endsWith (".component") && ! pluginPath.contains ("/Library/Audio/Plug-Ins/"))
    {
        if (id.isEmpty()) id = registerBuiltAu (pluginPath);
        if (id.isEmpty()) { std::printf ("no se pudo registrar %s\n", pluginPath.toRawUTF8()); std::exit (2); }
        for (auto* f : formats().getFormats()) if (f->getName() == "AudioUnit") f->findAllTypesForFile (types, id);
    }
    else
   #endif
    for (auto* f : formats().getFormats())
        if (f->fileMightContainThisPluginType (pluginPath)) f->findAllTypesForFile (types, pluginPath);
    if (types.isEmpty()) { std::printf ("no se encontró un plugin en %s\n", pluginPath.toRawUTF8()); std::exit (2); }
    juce::String err;
    auto pl = std::make_unique<Plug>();
    pl->p = formats().createPluginInstance (*types[0], sr, maxBlock, err);
    if (pl->p == nullptr) { std::printf ("no se pudo cargar: %s\n", err.toRawUTF8()); std::exit (2); }
    pl->p->enableAllBuses();
    pl->sr = sr; pl->maxBlock = maxBlock; pl->ph.sr = sr;
    pl->p->setPlayHead (&pl->ph);
    pl->p->setRateAndBufferSizeDetails (sr, maxBlock);
    pl->p->prepareToPlay (sr, maxBlock);
    return pl;
}

juce::AudioProcessorParameter* param (Plug& pl, const juce::String& name)
{
    for (auto* p : pl.p->getParameters()) if (p->getName (64) == name) return p;
    std::printf ("no hay parámetro \"%s\"\n", name.toRawUTF8());
    std::exit (2);
}
void setChoice (Plug& pl, const juce::String& name, int index)
{
    auto* p = param (pl, name);
    const int n = std::max (2, p->getNumSteps());
    p->setValueNotifyingHost ((float) index / (float) (n - 1));
}

struct Ev { juce::int64 sample; juce::MidiMessage m; };
struct Rendered { std::vector<float> L, R; std::vector<Ev> midi; };

// Procesa `total` muestras desde la posición actual del playhead. blockOf(i) da el tamaño de cada bloque;
// onBlock (posición absoluta) corre antes de cada bloque (automatización).
Rendered render (Plug& pl, juce::int64 total, const std::vector<Ev>& in, const std::function<int (int)>& blockOf,
                 const std::function<void (juce::int64)>& onBlock = {})
{
    Rendered r; r.L.resize ((size_t) total); r.R.resize ((size_t) total);
    const int chans = std::max (2, std::max (pl.p->getTotalNumOutputChannels(), pl.p->getTotalNumInputChannels()));
    juce::AudioBuffer<float> buf (chans, pl.maxBlock);
    juce::MidiBuffer mb;
    const juce::int64 start = pl.ph.pos;
    juce::int64 done = 0;
    for (int b = 0; done < total; ++b)
    {
        const int n = (int) std::min<juce::int64> (total - done, juce::jlimit (1, pl.maxBlock, blockOf (b)));
        const juce::int64 t0 = start + done;
        if (onBlock) onBlock (t0);
        mb.clear();
        for (auto& e : in) if (e.sample >= t0 && e.sample < t0 + n) mb.addEvent (e.m, (int) (e.sample - t0));
        buf.setSize (chans, n, false, false, true);
        buf.clear();
        pl.p->processBlock (buf, mb);
        for (int i = 0; i < n; ++i) { r.L[(size_t) (done + i)] = buf.getSample (0, i); r.R[(size_t) (done + i)] = buf.getSample (1, i); }
        for (const auto md : mb) r.midi.push_back ({ t0 + md.samplePosition, md.getMessage() });
        done += n;
        pl.ph.pos += pl.ph.playing ? n : 0;
    }
    return r;
}

std::function<int (int)> fixed (int n) { return [n] (int) { return n; }; }

double nullDb (const Rendered& a, const Rendered& b)
{
    double d2 = 0, s2 = 0;
    const size_t n = std::min (a.L.size(), b.L.size());
    for (size_t i = 0; i < n; ++i)
    {
        const double dl = a.L[i] - b.L[i], dr = a.R[i] - b.R[i];
        d2 += dl * dl + dr * dr; s2 += (double) a.L[i] * a.L[i] + (double) a.R[i] * a.R[i];
    }
    if (d2 == 0.0) return -999.0;
    return 10.0 * std::log10 (d2 / std::max (1e-30, s2));
}

juce::String midiText (const std::vector<Ev>& v)
{
    juce::String s;
    for (auto& e : v) if (e.m.isNoteOnOrOff()) s << e.sample << (e.m.isNoteOn() ? "+" : "-") << e.m.getNoteNumber() << " ";
    return s;
}
int noteOns (const std::vector<Ev>& v) { int c = 0; for (auto& e : v) if (e.m.isNoteOn()) ++c; return c; }
int openNotes (const std::vector<Ev>& v)
{
    int on[16][128] = {};
    for (auto& e : v)
        if (e.m.isNoteOn()) on[e.m.getChannel() - 1][e.m.getNoteNumber()] = 1;
        else if (e.m.isNoteOff()) on[e.m.getChannel() - 1][e.m.getNoteNumber()] = 0;
    int c = 0; for (auto& ch : on) for (int k : ch) c += k;
    return c;
}
double rmsDb (const Rendered& r, juce::int64 a, juce::int64 b)
{
    double s = 0; juce::int64 n = 0;
    for (auto i = a; i < b && i < (juce::int64) r.L.size(); ++i, ++n) s += (double) r.L[(size_t) i] * r.L[(size_t) i];
    return 10.0 * std::log10 (s / std::max<juce::int64> (1, n) + 1e-30);
}

// El material de los renders del QA (material.lua, synth): acorde, notas cortas, una larga y silencio.
std::vector<Ev> qaSynthMaterial (double sr)
{
    const double t[][4] = { {0.0, 2.0, 48, 100}, {0.0, 2.0, 52, 100}, {0.0, 2.0, 55, 100},
        {2.2, 0.2, 60, 90}, {2.45, 0.2, 62, 70}, {2.7, 0.2, 64, 110}, {2.95, 0.2, 65, 60},
        {3.2, 0.2, 67, 90}, {3.45, 0.2, 69, 127}, {3.7, 0.2, 71, 80}, {3.95, 0.2, 72, 100}, {4.5, 1.0, 57, 127} };
    std::vector<Ev> v;
    for (auto& e : t)
    {
        v.push_back ({ (juce::int64) std::lround (e[0] * sr), juce::MidiMessage::noteOn (1, (int) e[2], (juce::uint8) e[3]) });
        v.push_back ({ (juce::int64) std::lround ((e[0] + e[1]) * sr), juce::MidiMessage::noteOff (1, (int) e[2]) });
    }
    std::stable_sort (v.begin(), v.end(), [] (const Ev& a, const Ev& b) { return a.sample < b.sample; });
    return v;
}

// Acid Line (programa 16) con el reloj del host, como job_bs.lua del QA.
void setupAcid (Plug& pl) { pl.p->setCurrentProgram (16); setChoice (pl, "Clock", 1); }

// ───────── pruebas ─────────
void t1Program0()
{
    auto a = load(); auto b = load();
    b->p->setCurrentProgram (0);
    int diffs = 0; juce::String which;
    auto& pa = a->p->getParameters(); auto& pb = b->p->getParameters();
    for (int i = 0; i < pa.size() && i < pb.size(); ++i)
        if (std::abs (pa[i]->getValue() - pb[i]->getValue()) > 1e-6f) { ++diffs; which << pa[i]->getName (32) << " "; }
    const std::vector<Ev> notes { { 2400, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100) }, { 40000, juce::MidiMessage::noteOff (1, 48) } };
    a->ph.playing = b->ph.playing = true;
    const auto ra = render (*a, 96000, notes, fixed (512)), rb = render (*b, 96000, notes, fixed (512));
    const double nd = nullDb (ra, rb);
    result ("T1", diffs == 0 && nd < -120.0 && a->p->getCurrentProgram() == 0,
            juce::String::formatted ("fresh vs setCurrentProgram(0): %d parameter diffs, null %.1f dB, program %d \"%s\" ",
                                     diffs, nd, a->p->getCurrentProgram(), a->p->getProgramName (a->p->getCurrentProgram()).toRawUTF8()) + which);
}

void t2Fresh()
{
    // REAPER al arrancar un render: VST3 → setActive(false/true) (prepareToPlay), AU → AudioUnitReset (reset()).
    // AudioPluginInstance::reset() del host de JUCE hace eso mismo en cada formato.
    const auto mat = qaSynthMaterial (48000.0);
    Rendered r[2];
    for (int k = 0; k < 2; ++k)
    {
        auto pl = load (512);
        setupAcid (*pl);
        if (k == 1)      // la segunda estuvo andando parada (secuenciador libre, ruido, deriva) antes del render
        {
            pl->ph.playing = false;
            render (*pl, 48000 * 3 + 777, {}, fixed (256));
            pl->p->setPlayHead (&pl->ph);
        }
        pl->p->reset();
        pl->ph.playing = true; pl->ph.pos = 0;
        r[k] = render (*pl, 48000 * 10, mat, fixed (512));
    }
    const double nd = nullDb (r[0], r[1]);
    // La que ya tocó cierra al empezar las notas que había dejado abiertas en el MIDI out (note-off sin note-on en
    // este render): eso es lo correcto y no cuenta como diferencia.
    std::vector<Ev> r1;
    { std::array<bool, 128> on {}; for (auto& e : r[1].midi) { if (e.m.isNoteOn()) on[(size_t) e.m.getNoteNumber()] = true;
                                                                else if (e.m.isNoteOff() && ! on[(size_t) e.m.getNoteNumber()]) continue;
                                                                else if (e.m.isNoteOff()) on[(size_t) e.m.getNoteNumber()] = false;
                                                                r1.push_back (e); } }
    const bool sameMidi = midiText (r[0].midi) == midiText (r1);
    if (! sameMidi)      // el primer evento distinto, para ver qué arrastra la instancia que ya tocó
    {
        const auto a = juce::StringArray::fromTokens (midiText (r[0].midi), " ", ""), b = juce::StringArray::fromTokens (midiText (r1), " ", "");
        for (int i = 0; i < std::max (a.size(), b.size()); ++i)
            if (a[i] != b[i]) { std::printf ("     first MIDI difference at event %d: '%s' vs '%s'\n", i, a[i].toRawUTF8(), b[i].toRawUTF8()); break; }
    }
    result ("T2", nd < -100.0 && sameMidi,
            juce::String::formatted ("two fresh instances (one ran 3 s stopped first) after reset(): null %.1f dB, MIDI out %d vs %d note-ons%s",
                                     nd, noteOns (r[0].midi), noteOns (r[1].midi), sameMidi ? ", identical" : ", DIFFERENT"));
}

void t3Buffers()
{
    const auto mat = qaSynthMaterial (48000.0);
    auto run = [&] (std::function<int (int)> blocks) {
        auto pl = load (1024);
        setupAcid (*pl);
        pl->p->reset();
        pl->ph.playing = true; pl->ph.pos = 0;
        return render (*pl, 48000 * 10, mat, blocks);
    };
    const auto ref = run (fixed (512));
    juce::String msg = juce::String::formatted ("512: %d note-ons;", noteOns (ref.midi));
    bool ok = noteOns (ref.midi) > 60;
    std::vector<std::pair<juce::String, std::function<int (int)>>> cases {
        { "64", fixed (64) }, { "333", fixed (333) }, { "1024", fixed (1024) }, { "768", fixed (768) } };
    for (unsigned seed : { 1u, 2u, 3u })
    {
        auto rng = std::make_shared<std::mt19937> (seed);
        cases.push_back ({ "rand" + juce::String (seed), [rng] (int) { return 1 + (int) ((*rng)() % 1024); } });
    }
    for (auto& c : cases)
    {
        const auto r = run (c.second);
        const bool same = midiText (r.midi) == midiText (ref.midi);
        const double nd = nullDb (ref, r);
        ok = ok && same && nd < -100.0;
        msg << " " << c.first << ": " << noteOns (r.midi) << (same ? " same" : " DIFF") << juce::String::formatted (" null %.0f dB;", nd);
    }
    result ("T3", ok, msg);
}

void t4State()
{
    auto a = load();
    juce::MemoryBlock s1, s2, s3;
    a->p->getStateInformation (s1);
    a->p->getStateInformation (s2);
    auto b = load();
    b->p->setStateInformation (s1.getData(), (int) s1.getSize());
    b->p->getStateInformation (s3);
    // Con un preset cargado y un cambio, guardado → otra instancia → guardado.
    auto c = load();
    c->p->setCurrentProgram (16);
    param (*c, "VCF Freq")->setValueNotifyingHost (0.33f);
    juce::MemoryBlock c1, c2, c3;
    c->p->getStateInformation (c1);
    auto d = load();
    d->p->setStateInformation (c1.getData(), (int) c1.getSize());
    d->p->getStateInformation (c2);
    d->p->setStateInformation (c2.getData(), (int) c2.getSize());
    d->p->getStateInformation (c3);
    const bool ok = s1 == s2 && s1 == s3 && c1 == c2 && c2 == c3;
    result ("T4", ok, juce::String::formatted ("state bytes: fresh twice %s, fresh→load→save %s, edited preset→load→save %s, again %s (%d bytes)",
                                                s1 == s2 ? "equal" : "DIFFER", s1 == s3 ? "equal" : "DIFFER",
                                                c1 == c2 ? "equal" : "DIFFER", c2 == c3 ? "equal" : "DIFFER", (int) c1.getSize()));
}

void t5Gap()
{
    // a) transporte andando: nota, el host deja de llamar 1 s (REAPER en bypass) y el note-off se pierde ahí.
    {
        auto pl = load (512);
        pl->ph.playing = true;
        const std::vector<Ev> on { { 4800, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 110) } };
        auto r1 = render (*pl, 24000, on, fixed (512));
        pl->ph.pos += 48000;                                    // bloques que el host no mandó
        auto r2 = render (*pl, 48000 * 3, {}, fixed (512));
        std::vector<Ev> all = r1.midi; all.insert (all.end(), r2.midi.begin(), r2.midi.end());
        const double tail = rmsDb (r2, 48000 * 2, 48000 * 3);
        result ("T5a", tail < -90.0 && openNotes (all) == 0,
                juce::String::formatted ("playing, 1 s host gap over the note-off: level 2-3 s after %.1f dB, MIDI out open notes %d",
                                         tail, openNotes (all)));
    }
    // b) transporte parado, en tiempo real: 400 ms sin llamadas.
    {
        auto pl = load (512);
        pl->ph.playing = false;
        const std::vector<Ev> on { { 1000, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 110) } };
        auto r1 = render (*pl, 9600, on, fixed (512));
        std::this_thread::sleep_for (std::chrono::milliseconds (400));
        auto r2 = render (*pl, 48000 * 3, {}, fixed (512));
        std::vector<Ev> all = r1.midi; all.insert (all.end(), r2.midi.begin(), r2.midi.end());
        const double tail = rmsDb (r2, 48000 * 2, 48000 * 3);
        result ("T5b", tail < -90.0 && openNotes (all) == 0,
                juce::String::formatted ("stopped, 400 ms wall-clock gap: level 2-3 s after %.1f dB, MIDI out open notes %d", tail, openNotes (all)));
    }
}

void t6Latency()
{
    for (int bs : { 64, 512 })
    {
        auto pl = load (1024);
        pl->ph.playing = true;
        const int lat = pl->p->getLatencySamples();
        const juce::int64 at = 24000 + 17;
        const std::vector<Ev> notes { { at, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 110) }, { at + 12000, juce::MidiMessage::noteOff (1, 57) } };
        const auto r = render (*pl, 48000, notes, fixed (bs));
        juce::int64 midiAt = -1, midiOff = -1;
        for (auto& e : r.midi) { if (e.m.isNoteOn() && midiAt < 0) midiAt = e.sample; if (e.m.isNoteOff() && midiOff < 0) midiOff = e.sample; }
        // Arranque del audio: 50 % del pico de los primeros 20 ms (la nota de Init arranca con ataque 0).
        float pk = 0; for (auto i = at; i < at + 960; ++i) pk = std::max (pk, std::abs (r.L[(size_t) i]));
        juce::int64 onset = -1; for (auto i = at; i < at + 960; ++i) if (std::abs (r.L[(size_t) i]) >= 0.5f * pk) { onset = i; break; }
        const bool ok = midiAt - at == lat && midiOff - (at + 12000) == lat;
        result ("T6", ok, juce::String::formatted ("block %d: latency %d; MIDI-out note-on at +%d, note-off at +%d, audio onset (50%% of peak) at +%d samples after the input note",
                                                    bs, lat, (int) (midiAt - at), (int) (midiOff - at - 12000), (int) (onset - at)));
    }
}

void writeWav (const juce::File& f, const Rendered& r)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), 48000.0, 2, 32, {}, 0));
    if (w == nullptr) return;
    const float* ch[2] = { r.L.data(), r.R.data() };
    w->writeFromFloatArrays (ch, 2, (int) r.L.size());
}

void t7Zipper (const juce::File& outDir)
{
    // job_dyn.lua del QA: instancia nueva, una nota sostenida (57, 0-6 s), el parámetro en 0.3; automatización:
    // salto a 0.7 en 1 s, rampa lineal 2→4 s hasta 0.3, salto a 0.6 en 5 s. REAPER la aplica por bloque (512).
    outDir.createDirectory();
    for (auto name : { "Mixer Saw", "Mixer Pulse" })
    {
        for (int automated = 0; automated < 2; ++automated)
        {
            auto pl = load (512);
            pl->ph.playing = true;
            auto* prm = param (*pl, name);
            prm->setValueNotifyingHost (0.3f);
            const std::vector<Ev> notes { { 0, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100) }, { 288000, juce::MidiMessage::noteOff (1, 57) } };
            auto env = [] (double t) {
                if (t < 1.0) return 0.3; if (t < 2.0) return 0.7; if (t < 4.0) return 0.7 - 0.4 * (t - 2.0) / 2.0;
                if (t < 5.0) return 0.3; return 0.6; };
            const auto r = render (*pl, 312000, notes, fixed (512), [&] (juce::int64 t0) {
                if (automated) prm->setValueNotifyingHost ((float) env ((double) t0 / 48000.0)); });
            const auto file = outDir.getChildFile (juce::String (automated ? "auto_env_" : "auto_static_") + juce::String (name).replace (" ", "_") + ".wav");
            writeWav (file, r);
        }
    }
    std::printf ("INFO T7         WAVs in %s (analyse with zipper.py)\n", outDir.getFullPathName().toRawUTF8());
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    if (args.isEmpty()) { std::printf ("uso: at101vhostcheck <AT101V.vst3|.component> [--tests program0,fresh,...] [--out dir]\n"); return 2; }
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };
    pluginPath = juce::File::getCurrentWorkingDirectory().getChildFile (args[0]).getFullPathName();
    const auto tests = juce::StringArray::fromTokens (opt ("--tests", "program0,fresh,buffers,state,gap,latency,zipper"), ",", "");
    const juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile (opt ("--out", "hostcheck-out"));
    std::printf ("at101vhostcheck %s\n", pluginPath.toRawUTF8());
    for (auto& t : tests)
    {
        if (t == "program0") t1Program0();
        else if (t == "fresh") t2Fresh();
        else if (t == "buffers") t3Buffers();
        else if (t == "state") t4State();
        else if (t == "gap") t5Gap();
        else if (t == "latency") t6Latency();
        else if (t == "zipper") t7Zipper (outDir);
        else std::printf ("prueba desconocida: %s\n", t.toRawUTF8());
    }
    std::printf (failures == 0 ? "all PASS\n" : "%d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
