// athosttest: prueba un plugin como lo usaría un DAW. Carga el VST3 (o AU) compilado, le manda MIDI y audio,
// mueve el transporte y los parámetros, y revisa lo que sale: NaN, picos, DC, notas colgadas, afinación según la
// frecuencia de muestreo, estado guardado y restaurado, MIDI de salida balanceado, CPU y denormales.
//
//   athosttest <plugin.vst3|.component> [--tests a,b,...] [--out dir] [--quick]
//   (un .component fuera de las carpetas de Plug-Ins se registra en el proceso: se prueba el compilado, no el instalado)
//   athosttest --chain <generador.vst3> <sinte.vst3> [--gen-programs all|0,3] [--synth-program n] [--secs 120] [--out dir]
//
// Pruebas: info programs midi rates state fuzz params cpu reprepare midiout midisync stopped bypass hostgap freeze roles ports (por
// defecto, todas; roles y ports sólo hacen algo con Orrery: Main y satélites, puertos virtuales). Aparte, con --tests:
// editor (CPU del editor con la ventana escondida, como lo deja Live; necesita una sesión gráfica), statebytes (estado
// estable byte a byte, instancia nueva = programa 0), repro (instancias nuevas, re-preparadas y con bloques al azar suenan igual),
// midilatency (la nota de MIDI out sale atrasada la latencia), midifx (notas y pedal a un efecto cambian la salida;
// --fx-program n), progorder (un programa deja los mismos parámetros venga del programa que venga)
// Cada resultado es PASS, WARN (para mirar a mano) o FAIL. Se escribe <out>/<plugin>.json y, de los casos con
// problemas, un WAV para escucharlos. Sale con 1 si hubo algún FAIL.

#if defined (__APPLE__)
 #include <AudioToolbox/AudioToolbox.h>     // antes de JuceHeader (que hace using namespace juce)
#endif
#include <JuceHeader.h>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <thread>
#if ! JUCE_WINDOWS
 #include <sys/resource.h>
#endif

using Clock = std::chrono::steady_clock;

namespace {

// Texto: los literales del fuente son UTF-8 y juce::String (const char*) los toma como ASCII.
juce::String U (const char* t) { return juce::String::fromUTF8 (t); }
juce::String fmt (const char* f, ...)
{
    char buf[2048];
    va_list ap; va_start (ap, f); std::vsnprintf (buf, sizeof (buf), f, ap); va_end (ap);
    return juce::String::fromUTF8 (buf);
}

// ───────── transporte del host ─────────
struct PlayHead : juce::AudioPlayHead
{
    double sr = 48000.0, bpm = 120.0, ppq = 0.0; bool playing = false; juce::int64 pos = 0;
    juce::uint64 hostNs = 0; bool hasHostNs = false;     // la hora del sistema por ciclo (VST3 systemTime)
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo pi;
        pi.setBpm (bpm); pi.setIsPlaying (playing); pi.setPpqPosition (ppq); pi.setTimeInSamples (pos);
        pi.setTimeInSeconds ((double) pos / sr); pi.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        pi.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0); pi.setIsLooping (false);
        if (hasHostNs) pi.setHostTimeNs (hostNs);
        return pi;
    }
};

struct Ev { juce::int64 sample; juce::MidiMessage m; };
struct Transport { juce::int64 sample; int action; double value; };     // 0 play, 1 stop, 2 saltar a ppq, 3 tempo

struct Plug
{
    std::unique_ptr<juce::AudioPluginInstance> p;
    PlayHead ph;
    double sr = 48000.0; int block = 512;
    int ins = 0, outs = 2;
};

juce::AudioPluginFormatManager& formats()
{
    static juce::AudioPluginFormatManager fm;
    static bool init = false;
    if (! init) { juce::addDefaultFormatsToManager (fm); init = true; }
    return fm;
}

juce::String pluginPath;

#if JUCE_MAC
// Un .component recién compilado (fuera de las carpetas de Plug-Ins): el host de AU de JUCE busca por tipo, subtipo y
// fabricante, así que con un path cargaba la versión INSTALADA del mismo plugin sin avisar. Se registra el binario
// compilado en este proceso con el fabricante 'Atqa' y se lo carga por ese identificador; la instalada no se toca.
juce::String registerBuiltAu (const juce::String& path)
{
    CFURLRef url = CFURLCreateWithFileSystemPath (nullptr, path.toCFString(), kCFURLPOSIXPathStyle, true);
    CFBundleRef bundle = CFBundleCreate (nullptr, url);                 // queda cargado mientras dure el proceso
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
    if (AudioComponentRegister (&desc, (U ("Audio Tools QA: ") + str (CFSTR ("name"))).toCFString(), version, fn) == nullptr) return {};
    const auto type = str (CFSTR ("type"));
    return "AudioUnit:" + juce::String (type == "aumu" ? "Synths/" : "Effects/") + type + "," + str (CFSTR ("subtype")) + ",Atqa";
}
#endif


std::unique_ptr<Plug> load (const juce::String& path, double sr = 48000.0, int block = 512)
{
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* f : formats().getFormats())
        if (f->fileMightContainThisPluginType (path)) f->findAllTypesForFile (types, path);
    if (types.isEmpty()) { std::printf ("no se encontró un plugin en %s\n", path.toRawUTF8()); std::exit (2); }
    juce::String err;
    auto pl = std::make_unique<Plug>();
    pl->p = formats().createPluginInstance (*types[0], sr, block, err);
    if (pl->p == nullptr) { std::printf ("no se pudo cargar: %s\n", err.toRawUTF8()); std::exit (2); }
    pl->p->enableAllBuses();
    pl->sr = sr; pl->block = block; pl->ph.sr = sr;
    pl->p->setPlayHead (&pl->ph);
    pl->p->setRateAndBufferSizeDetails (sr, block);
    pl->p->prepareToPlay (sr, block);
    pl->ins = pl->p->getTotalNumInputChannels(); pl->outs = pl->p->getTotalNumOutputChannels();
    return pl;
}

// ───────── render ─────────
struct Out
{
    std::vector<float> L, R;
    std::vector<Ev> midi;
    int badOffsets = 0;
    double totalMs = 0.0, maxBlockMs = 0.0; int blocks = 0;
    std::vector<double> blockMs;      // por bloque (para la prueba de denormales)
};

struct RenderOpts
{
    int block = 0;                     // 0 = el del plugin
    bool randomBlocks = false;         // tamaños al azar entre 1 y block
    std::function<void (juce::int64, int)> perBlock;   // (posición, tamaño) antes de procesar
    std::function<float (juce::int64, int)> audioIn;   // (muestra, canal) → entrada, si el plugin tiene entradas
    std::vector<Transport> transport;
    bool keepBlockTimes = false;
    bool frozenPosition = false;       // el host parado que sigue procesando: la misma posición en cada bloque
};

Out render (Plug& pl, std::vector<Ev> events, juce::int64 total, const RenderOpts& o = {})
{
    std::stable_sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.sample < b.sample; });
    Out out; out.L.reserve ((size_t) total); out.R.reserve ((size_t) total);
    const int maxN = o.block > 0 ? o.block : pl.block;
    juce::AudioBuffer<float> buf (std::max ({ 2, pl.ins, pl.outs }), maxN);
    juce::MidiBuffer mb;
    juce::Random rnd (1234);
    size_t ei = 0, ti = 0;
    for (juce::int64 pos = 0; pos < total;)
    {
        int n = o.randomBlocks ? 1 + rnd.nextInt (maxN) : maxN;
        n = (int) std::min<juce::int64> (n, total - pos);
        while (ti < o.transport.size() && o.transport[ti].sample <= pos)
        {
            const auto& t = o.transport[ti++];
            if (t.action == 0) pl.ph.playing = true;
            else if (t.action == 1) pl.ph.playing = false;
            else if (t.action == 2) pl.ph.ppq = t.value;
            else pl.ph.bpm = t.value;
        }
        if (o.perBlock) o.perBlock (pos, n);
        mb.clear();
        while (ei < events.size() && events[ei].sample < pos + n)
        {
            mb.addEvent (events[ei].m, (int) std::max<juce::int64> (0, events[ei].sample - pos));
            ++ei;
        }
        buf.setSize (buf.getNumChannels(), n, false, false, true);
        buf.clear();
        if (o.audioIn && pl.ins > 0)
            for (int c = 0; c < pl.ins; ++c)
                for (int i = 0; i < n; ++i) buf.setSample (c, i, o.audioIn (pos + i, c));
        pl.ph.pos = o.frozenPosition ? 0 : pos;
        // Bloques de 0 muestras: algunos hosts VST3 los mandan. A un AU no: AudioUnitRender con 0 cuadros deja la hora
        // de render en el mismo lugar y el AU (AUBase::NeedsToRender) devuelve el bloque siguiente sin procesarlo, con
        // sus notas; el ATFS1 AU salía desafinado 123 cents por eso. Ningún host de AU lo hace.
        static const bool noZero = juce::SystemStats::getEnvironmentVariable ("HT_NOZERO", "").isNotEmpty();   // para aislar fallas
        if (o.randomBlocks && ! noZero && pl.p->getPluginDescription().pluginFormatName != "AudioUnit" && rnd.nextInt (16) == 0)
        {
            juce::AudioBuffer<float> empty (buf.getNumChannels(), 0); juce::MidiBuffer none;
            pl.p->processBlock (empty, none);
        }
        const auto t0 = Clock::now();
        pl.p->processBlock (buf, mb);
        const double ms = std::chrono::duration<double, std::milli> (Clock::now() - t0).count();
        out.totalMs += ms; out.maxBlockMs = std::max (out.maxBlockMs, ms); ++out.blocks;
        if (o.keepBlockTimes) out.blockMs.push_back (ms);
        const float* l = buf.getReadPointer (0);
        const float* r = buf.getReadPointer (pl.outs > 1 ? 1 : 0);
        out.L.insert (out.L.end(), l, l + n); out.R.insert (out.R.end(), r, r + n);
        for (const auto md : mb)
        {
            if (md.samplePosition < 0 || md.samplePosition >= n) ++out.badOffsets;
            out.midi.push_back ({ pos + md.samplePosition, md.getMessage() });
        }
        if (pl.ph.playing && ! o.frozenPosition) pl.ph.ppq += n * pl.ph.bpm / 60.0 / pl.sr;
        pos += n;
    }
    // Los cambios de transporte que caen justo al final (un stop en `total`) también se aplican: si no, el host quedaba
    // reproduciendo para la prueba siguiente y una caja de ritmos que sigue al host tocaba sola.
    while (ti < o.transport.size() && o.transport[ti].sample <= total)
    {
        const auto& t = o.transport[ti++];
        if (t.action == 0) pl.ph.playing = true;
        else if (t.action == 1) pl.ph.playing = false;
        else if (t.action == 2) pl.ph.ppq = t.value;
        else pl.ph.bpm = t.value;
    }
    return out;
}

// ───────── análisis ─────────
struct Stats { double peak = 0, rms = 0, mean = 0; int nan = 0, inf = 0; };
Stats stats (const Out& o, double fromSec, double toSec, double sr)
{
    Stats s;
    const size_t a = (size_t) std::clamp (fromSec * sr, 0.0, (double) o.L.size()), b = (size_t) std::clamp (toSec * sr, 0.0, (double) o.L.size());
    double sum = 0, sq = 0; size_t n = 0;
    for (auto* v : { &o.L, &o.R })
        for (size_t i = a; i < b; ++i)
        {
            const float x = (*v)[i];
            if (std::isnan (x)) { ++s.nan; continue; }
            if (std::isinf (x)) { ++s.inf; continue; }
            s.peak = std::max (s.peak, (double) std::abs (x)); sum += x; sq += (double) x * x; ++n;
        }
    if (n > 0) { s.mean = sum / n; s.rms = std::sqrt (sq / n); }
    return s;
}
double db (double v) { return v > 1e-12 ? 20.0 * std::log10 (v) : -240.0; }
double durSec (const Out& o, double sr) { return (double) o.L.size() / sr; }

// Frecuencia fundamental (YIN) de un tramo del canal izquierdo.
double pitchOf (const Out& o, double fromSec, double sr)
{
    const int W = (int) (0.05 * sr), maxTau = (int) (sr / 30.0), minTau = (int) (sr / 2500.0);
    const size_t a = (size_t) (fromSec * sr);
    if (a + (size_t) (W + maxTau) >= o.L.size()) return 0.0;
    std::vector<double> d ((size_t) maxTau + 1, 0.0);
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double s = 0;
        for (int i = 0; i < W; ++i) { const double x = o.L[a + (size_t) i] - o.L[a + (size_t) (i + tau)]; s += x * x; }
        d[(size_t) tau] = s;
    }
    double run = 0; std::vector<double> cm ((size_t) maxTau + 1, 1.0);
    for (int tau = 1; tau <= maxTau; ++tau) { run += d[(size_t) tau]; cm[(size_t) tau] = run > 0 ? d[(size_t) tau] * tau / run : 1.0; }
    int best = -1;
    for (int tau = minTau; tau < maxTau; ++tau)
        if (cm[(size_t) tau] < 0.12) { while (tau + 1 < maxTau && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau; best = tau; break; }
    if (best < 0) { double m = 1e9; for (int tau = minTau; tau < maxTau; ++tau) if (cm[(size_t) tau] < m) { m = cm[(size_t) tau]; best = tau; } if (m > 0.4) return 0.0; }
    const double y0 = cm[(size_t) best - 1], y1 = cm[(size_t) best], y2 = cm[(size_t) best + 1];
    const double den = y0 - 2 * y1 + y2;
    const double t = den != 0 ? best + 0.5 * (y0 - y2) / den : best;
    return sr / t;
}

// Notas de salida que quedaron prendidas (por canal y nota). Como en un sinte MIDI: un note-off apaga la nota
// aunque haya llegado dos veces el note-on.
std::vector<std::pair<int, int>> stuckNotes (const std::vector<Ev>& midi)
{
    std::map<std::pair<int, int>, int> on;
    for (auto& e : midi)
    {
        if (e.m.isAllNotesOff() || e.m.isAllSoundOff()) { for (auto& kv : on) if (kv.first.first == e.m.getChannel()) kv.second = 0; continue; }
        if (e.m.isNoteOn()) on[{ e.m.getChannel(), e.m.getNoteNumber() }] = 1;
        else if (e.m.isNoteOff()) on[{ e.m.getChannel(), e.m.getNoteNumber() }] = 0;
    }
    std::vector<std::pair<int, int>> v;
    for (auto& kv : on) if (kv.second > 0) v.push_back (kv.first);
    return v;
}

// ───────── reporte ─────────
struct Result { juce::String test, name, status, detail; };
std::vector<Result> results;
juce::File outDir;
juce::String plugName;
int fails = 0, warns = 0;

void report (const juce::String& test, const juce::String& name, const juce::String& status, const juce::String& detail)
{
    results.push_back ({ test, name, status, detail });
    if (status == "FAIL") ++fails; else if (status == "WARN") ++warns;
    std::printf ("  [%s] %-10s %-34s %s\n", status.toRawUTF8(), test.toRawUTF8(), name.toRawUTF8(), detail.toRawUTF8());
    std::fflush (stdout);
}

void writeWav (const Out& o, double sr, const juce::String& tag)
{
    auto f = outDir.getChildFile (plugName + "_" + tag.replaceCharacters (" /:", "___") + ".wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    auto os = f.createOutputStream();
    if (os == nullptr) return;
    auto w = wav.createWriterFor (os.release(), sr, 2, 24, {}, 0);
    if (w == nullptr) return;
    juce::AudioBuffer<float> b (2, (int) o.L.size());
    for (size_t i = 0; i < o.L.size(); ++i)
    {
        auto clean = [] (float x) { return std::isfinite (x) ? juce::jlimit (-4.0f, 4.0f, x) : 0.0f; };
        b.setSample (0, (int) i, clean (o.L[i])); b.setSample (1, (int) i, clean (o.R[i]));
    }
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    delete w;
}

// MIDI de ayuda.
juce::int64 S (double sec, double sr) { return (juce::int64) std::llround (sec * sr); }
juce::MidiMessage on (int n, int v = 100, int ch = 1) { return juce::MidiMessage::noteOn (ch, n, (juce::uint8) v); }
juce::MidiMessage off (int n, int ch = 1, int v = 0) { return juce::MidiMessage::noteOff (ch, n, (juce::uint8) v); }
juce::MidiMessage cc (int c, int v, int ch = 1) { return juce::MidiMessage::controllerEvent (ch, c, v); }

// Los parámetros del plugin, sin los "MIDI CC n|m" que el envoltorio VST3 de JUCE agrega para emular controladores
// ni el de cambio de programa.
juce::Array<juce::AudioProcessorParameter*> realParams (Plug& pl)
{
    juce::Array<juce::AudioProcessorParameter*> v;
    for (auto* p : pl.p->getParameters())
    {
        const auto n = p->getName (64);
        if (n.startsWith ("MIDI CC ") || n == "Program") continue;
        v.add (p);
    }
    return v;
}

// Efecto: tiene entrada y no es un instrumento. Murmur acepta MIDI (Pitch, Gate, el pedal) pero sin audio de
// entrada no suena; AT20V FX también recibe MIDI y toca solo, así que se lo sigue probando como instrumento.
bool isEffect (Plug& pl) { return pl.ins > 0 && (! pl.p->acceptsMidi() || (! pl.p->getPluginDescription().isInstrument && ! pl.p->getName().containsIgnoreCase ("AT20V"))); }

// Entrada de audio para los efectos: ruido rosado suave y un seno, sólo durante [0, hasta).
std::function<float (juce::int64, int)> testInput (double sr, double untilSec)
{
    auto state = std::make_shared<std::array<float, 3>> (std::array<float, 3> { 0, 0, 0 });
    auto rng = std::make_shared<juce::Random> (77);
    return [=] (juce::int64 i, int ch) -> float {
        if ((double) i / sr >= untilSec) return 0.0f;
        const float w = rng->nextFloat() * 2.0f - 1.0f;
        auto& s = *state; s[(size_t) ch % 3] = 0.97f * s[(size_t) ch % 3] + 0.03f * w;
        return 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr) + 2.0f * s[(size_t) ch % 3];
    };
}

double waitSec (Plug& pl) { return juce::jlimit (4.0, 12.0, pl.p->getTailLengthSeconds() + 4.0); }

// Nivel de reposo: el mismo render sin notas (algunos instrumentos suenan solos: drones, autooscilación).
double restLevelDb (const juce::String& path, double secs)
{
    auto pl = load (path);
    RenderOpts o;
    auto r = render (*pl, {}, S (secs, pl->sr), o);
    return db (stats (r, secs - 0.5, secs, pl->sr).rms);
}

// Corre un escenario MIDI en una instancia nueva y revisa NaN, picos, notas colgadas y MIDI de salida.
void scenario (const juce::String& name, std::vector<Ev> ev, double lastEventSec, double restDb, bool expectSilence = true,
               std::function<void (Plug&)> setup = {}, RenderOpts o = {})
{
    auto pl = load (pluginPath);
    if (setup) setup (*pl);
    const double total = lastEventSec + waitSec (*pl);
    if (isEffect (*pl)) o.audioIn = testInput (pl->sr, lastEventSec);
    auto r = render (*pl, ev, S (total, pl->sr), o);
    const auto all = stats (r, 0, total, pl->sr);
    const auto end = stats (r, total - 0.5, total, pl->sr);
    juce::String d = fmt ("pico %.2f, final %.0f dB (reposo %.0f)", all.peak, db (end.rms), restDb);
    juce::String status = "PASS";
    if (all.nan + all.inf > 0) { status = "FAIL"; d << ", " << all.nan << " NaN, " << all.inf << " Inf"; }
    else if (all.peak > 4.0) { status = "FAIL"; d << ", pico excesivo"; }
    else if (all.peak > 1.6) status = "WARN";
    if (expectSilence && db (end.rms) > std::max (restDb + 6.0, -70.0))
    {
        // Una cola declarada más larga que la espera (la reverb larga del sonido de prueba de Orrery) puede seguir
        // sonando, si decae: una nota colgada se sostiene.
        const double tail = pl->p->getTailLengthSeconds(), before = db (stats (r, total - 4.5, total - 4.0, pl->sr).rms);
        if (tail > waitSec (*pl) && before - db (end.rms) > 6.0) d << fmt (", cola declarada de %.0f s (decae %.0f dB en 4 s)", tail, before - db (end.rms));
        // Con parámetros al azar un release largo (un RELEASE al máximo mientras se suelta una tecla) deja una cola
        // que decae: no es una nota colgada, que se sostiene. Se avisa.
        else if (name.startsWith (U ("parámetros al azar")) && before - db (end.rms) > 6.0)
        { status = status == "FAIL" ? status : "WARN"; d << fmt (", cola larga que decae %.0f dB en 4 s", before - db (end.rms)); }
        else { status = "FAIL"; d << U (", sigue sonando (¿nota colgada?)"); }
    }
    if (pl->p->producesMidi())
    {
        auto st = stuckNotes (r.midi);
        if (! st.empty()) { status = "FAIL"; d << ", MIDI out con " << (int) st.size() << " notas sin note-off"; }
        if (r.badOffsets > 0) { status = "FAIL"; d << ", " << r.badOffsets << " eventos fuera del bloque"; }
    }
    if (status != "PASS")
    {
        writeWav (r, pl->sr, "midi_" + name);
        if (pl->p->producesMidi())     // el MIDI de salida, para ver qué nota quedó
        {
            juce::String txt;
            for (auto& e : r.midi) txt << juce::String ((double) e.sample / pl->sr, 4) << "  " << e.m.getDescription() << "\n";
            outDir.getChildFile (plugName + "_midi_" + name.replaceCharacters (" /:", "___") + ".txt").replaceWithText (txt);
        }
    }
    report ("midi", name, status, d);
}

// ───────── pruebas ─────────
void testInfo()
{
    auto pl = load (pluginPath);
    auto& p = *pl->p;
    report ("info", p.getName(), "PASS", fmt ("%d in, %d out, MIDI in %d out %d, %d programas, %d parámetros, latencia %d, cola %.1f s",
            pl->ins, pl->outs, (int) p.acceptsMidi(), (int) p.producesMidi(), p.getNumPrograms(), realParams (*pl).size(), p.getLatencySamples(), p.getTailLengthSeconds()));
}

void testGeneratorPrograms (bool quick)
{
    auto pl = load (pluginPath);
    auto& p = *pl->p;
    const double sr = pl->sr;
    for (int k = 0; k < p.getNumPrograms(); ++k)
    {
        if (quick && k > 3) break;
        p.setCurrentProgram (k);
        juce::AudioProcessorParameter* play = nullptr;
        for (auto* prm : realParams (*pl)) if (prm->getName (32) == "Play") play = prm;
        if (play != nullptr) play->setValue (1.0f);
        // A los 20 s se apaga Play y se para el transporte (Orrery sigue con el host parado a propósito).
        RenderOpts o; o.transport = { { 0, 0, 0 }, { S (20.0, sr), 1, 0 } };
        o.perBlock = [=] (juce::int64 pos, int) { if (play != nullptr && pos >= S (20.0, sr)) play->setValue (0.0f); };
        pl->ph.ppq = 0;
        auto r = render (*pl, {}, S (30.0, sr), o);
        auto st = stuckNotes (r.midi);
        int notes = 0, late = 0; std::set<int> chans;
        for (auto& e : r.midi) { if (e.m.isNoteOn()) { ++notes; chans.insert (e.m.getChannel()); if (e.sample > S (20.5, sr)) ++late; } }
        const auto a = stats (r, 0, 30.0, sr);
        juce::String status = ! st.empty() || r.badOffsets > 0 || a.nan + a.inf > 0 ? "FAIL" : (notes == 0 || late > 0 ? "WARN" : "PASS");
        juce::String d = fmt ("%d notas en %d canales en 20 s, %d sin note-off al parar, %d notas después del stop, %d fuera del bloque",
                              notes, (int) chans.size(), (int) st.size(), late, r.badOffsets);
        report ("programs", juce::String (k) + " " + p.getProgramName (k), status, d);
        render (*pl, {}, S (0.5, sr), {});
    }
}

bool isGenerator (Plug& pl) { return pl.p->producesMidi() && pl.p->getName().containsIgnoreCase ("orrery"); }

void testPrograms (bool quick)
{
    auto pl = load (pluginPath);
    if (isGenerator (*pl)) { pl.reset(); testGeneratorPrograms (quick); return; }
    auto& p = *pl->p;
    const int np = p.getNumPrograms();
    const double sr = pl->sr, wait = waitSec (*pl);
    for (int k = 0; k < np; ++k)
    {
        if (quick && k > 3) break;
        p.setCurrentProgram (k);
        // Reposo del programa, luego un acorde de 1,5 s y la espera.
        auto rest = render (*pl, {}, S (1.0, sr), {});
        const double restDb = db (stats (rest, 0.5, 1.0, sr).rms);
        std::vector<Ev> ev { { S (0.1, sr), on (48) }, { S (0.1, sr), on (55) }, { S (0.1, sr), on (64) },
                             { S (1.6, sr), off (48) }, { S (1.6, sr), off (55) }, { S (1.6, sr), off (64) } };
        RenderOpts o; if (isEffect (*pl)) o.audioIn = testInput (sr, 1.6);
        const double total = 1.6 + wait;
        auto r = render (*pl, ev, S (total, sr), o);
        const auto all = stats (r, 0, total, sr), sus = stats (r, 0.4, 1.5, sr), end = stats (r, total - 0.5, total, sr);
        juce::String d = fmt ("pico %.2f, nota %.0f dB, final %.0f dB, reposo %.0f dB, DC %.3f", all.peak, db (sus.rms), db (end.rms), restDb, sus.mean);
        juce::String status = "PASS";
        if (all.nan + all.inf > 0) { status = "FAIL"; d << ", NaN/Inf"; }
        else if (all.peak > 4.0) { status = "FAIL"; d << ", pico excesivo"; }
        else if (all.peak > 1.6) { status = "WARN"; d << ", pico alto"; }
        if (db (sus.rms) < -60.0)
        {
            // Presets de secuencia: suenan con el transporte del host en marcha.
            RenderOpts ot; ot.transport = { { 0, 0, 0 }, { S (4.0, sr), 1, 0 } };
            if (isEffect (*pl)) ot.audioIn = testInput (sr, 4.0);
            pl->ph.ppq = 0;
            auto rt = render (*pl, ev, S (4.0, sr), ot);
            const auto ts = stats (rt, 0.5, 4.0, sr);
            if (db (ts.rms) > -60.0) d << fmt (", con el transporte %.0f dB", db (ts.rms));
            else { status = status == "FAIL" ? status : "WARN"; d << ", no suena con un acorde"; }
            render (*pl, { { 0, cc (123, 0) }, { 0, cc (120, 0) } }, S (0.5, sr), {});
        }
        if (db (end.rms) > std::max (restDb + 6.0, -70.0) && db (end.rms) > db (sus.rms) - 30.0)
        {
            // Un efecto con una cola declarada más larga que la espera (reverbs largas, congelados) puede seguir sonando.
            const double tail = p.getTailLengthSeconds();
            if (isEffect (*pl) && tail > wait - 4.0) d << (std::isinf (tail) ? juce::String (", cola infinita declarada") : fmt (", cola declarada de %.0f s", tail));
            else { status = "FAIL"; d << ", sigue sonando tras la espera"; }
        }
        if (std::abs (sus.mean) > 0.02) { status = status == "FAIL" ? status : "WARN"; d << ", DC"; }
        if (status != "PASS") writeWav (r, sr, "prog" + juce::String (k));
        report ("programs", juce::String (k) + " " + p.getProgramName (k), status, d);
        // Que el próximo programa arranque limpio.
        render (*pl, { { 0, cc (123, 0) }, { 0, cc (120, 0) } }, S (0.5, sr), {});
    }
}

void testMidi()
{
    auto probe = load (pluginPath);
    const double sr = probe->sr;
    const bool fx = isEffect (*probe);
    probe.reset();
    const double restDb = restLevelDb (pluginPath, 8.0);
    if (fx) { scenario ("efecto, entrada y silencio", {}, 2.0, restDb); return; }
    auto t = [sr] (double s) { return S (s, sr); };

    scenario ("note-on vel 0 como note-off", { { t (0.1), on (60) }, { t (0.8), on (60, 0) } }, 0.8, restDb);
    scenario ("note-off sin note-on", { { t (0.1), off (61) }, { t (0.2), off (62) }, { t (0.3), off (0) }, { t (0.4), off (127) } }, 0.4, restDb);
    scenario ("misma nota dos veces, un off", { { t (0.1), on (60) }, { t (0.3), on (60) }, { t (0.8), off (60) } }, 0.8, restDb);
    scenario ("misma nota dos veces, dos off", { { t (0.1), on (60) }, { t (0.3), on (60) }, { t (0.8), off (60) }, { t (0.9), off (60) } }, 0.9, restDb);
    {
        std::vector<Ev> ev;
        for (int n = 0; n < 128; ++n) ev.push_back ({ t (0.1), on (n, 1 + n % 127) });
        for (int n = 0; n < 128; ++n) ev.push_back ({ t (1.0) + n * 7, off (n) });
        scenario ("128 notas a la vez", ev, 1.1, restDb);
    }
    scenario ("all notes off (CC123)", { { t (0.1), on (48) }, { t (0.1), on (52) }, { t (0.1), on (55) }, { t (0.8), cc (123, 0) } }, 0.8, restDb);
    scenario ("all sound off (CC120)", { { t (0.1), on (48) }, { t (0.1), on (52) }, { t (0.1), on (55) }, { t (0.8), cc (120, 0) } }, 0.8, restDb);
    scenario ("pedal CC64", { { t (0.0), cc (64, 127) }, { t (0.1), on (60) }, { t (0.3), off (60) }, { t (0.4), on (64) }, { t (0.6), off (64) }, { t (1.5), cc (64, 0) } }, 1.5, restDb);
    scenario ("pedal, misma nota repetida", { { t (0.0), cc (64, 127) }, { t (0.1), on (60) }, { t (0.2), off (60) }, { t (0.3), on (60) }, { t (0.4), off (60) }, { t (1.0), cc (64, 0) } }, 1.0, restDb);
    {
        std::vector<Ev> ev { { t (0.1), on (60) } };
        for (int i = 0; i < 40; ++i) ev.push_back ({ t (0.15 + i * 0.05), juce::MidiMessage::pitchWheel (1, i % 3 == 0 ? 0 : (i % 3 == 1 ? 16383 : 8192)) });
        ev.push_back ({ t (2.2), off (60) }); ev.push_back ({ t (2.3), juce::MidiMessage::pitchWheel (1, 8192) });
        scenario ("pitch bend extremo", ev, 2.3, restDb);
    }
    {
        std::vector<Ev> ev { { t (0.1), on (60) }, { t (0.1), on (67) } };
        for (int i = 0; i <= 127; ++i) { ev.push_back ({ t (0.15) + i * 400, cc (1, i) }); ev.push_back ({ t (0.15) + i * 400 + 100, juce::MidiMessage::channelPressureChange (1, i) });
                                          ev.push_back ({ t (0.15) + i * 400 + 200, juce::MidiMessage::aftertouchChange (1, 60, i) }); ev.push_back ({ t (0.15) + i * 400 + 300, cc (2, i) }); }
        ev.push_back ({ t (1.4), off (60) }); ev.push_back ({ t (1.4), off (67) }); ev.push_back ({ t (1.5), cc (1, 0) });
        scenario ("mod wheel, aftertouch, breath", ev, 1.5, restDb);
    }
    {
        std::vector<Ev> ev;
        for (int ch = 1; ch <= 16; ++ch) { ev.push_back ({ t (0.05 * ch), on (40 + ch, 100, ch) }); ev.push_back ({ t (1.0 + 0.02 * ch), off (40 + ch, ch) }); }
        scenario ("notas en los 16 canales", ev, 1.4, restDb);
    }
    {
        // Los 128 controladores (incluidos los de modo de canal 120-127) con notas sonando; al final, reset y note-offs.
        std::vector<Ev> ev { { t (0.05), on (48) }, { t (0.05), on (60) }, { t (0.05), on (67) } };
        for (int c = 0; c < 128; ++c) { ev.push_back ({ t (0.1) + c * 300, cc (c, 127) }); ev.push_back ({ t (0.1) + c * 300 + 150, cc (c, c == 7 || c == 11 ? 100 : 0) }); }
        const double e = 0.1 + 128 * 300 / sr + 0.05;
        ev.push_back ({ t (e), cc (121, 0) }); ev.push_back ({ t (e), off (48) }); ev.push_back ({ t (e), off (60) }); ev.push_back ({ t (e), off (67) });
        scenario ("los 128 CC con notas sonando", ev, e, restDb);
    }
    scenario ("notas extremas 0 y 127", { { t (0.1), on (0, 1) }, { t (0.5), off (0) }, { t (0.6), on (127, 127) }, { t (1.0), off (127) },
                                           { t (1.1), on (0, 127) }, { t (1.5), off (0) }, { t (1.6), on (127, 1) }, { t (2.0), off (127) } }, 2.0, restDb);
    {
        const juce::int64 b = 512;
        scenario (U ("eventos en la última muestra del bloque"), { { b * 10 - 1, on (60) }, { b * 10 - 1, on (64) }, { b * 60 - 1, off (60) }, { b * 60 - 1, off (64) } }, (double) (b * 60) / sr, restDb);
    }
    {
        std::vector<Ev> ev;
        for (int i = 0; i < 400; ++i) { ev.push_back ({ t (0.1 + i * 0.005), on (60 + i % 5, 90) }); ev.push_back ({ t (0.1 + i * 0.005) + 60, off (60 + i % 5) }); }
        scenario (U ("reataque rápido (cada 5 ms)"), ev, 2.2, restDb);
    }
    scenario ("on y off en la misma muestra", { { t (0.1), on (60) }, { t (0.1), off (60) }, { t (0.3), on (62) }, { t (0.6), off (62) }, { t (0.6), on (62) }, { t (1.0), off (62) } }, 1.0, restDb);
    scenario ("note-off con velocidad 127", { { t (0.1), on (60) }, { t (0.6), off (60, 1, 127) } }, 0.6, restDb);
    {
        std::vector<Ev> ev { { t (0.05), on (48) }, { t (0.05), on (60) } };
        for (int k = 0; k < 40; ++k) ev.push_back ({ t (0.1 + 0.05 * k), juce::MidiMessage::programChange (1, k) });
        ev.push_back ({ t (2.3), off (48) }); ev.push_back ({ t (2.3), off (60) });
        scenario ("program change con notas", ev, 2.3, restDb);
    }
    {
        std::vector<Ev> ev { { t (0.05), on (55) } };
        juce::Random rnd (9);
        for (int k = 0; k < 60; ++k)
        {
            juce::MemoryBlock mbk; const int len = k == 0 ? 0 : rnd.nextInt (k % 7 == 0 ? 3000 : 64);
            for (int i = 0; i < len; ++i) { const juce::uint8 byte = (juce::uint8) rnd.nextInt (128); mbk.append (&byte, 1); }
            if (k % 3 == 0 && len > 4) { auto* d = (juce::uint8*) mbk.getData(); d[0] = 0x43; d[1] = (juce::uint8) (0x10 | rnd.nextInt (16)); d[2] = 0x5e; }   // con cabecera Yamaha
            ev.push_back ({ t (0.1 + 0.02 * k), juce::MidiMessage::createSysExMessage (mbk.getData(), (int) mbk.getSize()) });
        }
        ev.push_back ({ t (1.5), off (55) });
        scenario ("sysex basura", ev, 1.5, restDb);
    }
    {
        std::vector<Ev> ev;
        juce::Random rnd (5);
        for (int i = 0; i < 3000; ++i)
        {
            const auto s = t (0.05) + (juce::int64) rnd.nextInt ((int) (sr * 6));
            const int kind = rnd.nextInt (10), ch = 1 + rnd.nextInt (2), n = 24 + rnd.nextInt (80);
            if (kind < 4) ev.push_back ({ s, on (n, 1 + rnd.nextInt (127), ch) });
            else if (kind < 8) ev.push_back ({ s, off (n, ch) });
            else if (kind == 8) ev.push_back ({ s, juce::MidiMessage::pitchWheel (ch, rnd.nextInt (16384)) });
            else ev.push_back ({ s, cc (rnd.nextBool() ? 1 : 64, rnd.nextInt (128), ch) });
        }
        for (int ch = 1; ch <= 2; ++ch) { ev.push_back ({ t (6.2), cc (64, 0, ch) }); for (int n = 24; n < 104; ++n) ev.push_back ({ t (6.2), off (n, ch) }); ev.push_back ({ t (6.3), juce::MidiMessage::pitchWheel (ch, 8192) }); }
        scenario ("lluvia aleatoria de MIDI", ev, 6.3, restDb);
    }
    // Parámetros al azar con notas apretadas (voces, unísono, calidad, modo...), vuelta a los valores iniciales y
    // recién ahí se sueltan las teclas: tiene que quedar en silencio.
    const int onlySeed = juce::SystemStats::getEnvironmentVariable ("HT_SEED", "0").getIntValue();
    for (int seed : { 1, 2, 3, 4, 5, 6 })
    {
        if (onlySeed > 0 && seed != onlySeed) continue;
        std::vector<Ev> ev { { t (0.05), on (48) }, { t (0.05), on (55) }, { t (0.05), on (60) }, { t (0.05), on (64) } };
        for (int i = 0; i < 6; ++i) { ev.push_back ({ t (0.4 + 0.3 * i), on (67 + i) }); ev.push_back ({ t (0.55 + 0.3 * i), off (67 + i) }); }
        for (int n : { 48, 55, 60, 64 }) ev.push_back ({ t (2.8), off (n) });
        auto rnd = std::make_shared<juce::Random> (seed * 977);
        auto init = std::make_shared<std::vector<float>>();
        RenderOpts o;
        auto plRef = std::make_shared<Plug*> (nullptr);
        o.perBlock = [=] (juce::int64 pos, int) {
            auto& pl = **plRef; auto ps = realParams (pl);
            if (init->empty()) for (auto* p : ps) init->push_back (p->getValue());
            if (pos < t (0.2)) return;
            if (pos < t (2.4))
            {
                // HT_PARAMS=lo:hi limita el azar a esos índices (para aislar el parámetro culpable); HT_PARAMS=a,b,c o
                // a:b,c:d, a esos índices o tramos (para aislar una combinación).
                static const auto spec = juce::SystemStats::getEnvironmentVariable ("HT_PARAMS", "");
                auto allowed = [&] (int k) {
                    if (spec.isEmpty()) return true;
                    for (auto& part : juce::StringArray::fromTokens (spec, ",", ""))
                    {
                        const auto r = juce::StringArray::fromTokens (part, ":", "");
                        if (r.size() == 2 ? (k >= r[0].getIntValue() && k < r[1].getIntValue()) : k == part.getIntValue()) return true;
                    }
                    return false;
                };
                for (int i = 0; i < 4; ++i) { const int k = rnd->nextInt (ps.size()); const float v = rnd->nextFloat(); if (allowed (k)) ps[k]->setValue (v); }
            }
            else if (pos < t (2.5)) for (int i = 0; i < ps.size() && i < (int) init->size(); ++i) ps[i]->setValue ((*init)[(size_t) i]);
        };
        scenario ("parámetros al azar con teclas apretadas " + juce::String (seed), ev, 2.8, restDb, true, [plRef] (Plug& pl) { *plRef = &pl; }, o);
    }
    // Bypass con una nota sonando: el note-off llega durante el bypass.
    {
        auto plRef = std::make_shared<Plug*> (nullptr);
        RenderOpts o;
        o.perBlock = [=] (juce::int64 pos, int) {
            if (auto* bp = (*plRef)->p->getBypassParameter())
            {
                if (pos >= t (0.5) && pos < t (1.5)) bp->setValue (1.0f);
                else bp->setValue (0.0f);
            }
        };
        scenario ("bypass con nota, note-off durante el bypass", { { t (0.1), on (60) }, { t (0.1), on (64) }, { t (1.0), off (60) }, { t (1.0), off (64) } }, 1.6, restDb, true,
                  [plRef] (Plug& pl) { *plRef = &pl; }, o);
    }
}

void testRates (bool quick)
{
    struct Cfg { double sr; int block; bool rnd; };
    std::vector<Cfg> cfgs;
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int b : { 32, 512, 4096 }) cfgs.push_back ({ sr, b, false });
    cfgs.push_back ({ 48000.0, 1, false });
    cfgs.push_back ({ 48000.0, 1024, true });
    cfgs.push_back ({ 44100.0, 333, true });
    if (quick) cfgs = { { 44100.0, 512, false }, { 96000.0, 64, false }, { 48000.0, 1024, true } };
    double refRms = 0, refF0 = 0;
    auto run = [&] (const Cfg& c, double& rms, double& f0, Stats& all, Out& keep) {
        auto pl = load (pluginPath, c.sr, c.block);
        RenderOpts o; o.randomBlocks = c.rnd; if (isEffect (*pl)) o.audioIn = testInput (c.sr, 2.0);
        std::vector<Ev> ev { { S (0.1, c.sr), on (69, 100) }, { S (1.6, c.sr), off (69) } };
        auto r = render (*pl, ev, S (2.5, c.sr), o);
        all = stats (r, 0, 2.5, c.sr); rms = stats (r, 0.4, 1.5, c.sr).rms; f0 = pitchOf (r, 0.8, c.sr);
        keep = std::move (r);
    };
    const bool fxRates = [&] { auto probe = load (pluginPath); return isEffect (*probe); }();
    { Stats a; Out k; run ({ 48000.0, 512, false }, refRms, refF0, a, k);
      report ("rates", "48 kHz / 512 (referencia)", a.nan + a.inf > 0 ? "FAIL" : "PASS", fmt ("nivel %.1f dB, f0 %.2f Hz (nota 69)", db (refRms), refF0)); }
    for (auto& c : cfgs)
    {
        double rms, f0; Stats a; Out k;
        const auto t0 = Clock::now();
        run (c, rms, f0, a, k);
        const double secs = std::chrono::duration<double> (Clock::now() - t0).count();
        juce::String name = fmt ("%.1f kHz / %s%d", c.sr / 1000.0, c.rnd ? "al azar hasta " : "", c.block);
        juce::String d = fmt ("nivel %+.1f dB, f0 %.2f Hz, pico %.2f, %.1f s", db (rms) - db (refRms), f0, a.peak, secs);
        juce::String st = "PASS";
        if (a.nan + a.inf > 0) { st = "FAIL"; d << ", NaN/Inf"; }
        else if (a.peak > 4.0) { st = "FAIL"; d << ", pico excesivo"; }
        if (refRms > 1e-4 && std::abs (db (rms) - db (refRms)) > 6.0) { st = st == "FAIL" ? st : "WARN"; d << ", nivel distinto"; }
        // La afinación sólo se compara en instrumentos: un efecto puede transponer al azar (los granos de Murmur).
        if (! fxRates && refF0 > 20 && f0 > 20 && std::abs (12.0 * std::log2 (f0 / refF0)) > 0.25) { st = "FAIL"; d << ", desafinado " << juce::String (1200.0 * std::log2 (f0 / refF0), 0) << " cents"; }
        if (st != "PASS") writeWav (k, c.sr, "rate_" + name);
        report ("rates", name, st, d);
    }
    // (Bloques más grandes que el máximo preparado: VST3 lo prohíbe y el envoltorio de JUCE se cae; ningún DAW lo hace.)
}

std::vector<Ev> phrase (double sr)
{
    std::vector<Ev> ev;
    const int notes[] = { 48, 55, 60, 64, 67, 72, 62, 57 };
    for (int i = 0; i < 8; ++i) { ev.push_back ({ S (0.1 + 0.25 * i, sr), on (notes[i], 70 + 6 * i) }); ev.push_back ({ S (0.3 + 0.25 * i, sr), off (notes[i]) }); }
    ev.push_back ({ S (2.4, sr), on (48) }); ev.push_back ({ S (2.4, sr), on (60) }); ev.push_back ({ S (3.4, sr), off (48) }); ev.push_back ({ S (3.4, sr), off (60) });
    return ev;
}

void testState()
{
    const int np = load (pluginPath)->p->getNumPrograms();
    for (int k : { 0, std::max (0, np / 2), std::max (0, np - 1) })
    {
        auto a = load (pluginPath);
        a->p->setCurrentProgram (k);
        // Mover unos parámetros para que el estado no sea el de fábrica.
        auto params = realParams (*a);
        juce::Random rnd (k + 3);
        for (int i = 0; i < std::min (6, params.size()); ++i) { auto* pr = params[rnd.nextInt (params.size())]; pr->setValue (juce::jlimit (0.0f, 1.0f, pr->getValue() + 0.1f * (rnd.nextFloat() - 0.5f))); }
        render (*a, {}, 2048, {});      // que los cambios lleguen al procesador
        juce::MemoryBlock st; a->p->getStateInformation (st);
        a->p->setStateInformation (st.getData(), (int) st.getSize());      // las dos instancias cargan el mismo estado
        render (*a, {}, 2048, {});
        // a se cierra antes de abrir b, como un proyecto que se guarda y se vuelve a abrir (Orrery trata un estado que
        // se carga mientras su original sigue abierto como una copia: Duplicate Track, y le resuelve el rol de nuevo).
        std::vector<float> aVals; for (auto* pr : params) aVals.push_back (pr->getValue());
        RenderOpts o; if (isEffect (*a)) o.audioIn = testInput (a->sr, 3.4);
        auto ra = render (*a, phrase (a->sr), S (5.0, a->sr), o);
        const double aSr = a->sr; const bool aEffect = isEffect (*a);
        a.reset();
        auto b = load (pluginPath);
        b->p->setStateInformation (st.getData(), (int) st.getSize());
        render (*b, {}, 2048, {});
        int mism = 0; juce::String which;
        auto bParams = realParams (*b);
        for (int i = 0; i < bParams.size() && i < (int) aVals.size(); ++i)
        {
            float va = aVals[(size_t) i], vb = bParams[i]->getValue();
            const int steps = bParams[i]->getNumSteps();      // los discretos se comparan por paso
            if (steps > 1 && steps < 100000) { va = std::round (va * (float) (steps - 1)); vb = std::round (vb * (float) (steps - 1)); }
            if (std::abs (va - vb) > 1e-3f) { ++mism; if (mism <= 3) which << bParams[i]->getName (24) << " " << juce::String (va, 3) << U ("≠") << juce::String (vb, 3) << "; "; }
        }
        juce::MemoryBlock st2; b->p->getStateInformation (st2);
        RenderOpts o2; if (aEffect) o2.audioIn = testInput (b->sr, 3.4);
        auto rb = render (*b, phrase (b->sr), S (5.0, b->sr), o2);
        const double la = db (stats (ra, 0, 5, aSr).rms), lb = db (stats (rb, 0, 5, b->sr).rms);
        double num = 0, da = 0, dbb = 0;
        for (size_t i = 0; i < std::min (ra.L.size(), rb.L.size()); ++i) { num += (double) ra.L[i] * rb.L[i]; da += (double) ra.L[i] * ra.L[i]; dbb += (double) rb.L[i] * rb.L[i]; }
        const double corr = da > 0 && dbb > 0 ? num / std::sqrt (da * dbb) : 1.0;
        juce::String d = fmt ("%d bytes, %d parámetros distintos, nivel %.1f vs %.1f dB, correlación %.3f%s", (int) st.getSize(), mism, la, lb, corr,
                                                  st2.getSize() == st.getSize() ? "" : ", el estado re-guardado cambia de tamaño");
        if (mism > 0) d << " (" << which << ")";
        juce::String status = mism > 0 || std::abs (la - lb) > 3.0 ? "FAIL" : (corr < 0.9 ? "WARN" : "PASS");
        report ("state", "programa " + juce::String (k) + " guardado y restaurado", status, d);
    }
    // Estados rotos: no tienen que colgar ni romper el sonido.
    auto good = load (pluginPath);
    juce::MemoryBlock st; good->p->getStateInformation (st);
    juce::Random rnd (42);
    struct Bad { juce::String name; juce::MemoryBlock data; };
    std::vector<Bad> bad;
    bad.push_back ({ U ("vacío"), {} });
    { juce::MemoryBlock m; m.append ("x", 1); bad.push_back ({ "1 byte", m }); }
    { juce::MemoryBlock m; for (int i = 0; i < 20000; ++i) { auto c = (char) rnd.nextInt (256); m.append (&c, 1); } bad.push_back ({ "20 KB al azar", m }); }
    { juce::MemoryBlock m (st.getData(), st.getSize() / 2); bad.push_back ({ "cortado a la mitad", m }); }
    { juce::MemoryBlock m (st); for (size_t i = 8; i < m.getSize(); i += 37) ((char*) m.getData())[i] ^= 0x5a; bad.push_back ({ "bytes alterados", m }); }
    { juce::String x = "<?xml version=\"1.0\"?><PARAMS><PARAM id=\"zzz\" value=\"nan\"/></PARAMS>"; juce::MemoryBlock m; juce::AudioProcessor::copyXmlToBinary (*juce::parseXML (x), m); bad.push_back ({ "XML ajeno con NaN", m }); }
    { juce::String x = "{\"params\":{\"cutoff\":1e30,\"x\":null}}"; juce::MemoryBlock m (x.toRawUTF8(), x.getNumBytesAsUTF8()); bad.push_back ({ "JSON ajeno", m }); }
    double goodLvl;
    {
        RenderOpts o; if (isEffect (*good)) o.audioIn = testInput (good->sr, 1.6);
        auto r = render (*good, { { S (0.1, good->sr), on (60) }, { S (1.6, good->sr), off (60) } }, S (2.5, good->sr), o);
        goodLvl = db (stats (r, 0.4, 1.5, good->sr).rms);
    }
    for (auto& b : bad)
    {
        auto pl = load (pluginPath);
        pl->p->setStateInformation (b.data.getData(), (int) b.data.getSize());
        RenderOpts o; if (isEffect (*pl)) o.audioIn = testInput (pl->sr, 1.6);
        auto r = render (*pl, { { S (0.1, pl->sr), on (60) }, { S (1.6, pl->sr), off (60) } }, S (2.5, pl->sr), o);
        auto a = stats (r, 0, 2.5, pl->sr);
        const double lvl = db (stats (r, 0.4, 1.5, pl->sr).rms);
        juce::String status = a.nan + a.inf > 0 || a.peak > 4.0 ? "FAIL" : (lvl < std::min (-60.0, goodLvl - 20.0) && goodLvl > -60.0 ? "WARN" : "PASS");
        report ("state", "estado roto: " + b.name, status, fmt ("pico %.2f, nivel %.0f dB%s", a.peak, lvl, a.nan + a.inf > 0 ? ", NaN/Inf" : ""));
    }
}

void testFuzz (bool quick)
{
    const int only = juce::SystemStats::getEnvironmentVariable ("HT_SEED", "0").getIntValue();
    for (int seed : { 1, 2, 3 })
    {
        if (quick && seed > 1) break;
        if (only > 0 && seed != only) continue;
        auto pl = load (pluginPath);
        auto params = realParams (*pl);
        juce::Random rnd (seed * 101);
        const double sr = pl->sr, secs = quick ? 10.0 : 30.0;
        std::vector<Ev> ev;
        for (double s = 0.05; s < secs - 1.0; s += 0.08 + rnd.nextDouble() * 0.3)
        {
            const int n = 30 + rnd.nextInt (60);
            ev.push_back ({ S (s, sr), on (n, 1 + rnd.nextInt (127)) });
            ev.push_back ({ S (s + 0.05 + rnd.nextDouble() * 0.8, sr), off (n) });
        }
        ev.push_back ({ S (secs - 1.0, sr), cc (123, 0) });
        RenderOpts o;
        if (isEffect (*pl)) o.audioIn = testInput (sr, secs);
        juce::int64 lastAll = 0;
        o.perBlock = [&] (juce::int64 pos, int) {
            if (pos >= S (secs - 1.0, sr)) return;
            for (int i = 0; i < 3; ++i) params[rnd.nextInt (params.size())]->setValue (rnd.nextFloat());
            if (pos - lastAll > S (2.0, sr)) { lastAll = pos; for (auto* p : params) p->setValue (rnd.nextFloat()); }
        };
        auto r = render (*pl, ev, S (secs, sr), o);
        auto a = stats (r, 0, secs, sr);
        juce::String status = a.nan + a.inf > 0 ? "FAIL" : (a.peak > 8.0 ? "WARN" : "PASS");
        juce::String d = fmt ("pico %.2f, nivel %.0f dB, bloque más lento %.2f ms (de %.2f ms reales)", a.peak, db (a.rms), r.maxBlockMs, 1000.0 * pl->block / sr);
        if (a.nan + a.inf > 0) d << ", " << a.nan << " NaN " << a.inf << " Inf";
        if (status != "PASS") writeWav (r, sr, "fuzz" + juce::String (seed));
        report ("fuzz", U ("parámetros al azar, semilla ") + juce::String (seed), status, d);
    }
}

void testParams()
{
    auto pl = load (pluginPath);
    auto params = realParams (*pl);
    juce::MemoryBlock init; pl->p->getStateInformation (init);
    const double sr = pl->sr;
    int bad = 0, loud = 0;
    juce::String list;
    for (int i = 0; i < params.size(); ++i)
    {
        for (float v : { 0.0f, 1.0f })
        {
            pl->p->setStateInformation (init.getData(), (int) init.getSize());
            render (*pl, { { 0, cc (123, 0) } }, 1024, {});
            params[i]->setValue (v);
            RenderOpts o; if (isEffect (*pl)) o.audioIn = testInput (sr, 1.0);
            auto r = render (*pl, { { S (0.02, sr), on (48) }, { S (0.02, sr), on (60) }, { S (0.02, sr), on (67) }, { S (0.6, sr), off (48) }, { S (0.6, sr), off (60) }, { S (0.6, sr), off (67) } }, S (1.0, sr), o);
            auto a = stats (r, 0, 1.0, sr);
            if (a.nan + a.inf > 0 || a.peak > 4.0)
            {
                const bool nan = a.nan + a.inf > 0;
                (nan ? bad : loud)++;
                list << params[i]->getName (32) << "=" << (int) v << (nan ? " NaN" : fmt (" pico %.1f", a.peak)) << "; ";
                if (bad + loud <= 6) writeWav (r, sr, "param_" + params[i]->getName (24) + "_" + juce::String ((int) v));
            }
        }
    }
    report ("params", juce::String (params.size()) + U (" parámetros en mínimo y máximo"), bad > 0 ? "FAIL" : (loud > 0 ? "WARN" : "PASS"),
            fmt ("%d con NaN/Inf, %d con picos > 4", bad, loud) + (list.isEmpty() ? juce::String() : ": " + list));
}

void testCpu()
{
    auto pl = load (pluginPath);
    const double sr = pl->sr;
    RenderOpts o; o.keepBlockTimes = true; if (isEffect (*pl)) o.audioIn = testInput (sr, 1.0);
    std::vector<Ev> ev { { S (0.05, sr), on (48) }, { S (0.05, sr), on (55) }, { S (0.05, sr), on (64) }, { S (1.0, sr), off (48) }, { S (1.0, sr), off (55) }, { S (1.0, sr), off (64) } };
    auto r = render (*pl, ev, S (40.0, sr), o);
    const int bps = (int) (sr / pl->block);
    auto avg = [&] (int fromSec, int toSec) { double s = 0; int n = 0; for (int b = fromSec * bps; b < toSec * bps && b < (int) r.blockMs.size(); ++b) { s += r.blockMs[(size_t) b]; ++n; } return n ? s / n : 0.0; };
    const double note = avg (0, 1), after = avg (5, 10), late = avg (35, 40), real = 1000.0 * pl->block / sr;
    juce::String d = fmt ("con notas %.1f %%, en silencio a los 5 s %.1f %%, a los 35 s %.1f %% de tiempo real", 100 * note / real, 100 * after / real, 100 * late / real);
    report ("cpu", "silencio largo (denormales)", late > std::max (after * 3.0, 0.02 * real) ? "WARN" : "PASS", d);
}

void testReprepare()
{
    auto probe = load (pluginPath); if (isEffect (*probe)) return;
    const double restDb = restLevelDb (pluginPath, 6.0);
    probe.reset();
    // El usuario cambia la frecuencia de muestreo o el buffer con notas sonando; el host después manda los note-off.
    auto pl = load (pluginPath);
    auto r1 = render (*pl, { { 100, on (60) }, { 100, on (64) } }, S (0.5, pl->sr), {});
    pl->p->releaseResources();
    pl->sr = 96000.0; pl->ph.sr = 96000.0; pl->block = 128;
    pl->p->setRateAndBufferSizeDetails (96000.0, 128);
    pl->p->prepareToPlay (96000.0, 128);
    auto r2 = render (*pl, { { 1000, off (60) }, { 1000, off (64) } }, S (6.0, 96000.0), {});
    auto a = stats (r2, 0, 6.0, 96000.0); const double endDb = db (stats (r2, 5.5, 6.0, 96000.0).rms);
    juce::String st = a.nan + a.inf > 0 ? "FAIL" : (endDb > std::max (restDb + 6.0, -70.0) ? "FAIL" : "PASS");
    report ("reprepare", U ("48→96 kHz con notas, luego note-off"), st, fmt ("pico %.2f, final %.0f dB (reposo %.0f)", a.peak, endDb, restDb));
    // reset() con notas: no tiene que dejar ruido ni NaN.
    auto p2 = load (pluginPath);
    render (*p2, { { 100, on (60) } }, S (0.3, p2->sr), {});
    p2->p->reset();
    auto r3 = render (*p2, { { 100, off (60) } }, S (6.0, p2->sr), {});
    auto b = stats (r3, 0, 6.0, p2->sr); const double end3 = db (stats (r3, 5.5, 6.0, p2->sr).rms);
    report ("reprepare", "reset() con una nota, luego note-off", b.nan + b.inf > 0 || end3 > std::max (restDb + 6.0, -70.0) ? "FAIL" : "PASS",
            fmt ("pico %.2f, final %.0f dB", b.peak, end3));
}

// Host parado que sigue procesando (Ableton, Bitwig): la posición no cambia entre bloques. Lo aleatorio que sale de la
// posición no puede repetir el mismo bloque (se oye como un tono a fs / tamaño de bloque). Entrada en silencio.
void testStopped()
{
    auto pl = load (pluginPath);
    auto& p = *pl->p;
    const int np = std::max (1, p.getNumPrograms());
    int worst = 0;
    for (int k = 0; k < np; ++k)
    {
        if (np > 1) p.setCurrentProgram (k);
        RenderOpts o; o.frozenPosition = true;
        if (isEffect (*pl)) o.audioIn = [] (juce::int64, int) { return 0.0f; };
        pl->ph.playing = false; pl->ph.ppq = 8.0;
        auto r = render (*pl, {}, S (1.5, pl->sr), o);
        const int B = pl->block; int repeats = 0, loud = 0;
        for (size_t b = (size_t) S (0.5, pl->sr) / (size_t) B; (b + 2) * (size_t) B <= r.L.size(); ++b)
        {
            double e = 0; bool same = true;
            for (int i = 0; i < B; ++i)
            {
                const float a = r.L[b * (size_t) B + (size_t) i], c = r.L[(b + 1) * (size_t) B + (size_t) i];
                e += (double) a * a;
                if (a != c) same = false;
            }
            if (e / B > 1e-12) { ++loud; if (same) ++repeats; }
        }
        if (repeats > 0 && repeats * 2 >= loud)
        {
            ++worst;
            report ("stopped", juce::String (k) + " " + p.getProgramName (k), "FAIL", fmt ("%d de %d bloques con sonido repiten el anterior con el host parado", repeats, loud));
        }
    }
    if (worst == 0) report ("stopped", U ("todos los programas, host parado"), "PASS", fmt ("%d programas sin bloques repetidos", np));
}

// Freeze y sesión: lo congelado tiene que seguir sonando después de guardar y reabrir el proyecto (y después de un
// prepareToPlay del host, que muchos mandan al cambiar el tamaño de bloque o al exportar).
void testFreezeState()
{
    auto a = load (pluginPath);
    juce::AudioProcessorParameter* fr = nullptr;
    for (auto* prm : realParams (*a)) if (prm->getName (32).equalsIgnoreCase ("Freeze")) fr = prm;
    if (fr == nullptr || a->ins == 0) return;
    const double sr = a->sr;
    RenderOpts feed; feed.audioIn = testInput (sr, 4.0);
    render (*a, phrase (sr), S (4.0, sr), feed);
    fr->setValue (1.0f);
    RenderOpts quiet; quiet.audioIn = [] (juce::int64, int) { return 0.0f; };
    const double la = db (stats (render (*a, {}, S (2.0, sr), quiet), 1.0, 2.0, sr).rms);
    a->p->prepareToPlay (sr, a->block * 2);                          // el host re-prepara
    const double lp = db (stats (render (*a, {}, S (2.0, sr), quiet), 1.0, 2.0, sr).rms);
    juce::MemoryBlock st; a->p->getStateInformation (st);
    auto b = load (pluginPath);
    b->p->setStateInformation (st.getData(), (int) st.getSize());
    const double lb = db (stats (render (*b, {}, S (2.0, sr), quiet), 1.0, 2.0, sr).rms);
    const bool ok = la > -60.0 && lp > la - 6.0 && lb > la - 6.0;
    report ("freeze", U ("congelado: re-preparar y reabrir la sesión"), ok ? "PASS" : "FAIL",
            fmt ("congelado %.0f dB, tras prepareToPlay %.0f dB, sesión reabierta %.0f dB (%d bytes)", la, lp, lb, (int) st.getSize()));
}

// Bypass: el host sigue compensando la latencia informada, así que el seco tiene que salir retrasado lo mismo.
void testBypass()
{
    auto pl = load (pluginPath);
    auto& p = *pl->p;
    const int L = p.getLatencySamples();
    if (pl->ins == 0 || L <= 0) { report ("bypass", U ("sin latencia o sin entrada"), "PASS", fmt ("latencia %d", L)); return; }
    if (p.getPluginDescription().isInstrument) { report ("bypass", U ("instrumento: en bypass no suena"), "PASS", fmt ("latencia %d", L)); return; }
    const int n = pl->block, total = 4 * n + L + 64, at = 1000;
    juce::AudioBuffer<float> buf (std::max ({ 2, pl->ins, pl->outs }), n);
    juce::MidiBuffer mb;
    std::vector<float> out;
    for (int pos = 0; pos < total; pos += n)
    {
        buf.clear();
        if (at >= pos && at < pos + n) for (int c = 0; c < pl->ins; ++c) buf.setSample (c, at - pos, 1.0f);
        p.processBlockBypassed (buf, mb);
        for (int i = 0; i < n; ++i) out.push_back (buf.getSample (0, i));
    }
    int peakAt = -1; float pk = 0;
    for (int i = 0; i < (int) out.size(); ++i) if (std::abs (out[(size_t) i]) > pk) { pk = std::abs (out[(size_t) i]); peakAt = i; }
    const int got = peakAt - at;
    report ("bypass", U ("impulso por processBlockBypassed"), got == L ? "PASS" : "FAIL", fmt ("sale a %d muestras; latencia informada %d", got, L));
}

// Hueco del host (QA en DAWs 2026-10-22, X1): REAPER no llama a un plugin en bypass (ni processBlock ni
// processBlockBypassed), así que un note-off que llega en ese rato se pierde. Al volver, el plugin tiene que soltar
// las notas (dh::HostGap). Dos casos: (a) transporte andando, se saltan los bloques de 1,0 a 2,0 s mientras la
// posición avanza y el note-off de 1,5 s no llega; (b) transporte parado, 600 ms de reloj sin llamadas.
void testHostGap()
{
    {
        auto probe = load (pluginPath);
        if (isEffect (*probe) || ! probe->p->acceptsMidi() || isGenerator (*probe)) return;
    }
    struct Lv { double note, end; };
    auto run = [] (bool gap, bool playing) -> Lv {
        auto pl = load (pluginPath);
        const double sr = pl->sr; const int n = pl->block;
        const double total = 2.0 + waitSec (*pl);
        juce::AudioBuffer<float> buf (std::max ({ 2, pl->ins, pl->outs }), n);
        juce::MidiBuffer mb;
        Out o;
        const juce::int64 tOn = S (0.5, sr), tOff = S (1.5, sr), g0 = S (1.0, sr), g1 = S (2.0, sr), end = S (total, sr);
        pl->ph.playing = playing; pl->ph.ppq = 0;
        bool slept = false;
        for (juce::int64 pos = 0; pos < end; pos += n)
        {
            const bool skip = gap && pos >= g0 && pos < g1;
            if (! skip)
            {
                if (gap && ! playing && pos >= g1 && ! slept) { juce::Thread::sleep (600); slept = true; }
                mb.clear();
                if (tOn >= pos && tOn < pos + n) mb.addEvent (on (60, 110), (int) (tOn - pos));
                if (tOff >= pos && tOff < pos + n) mb.addEvent (off (60), (int) (tOff - pos));
                buf.clear();
                pl->ph.pos = playing ? pos : 0;
                pl->p->processBlock (buf, mb);
            }
            else buf.clear();
            const float* l = buf.getReadPointer (0); const float* r = buf.getReadPointer (pl->outs > 1 ? 1 : 0);
            o.L.insert (o.L.end(), l, l + n); o.R.insert (o.R.end(), r, r + n);
            if (playing) pl->ph.ppq += n * pl->ph.bpm / 60.0 / sr;
        }
        return { db (stats (o, 0.6, 0.95, sr).rms), db (stats (o, total - 0.5, total, sr).rms) };
    };
    for (bool playing : { true, false })
    {
        const auto ctl = run (false, playing), gap = run (true, playing);
        const juce::String name = playing ? U ("hueco con el transporte andando") : U ("hueco de reloj, transporte parado");
        if (ctl.note < -60.0) { report ("hostgap", name, "PASS", fmt ("la nota no suena con el programa inicial (%.0f dB)", ctl.note)); continue; }
        const bool ok = gap.end <= std::max (ctl.end + 6.0, -70.0);
        report ("hostgap", name, ok ? "PASS" : "FAIL",
                fmt ("nota %.0f dB; al final %.0f dB con el note-off perdido, %.0f dB con el note-off%s", ctl.note, gap.end, ctl.end, ok ? "" : U (" (nota colgada)").toRawUTF8()));
    }
}

// Estado byte a byte (QA en Live 2026-10-22, H4: Live reescribía el set en cada guardado porque el estado cambiaba sin
// que el usuario tocara nada) y una instancia nueva igual al programa 0 (QA en REAPER, X2).
bool editPluginState (Plug& pl, const std::function<void (juce::XmlElement&)>& edit, juce::MemoryBlock* result);
void testStateBytes()
{
    // Orrery guarda un id propio de cada instancia (para reconocer copias): no cuenta como diferencia.
    auto save = [] (Plug& pl) {
        juce::MemoryBlock m;
        bool has = false;
        if (editPluginState (pl, [&] (juce::XmlElement& x) { if (x.hasAttribute ("instanceId")) { has = true; x.setAttribute ("instanceId", "0"); } }, &m) && has) return m;
        m.reset(); pl.p->getStateInformation (m);
        auto* d = static_cast<char*> (m.getData());
        const std::string key = "instanceId=\"";
        for (size_t i = 0; i + key.size() < m.getSize(); ++i)
            if (std::memcmp (d + i, key.data(), key.size()) == 0)
                for (size_t j = i + key.size(); j < m.getSize() && d[j] != '"'; ++j) d[j] = '0';
        return m;
    };
    auto diff = [] (const juce::MemoryBlock& a, const juce::MemoryBlock& b) -> juce::String {
        if (a == b) return fmt ("iguales, %d bytes", (int) a.getSize());
        size_t i = 0; const auto* x = static_cast<const char*> (a.getData()); const auto* y = static_cast<const char*> (b.getData());
        while (i < a.getSize() && i < b.getSize() && x[i] == y[i]) ++i;
        return fmt ("%d vs %d bytes, distintos desde el byte %d", (int) a.getSize(), (int) b.getSize(), (int) i);
    };
    auto a = load (pluginPath);
    const auto s1 = save (*a), s2 = save (*a);
    report ("statebytes", U ("instancia nueva guardada dos veces"), s1 == s2 ? "PASS" : "FAIL", diff (s1, s2));
    a->p->setStateInformation (s1.getData(), (int) s1.getSize());
    const auto s3 = save (*a);
    report ("statebytes", U ("guardar → cargar → guardar"), s1 == s3 ? "PASS" : "FAIL", diff (s1, s3));
    // Cada instancia se cierra antes de cargar su estado en otra (como reabrir el proyecto): Orrery trata un estado que
    // se carga con su original todavía abierto como una copia (Duplicate Track) y le resuelve el rol de nuevo.
    const int aProgram = a->p->getCurrentProgram(), aPrograms = a->p->getNumPrograms();
    const juce::String aName0 = a->p->getProgramName (0);
    a.reset();
    auto b = load (pluginPath);
    b->p->setStateInformation (s1.getData(), (int) s1.getSize());
    const auto s4 = save (*b);
    report ("statebytes", U ("cargado en otra instancia y guardado"), s1 == s4 ? "PASS" : "FAIL", diff (s1, s4));
    b.reset();
    if (aPrograms > 1)
    {
        auto c = load (pluginPath);
        c->p->setCurrentProgram (std::min (3, c->p->getNumPrograms() - 1)); c->p->setCurrentProgram (0);
        const auto s5 = save (*c);
        report ("statebytes", U ("instancia nueva == programa 0"), s1 == s5 && aProgram == 0 ? "PASS" : "FAIL",
                aName0 + ", " + diff (s1, s5));
        c.reset();
        auto d = load (pluginPath);
        d->p->setCurrentProgram (std::min (5, d->p->getNumPrograms() - 1));
        render (*d, { { 0, on (60) }, { S (0.3, d->sr), off (60) } }, S (0.5, d->sr), {});
        const auto t1 = save (*d);
        d.reset();
        auto e = load (pluginPath);
        e->p->setStateInformation (t1.getData(), (int) t1.getSize());
        render (*e, {}, S (0.2, e->sr), {});
        const auto t2 = save (*e);
        report ("statebytes", U ("otro programa tocado: guardar → cargar → guardar"), t1 == t2 ? "PASS" : "FAIL", diff (t1, t2));
    }
    // Lo que el host escribe vuelve igual al reabrir la sesión (QA en REAPER, D2: Cutoff 0.6739 volvía 0.6772).
    {
        auto f = load (pluginPath);
        auto params = realParams (*f);
        juce::Random rnd (4242);
        std::vector<float> want;
        for (auto* prm : params) { float v = rnd.nextFloat(); if (prm->isDiscrete() || prm->isBoolean()) v = prm->getValue(); prm->setValueNotifyingHost (v); want.push_back (prm->getValue()); }
        const auto st = save (*f);
        const int fParams = params.size();
        f.reset();
        auto g = load (pluginPath);
        g->p->setStateInformation (st.getData(), (int) st.getSize());
        auto back = realParams (*g);
        int bad = 0; juce::String first;
        for (int i = 0; i < std::min (fParams, back.size()); ++i)
            if (std::abs (back[i]->getValue() - want[(size_t) i]) > 1.0e-5f)
            {
                if (bad++ == 0) first = back[i]->getName (32) + fmt (" %.4f -> %.4f", want[(size_t) i], back[i]->getValue());
            }
        // WARN y no FAIL: un AudioParameterInt redondea al guardar (el host lo ve continuo) y vuelve en el entero; es
        // cosmético, y hacerlo discreto cambiaría la escala de la automatización de los AU ya guardados.
        report ("statebytes", U ("valores continuos al azar: guardar → reabrir"), bad == 0 ? "PASS" : "WARN",
                bad == 0 ? fmt ("%d parámetros iguales", fParams) : fmt ("%d distintos, el primero ", bad) + first);
    }
}

// Notas a un efecto (QA en REAPER 2026-10-22, X4): los efectos que sólo usan notas compilan sin los ~2.080
// parámetros "MIDI CC n|m" del VST3; las notas tienen que seguir llegando. Entrada: un seno de 220 Hz; se compara la
// salida con una nota (y el pedal CC64) entre 1,0 y 2,0 s contra la misma sin MIDI. --fx-program n elige el programa
// (por ejemplo uno con Follow MIDI). PASS si las notas cambian la salida; WARN si no (el programa no usa MIDI).
int fxProgram = -1;
void testMidiFx()
{
    auto probe = load (pluginPath);
    const bool fx = isEffect (*probe);
    const double sr = probe->sr;
    probe.reset();
    if (! fx) return;
    auto one = [&] (bool withNotes) {
        auto pl = load (pluginPath);
        if (fxProgram >= 0 && fxProgram < pl->p->getNumPrograms()) pl->p->setCurrentProgram (fxProgram);
        RenderOpts o; o.audioIn = [sr] (juce::int64 i, int) { return 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr); };
        std::vector<Ev> ev;
        if (withNotes) ev = { { S (1.0, sr), juce::MidiMessage::controllerEvent (1, 64, 127) }, { S (1.0, sr), on (69) }, { S (1.0, sr), on (76) },
                              { S (2.0, sr), off (69) }, { S (2.0, sr), off (76) }, { S (2.0, sr), juce::MidiMessage::controllerEvent (1, 64, 0) } };
        return render (*pl, ev, S (3.0, sr), o);
    };
    const auto a = one (false), b = one (true);
    double d = 0, ref = 0;
    for (size_t i = (size_t) (1.1 * sr); i < (size_t) (1.95 * sr) && i < a.L.size(); ++i) { const double x = a.L[i] - b.L[i]; d += x * x; ref += (double) a.L[i] * a.L[i]; }
    const double rel = 10.0 * std::log10 ((d + 1e-30) / (ref + 1e-30));
    report ("midifx", fmt ("notas y pedal a un efecto%s", fxProgram >= 0 ? fmt (" (programa %d)", fxProgram).toRawUTF8() : ""), rel > -60.0 ? "PASS" : "WARN",
            fmt ("diferencia con y sin MIDI %.1f dB respecto de la salida", rel));
}

// Un programa suena igual venga del programa que venga (QA en REAPER 2026-10-22, P1: en Plumb el modo de latencia,
// la voz y el modo MIDI de un preset quedaban en los siguientes). Para cada par j → k: los parámetros después de
// elegir j y después k son los mismos que eligiendo k en una instancia nueva.
void testProgramOrder()
{
    auto ref = load (pluginPath);
    const int N = ref->p->getNumPrograms();
    if (N < 2) return;
    std::vector<std::vector<float>> want ((size_t) N);
    for (int k = 0; k < N; ++k)
    {
        auto pl = load (pluginPath); pl->p->setCurrentProgram (k);
        for (auto* prm : realParams (*pl)) want[(size_t) k].push_back (prm->getValue());
    }
    int bad = 0; juce::String first;
    for (int j = 0; j < N; ++j)
    {
        auto pl = load (pluginPath);
        const auto ps = realParams (*pl);
        for (int k = 0; k < N; ++k)
        {
            pl->p->setCurrentProgram (j); pl->p->setCurrentProgram (k);
            for (int i = 0; i < ps.size() && i < (int) want[(size_t) k].size(); ++i)
                if (std::abs (ps[i]->getValue() - want[(size_t) k][(size_t) i]) > 1.0e-4f)
                {
                    if (bad++ == 0) first = fmt ("%s después de %s: %s", pl->p->getProgramName (k).toRawUTF8(), pl->p->getProgramName (j).toRawUTF8(), ps[i]->getName (40).toRawUTF8());
                    break;
                }
        }
    }
    report ("progorder", fmt ("%d programas, %d pares", N, N * N), bad == 0 ? "PASS" : "FAIL",
            bad == 0 ? U ("cada programa deja los mismos parámetros venga de donde venga") : fmt ("%d pares distintos; el primero: %s", bad, first.toRawUTF8()));
}

// Reproducible (QA en REAPER 2026-10-22: ATFS1 y el AU del AT101V no anulaban entre dos instancias nuevas): dos
// instancias nuevas con el mismo MIDI, y una que ya tocó y el host volvió a preparar (lo que hace un render), tienen
// que dar las mismas muestras. WARN si no (un plugin puede tener deriva al azar a propósito).
void testRepro()
{
    auto nullDb = [] (const Out& a, const Out& b) {
        double d = 0, e = 0; const size_t n = std::min (a.L.size(), b.L.size());
        for (size_t i = 0; i < n; ++i) { const double x = (double) a.L[i] - b.L[i], y = (double) a.R[i] - b.R[i]; d += x * x + y * y; e += (double) a.L[i] * a.L[i] + (double) a.R[i] * a.R[i]; }
        return d == 0.0 ? -400.0 : 10.0 * std::log10 (d / (e + 1e-30) + 1e-40);
    };
    auto go = [] (Plug& pl) {
        RenderOpts o; if (isEffect (pl)) o.audioIn = testInput (pl.sr, 3.5);
        return render (pl, phrase (pl.sr), S (4.5, pl.sr), o);
    };
    auto a = load (pluginPath), b = load (pluginPath);
    const auto ra = go (*a), rb = go (*b);
    const double n1 = nullDb (ra, rb);
    report ("repro", U ("dos instancias nuevas, el mismo MIDI"), n1 < -100.0 ? "PASS" : "WARN", n1 <= -400.0 ? U ("idénticas") : fmt ("diferencia %.1f dB", n1));
    auto c = load (pluginPath);
    go (*c);
    c->p->releaseResources(); c->p->prepareToPlay (c->sr, c->block); c->ph.ppq = 0;
    const auto rc = go (*c);
    const double n2 = nullDb (ra, rc);
    report ("repro", U ("una que ya tocó, preparada de nuevo"), n2 < -100.0 ? "PASS" : "WARN", n2 <= -400.0 ? U ("idénticas") : fmt ("diferencia %.1f dB", n2));
    // Bloques al azar de 1 a 512 muestras (y de 0 en VST3) contra bloques fijos de 512: lo mismo.
    auto e = load (pluginPath);
    RenderOpts ro; ro.randomBlocks = true; if (isEffect (*e)) ro.audioIn = testInput (e->sr, 3.5);
    const auto re = render (*e, phrase (e->sr), S (4.5, e->sr), ro);
    const double n3 = nullDb (ra, re);
    report ("repro", U ("bloques al azar contra bloques de 512"), n3 < -100.0 ? "PASS" : "WARN", n3 <= -400.0 ? U ("idénticas") : fmt ("diferencia %.1f dB", n3));
}

// MIDI de salida atrasado la latencia (QA en Live 2026-10-22, P1: el host compensa el audio pero no el MIDI de salida,
// y una pista que dobla por MIDI From quedaba adelantada). Una nota de entrada a los 0,5 s: la primera nota que sale
// (eco en Thru, o la que el plugin genera con esa nota) tiene que salir a 0,5 s + latencia. WARN si no.
void testMidiLatency()
{
    auto pl = load (pluginPath);
    if (! pl->p->producesMidi() || ! pl->p->acceptsMidi()) return;
    const int L = pl->p->getLatencySamples();
    const juce::int64 at = S (0.5, pl->sr);
    pl->ph.playing = true;
    auto r = render (*pl, { { at, on (60, 100) }, { at + S (0.4, pl->sr), off (60) } }, S (1.5, pl->sr), {});
    juce::int64 first = -1;
    for (auto& e : r.midi) if (e.m.isNoteOn()) { first = e.sample; break; }
    if (first < 0) { report ("midilatency", U ("nota de salida = entrada + latencia"), "PASS", fmt ("sin notas de salida con la configuración inicial (latencia %d)", L)); return; }
    const juce::int64 d = first - at;
    report ("midilatency", U ("nota de salida = entrada + latencia"), d == L ? "PASS" : "WARN",
            fmt ("sale %lld muestras después de la entrada; latencia informada %d", (long long) d, L));
}

// MIDI de salida a tiempo con el audio (QA en Live 2026-10-22, P1): un plugin con latencia L saca su audio L muestras
// tarde y el host lo compensa; el MIDI que produce no lo compensa nadie, así que tiene que salir también L muestras
// tarde. Con el transporte andando (secuenciadores) y un acorde en 0,5 s (instrumentos que sacan lo que tocan), la
// primera nota de salida tiene que caer donde empieza el audio. Sólo plugins con MIDI de salida y latencia.
void testMidiSync()
{
    auto pl = load (pluginPath);
    const int L = pl->p->getLatencySamples();
    if (! pl->p->producesMidi() || L <= 0 || isGenerator (*pl)) return;
    // Sólo el VST3: Live y REAPER no entregan el MIDI de salida de un AU, y el AU hospedado acá trae sus notas sin la
    // posición dentro del bloque (todas al principio).
    if (! pluginPath.endsWithIgnoreCase (".vst3")) return;
    for (auto* p : realParams (*pl)) if (p->getName (64).equalsIgnoreCase ("MIDI out")) p->setValueNotifyingHost (1.0f);
    pl->ph.playing = true;
    const juce::int64 at = S (0.5, pl->sr);
    std::vector<Ev> ev;
    for (int n : { 48, 55, 60, 64 }) { ev.push_back ({ at, on (n, 100) }); ev.push_back ({ at + S (1.0, pl->sr), off (n) }); }
    auto r = render (*pl, ev, S (2.0, pl->sr), {});
    juce::int64 tm = -1, ta = -1;
    for (auto& e : r.midi) if (e.m.isNoteOn()) { tm = e.sample; break; }
    for (size_t i = 0; i < r.L.size(); ++i) if (std::abs (r.L[i]) > 1.0e-3f || std::abs (r.R[i]) > 1.0e-3f) { ta = (juce::int64) i; break; }
    if (tm < 0 || ta < 0) { report ("midisync", U ("MIDI de salida con el audio"), "WARN", fmt ("sin notas o sin audio (nota %lld, audio %lld)", (long long) tm, (long long) ta)); return; }
    const juce::int64 d = tm - ta;
    report ("midisync", U ("MIDI de salida con el audio"), std::abs ((double) d) <= 8.0 ? "PASS" : "FAIL",
            fmt ("primera nota en %lld, audio desde %lld (%+lld muestras; latencia %d)", (long long) tm, (long long) ta, (long long) d, L));
}

// Editor escondido (QA en Live 2026-10-22, H3): Live crea el editor al insertar y después sólo esconde la ventana,
// sin cerrarlo. Un editor escondido no tiene que gastar CPU (timers, animaciones, repintados). Mide la CPU del
// proceso (sin audio) sin editor, con la ventana del host vacía y escondida (la referencia), con el editor a la vista
// y escondido. WARN si escondido gasta más de 2 % de un núcleo sobre la referencia. Medición ruidosa (±3 %): repetir.
// No reproduce lo que midió la QA en Live (8-26 %): JUCE ya no pinta una ventana escondida, lo que queda son los timers.
// (Tapar la ventana con otra no sirve para probar: macOS no la marcó ocluida en esta prueba.)
double processCpuSeconds()
{
   #if JUCE_WINDOWS
    return -1.0;
   #else
    rusage ru {}; getrusage (RUSAGE_SELF, &ru);
    return (double) ru.ru_utime.tv_sec + 1e-6 * (double) ru.ru_utime.tv_usec + (double) ru.ru_stime.tv_sec + 1e-6 * (double) ru.ru_stime.tv_usec;
   #endif
}

void testEditor()
{
    auto pl = load (pluginPath);
    if (! pl->p->hasEditor()) return;
    if (processCpuSeconds() < 0) { report ("editor", U ("CPU del editor escondido"), "WARN", U ("sin medición de CPU en este sistema")); return; }
    auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
    auto measure = [&] (int ms) {
        const double c0 = processCpuSeconds(); const auto t0 = Clock::now();
        pump (ms);
        return 100.0 * (processCpuSeconds() - c0) / std::chrono::duration<double> (Clock::now() - t0).count();
    };
    pump (500);
    const double base = measure (3000);
    juce::DocumentWindow w ("athosttest", juce::Colours::black, 0, true);
    w.setUsingNativeTitleBar (true);
    w.setTopLeftPosition (40, 40);
    // La ventana del host vacía, mostrada y escondida: lo que cuesta sin el plugin (el enlace de pantalla de macOS).
    juce::Component empty; empty.setSize (400, 300);
    w.setContentNonOwned (&empty, true);
    w.setVisible (true); pump (800); w.setVisible (false); pump (500);
    const double baseWin = measure (3000);
    w.clearContentComponent();
    std::unique_ptr<juce::AudioProcessorEditor> ed (pl->p->createEditorIfNeeded());
    if (ed == nullptr) { report ("editor", U ("CPU del editor escondido"), "FAIL", U ("createEditor devolvió nullptr")); return; }
    w.setContentNonOwned (ed.get(), true);
    w.setAlwaysOnTop (true);         // athosttest no es la aplicación del frente: sin esto la tapan otras ventanas
    w.setVisible (true); w.toFront (false);
    pump (1500);
    const double shown = measure (3000);
    pump (500);
    w.setVisible (false);
    pump (500);
    const double hidden = measure (3000);
    w.setVisible (true); w.toFront (false);
    pump (1000);
    const double again = measure (2000);
    w.setVisible (false);
    w.clearContentComponent();
    ed.reset();
    pump (300);
    const double extra = hidden - baseWin;
    report ("editor", U ("CPU del editor escondido"), extra > 2.0 ? "WARN" : "PASS",
            fmt ("%% de un núcleo: sin editor %.1f, ventana vacía escondida %.1f, a la vista %.1f, escondido %.1f, de nuevo a la vista %.1f",
                 base, baseWin, shown, hidden, again));
}

// ───────── Orrery: Main y satélites, puertos virtuales ─────────
// Varias instancias del mismo VST3 en el mismo proceso (como las pistas de un DAW), procesadas ciclo a ciclo con el
// mismo transporte y en un orden al azar (o cada una en su hilo).
juce::AudioProcessorParameter* paramNamed (Plug& pl, const juce::String& name)
{
    for (auto* p : realParams (pl)) if (p->getName (64) == name) return p;
    return nullptr;
}
void setChoice (Plug& pl, const juce::String& name, int index, int count)
{
    if (auto* p = paramNamed (pl, name)) p->setValueNotifyingHost (count > 1 ? (float) index / (float) (count - 1) : 0.0f);
}

// El estado propio del plugin dentro del estado VST3 que guarda el host (IComponent → bloque XML de JUCE).
bool editPluginState (Plug& pl, const std::function<void (juce::XmlElement&)>& edit, juce::MemoryBlock* result = nullptr)
{
    juce::MemoryBlock mb; pl.p->getStateInformation (mb);
    auto host = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
    if (host == nullptr) return false;
    auto* comp = host->getChildByName ("IComponent");
    if (comp == nullptr) return false;
    juce::MemoryBlock inner; inner.fromBase64Encoding (comp->getAllSubText().trim());
    auto x = juce::AudioProcessor::getXmlFromBinary (inner.getData(), (int) inner.getSize());
    if (x == nullptr) return false;
    // El bloque XML (8 bytes de cabecera + texto) y lo que el envoltorio VST3 agrega después.
    const auto* d = static_cast<const juce::uint8*> (inner.getData());
    const size_t used = 8 + (size_t) juce::ByteOrder::littleEndianInt (d + 4) + 1;
    juce::MemoryBlock tail; if (used < inner.getSize()) tail.append (d + used, inner.getSize() - used);
    edit (*x);
    juce::MemoryBlock fresh; juce::AudioProcessor::copyXmlToBinary (*x, fresh);
    fresh.append (tail.getData(), tail.getSize());
    comp->deleteAllTextElements(); comp->addTextElement (fresh.toBase64Encoding());
    juce::MemoryBlock out; juce::AudioProcessor::copyXmlToBinary (*host, out);
    if (result != nullptr) { *result = out; return true; }
    pl.p->setStateInformation (out.getData(), (int) out.getSize());
    return true;
}
juce::String pluginStateAttr (Plug& pl, const char* attr)
{
    juce::String v;
    juce::MemoryBlock dummy;
    editPluginState (pl, [&] (juce::XmlElement& x) { v = x.getStringAttribute (attr); }, &dummy);
    return v;
}
juce::String pluginParamValue (Plug& pl, const char* id)       // como entero ("1", no "1.0")
{
    juce::String v;
    juce::MemoryBlock dummy;
    editPluginState (pl, [&] (juce::XmlElement& x) { for (auto* c : x.getChildIterator()) if (c->getStringAttribute ("id") == id) v = juce::String ((int) std::lround (c->getDoubleAttribute ("value"))); }, &dummy);
    return v;
}

struct Rig
{
    std::vector<std::unique_ptr<Plug>> inst;
    std::vector<std::vector<Ev>> midi;
    juce::int64 pos = 0; double ppq = 0.0;
    std::mt19937 rng { 7 };
    bool playing = false, threads = false;
    int block = 512;
    std::vector<int> bypassed;     // por instancia: 1 = bypass
    void add (double sr, int n) { inst.push_back (load (pluginPath, sr, n)); midi.emplace_back(); bypassed.push_back (0); block = n; }
    void cycle()
    {
        const int N = (int) inst.size();
        const double sr = inst[0]->sr;
        const juce::uint64 ns = (juce::uint64) (5.0e9 + (double) pos / sr * 1.0e9);
        for (auto& p : inst) { p->ph.playing = playing; p->ph.ppq = ppq; p->ph.pos = pos; p->ph.bpm = 100.0; p->ph.hostNs = ns; p->ph.hasHostNs = true; }
        auto one = [&] (int i) {
            auto& p = *inst[(size_t) i];
            juce::AudioBuffer<float> buf (std::max (2, p.outs), block); buf.clear();
            juce::MidiBuffer mb;
            if (bypassed[(size_t) i]) p.p->processBlockBypassed (buf, mb); else p.p->processBlock (buf, mb);
            for (const auto m : mb) midi[(size_t) i].push_back ({ pos + m.samplePosition, m.getMessage() });
        };
        if (threads)
        {
            std::vector<std::thread> th;
            for (int i = 0; i < N; ++i) th.emplace_back ([&, i] { if (rng() % 2) std::this_thread::yield(); one (i); });
            for (auto& t : th) t.join();
        }
        else
        {
            std::vector<int> order ((size_t) N); for (int i = 0; i < N; ++i) order[(size_t) i] = i;
            std::shuffle (order.begin(), order.end(), rng);
            for (int i : order) one (i);
        }
        pos += block;
        if (playing) ppq += block * 100.0 / 60.0 / sr;
    }
    void run (double secs) { const juce::int64 end = pos + (juce::int64) (secs * inst[0]->sr); while (pos < end) cycle(); }
};

// Los mensajes de unos canales, desde una muestra.
std::vector<Ev> onChannels (const std::vector<Ev>& v, const std::set<int>& chans, juce::int64 from = 0)
{
    std::vector<Ev> r;
    for (auto& e : v) if (e.sample >= from && chans.count (e.m.getChannel())) r.push_back (e);
    return r;
}
int sameMidi (const std::vector<Ev>& a, const std::vector<Ev>& b)
{
    int m = 0;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
        if (a[i].sample == b[i].sample && a[i].m.getRawDataSize() == b[i].m.getRawDataSize()
            && std::memcmp (a[i].m.getRawData(), b[i].m.getRawData(), (size_t) a[i].m.getRawDataSize()) == 0) ++m;
    return (a.size() == b.size() && m == (int) a.size()) ? -1 : m;
}

void testConstellation()
{
    // Sólo el VST3: el estado se lee en su formato (IComponent), y los hosts sacan el MIDI de Orrery sólo del VST3.
    if (! pluginPath.endsWithIgnoreCase (".vst3")) { std::printf ("  roles: sólo con el VST3\n"); return; }
    {
        auto probe = load (pluginPath);
        if (paramNamed (*probe, "Role") == nullptr || paramNamed (*probe, "Satellite Lanes") == nullptr) return;     // sólo Orrery 1.0.5+
    }
    // 1. Auto: la primera es Main, las siguientes satélites en el próximo carril libre; se guarda y se restaura.
    {
        Rig rig; for (int i = 0; i < 4; ++i) rig.add (48000.0, 512);
        rig.run (0.1);
        juce::StringArray roles, lanes;
        for (auto& p : rig.inst) { roles.add (pluginStateAttr (*p, "autoRole")); lanes.add (pluginStateAttr (*p, "autoLanes")); }
        const bool ok = roles.joinIntoString (",") == "2,3,3,3" && lanes.joinIntoString (",") == "0,1,2,4";
        report ("roles", U ("1 Auto: Main y satélites"), ok ? "PASS" : "FAIL", "rol " + roles.joinIntoString (",") + U (", carriles ") + lanes.joinIntoString (","));
        // Preview: el Main sigue con el sonido de prueba, los satélites nuevos empiezan sin él.
        juce::StringArray pv; for (auto& p : rig.inst) pv.add (pluginParamValue (*p, "preview"));
        report ("roles", U ("1 Auto: sonido de prueba"), pv.joinIntoString (",") == "1,0,0,0" ? "PASS" : "FAIL", "preview " + pv.joinIntoString (","));
        // Reabrir el proyecto: cada una recupera su rol aunque se carguen en otro orden.
        std::vector<juce::MemoryBlock> st (4); for (int i = 0; i < 4; ++i) rig.inst[(size_t) i]->p->getStateInformation (st[(size_t) i]);
        rig.inst.clear();
        Rig again; for (int i = 0; i < 4; ++i) again.add (48000.0, 512);
        for (int i : { 3, 1, 0, 2 }) again.inst[(size_t) i]->p->setStateInformation (st[(size_t) i].getData(), (int) st[(size_t) i].getSize());
        again.run (0.1);
        juce::StringArray r2, l2; for (auto& p : again.inst) { r2.add (pluginStateAttr (*p, "autoRole")); l2.add (pluginStateAttr (*p, "autoLanes")); }
        report ("roles", U ("1 Auto: proyecto reabierto"), r2.joinIntoString (",") == "2,3,3,3" && l2.joinIntoString (",") == "0,1,2,4" ? "PASS" : "FAIL",
                "rol " + r2.joinIntoString (",") + U (", carriles ") + l2.joinIntoString (","));
    }
    // 2. Un proyecto de 1.0.4 (sin rol) abre como Independent con su Link, aunque ya haya un Main.
    {
        Rig rig; rig.add (48000.0, 512); rig.add (48000.0, 512);
        rig.run (0.1);
        auto& old = *rig.inst[1];
        juce::MemoryBlock legacy;
        editPluginState (old, [] (juce::XmlElement& x) {
            for (auto* id : { "role", "sat_lanes", "skip_sat", "midi_ports" })
                for (auto* c : x.getChildIterator()) if (c->getStringAttribute ("id") == id) { x.removeChildElement (c, true); break; }
            x.removeAttribute ("autoRole"); x.removeAttribute ("autoLanes");
            for (auto* c : x.getChildIterator()) if (c->getStringAttribute ("id") == "link") c->setAttribute ("value", 1.0);    // Lead
            for (auto* c : x.getChildIterator()) if (c->getStringAttribute ("id") == "run") c->setAttribute ("value", 1.0);
        }, &legacy);
        rig.add (48000.0, 512);
        auto& fresh = *rig.inst[2];
        fresh.p->setStateInformation (legacy.getData(), (int) legacy.getSize());
        rig.run (3.0);
        const auto role = pluginParamValue (fresh, "role"), link = pluginParamValue (fresh, "link");
        std::set<int> ch; int ons = 0; for (auto& e : rig.midi[2]) if (e.m.isNoteOn()) { ++ons; ch.insert (e.m.getChannel()); }
        const bool ok = role == "1" && link == "1" && ch.size() >= 3;
        report ("roles", U ("2 estado de 1.0.4"), ok ? "PASS" : "FAIL", fmt ("rol %s (1 = Independent), Link %s (1 = Lead), %d notas en %d canales", role.toRawUTF8(), link.toRawUTF8(), ons, (int) ch.size()));
    }
    // 3. Satélites iguales al Main, nota por nota y muestra por muestra.
    for (int mode = 0; mode < 3; ++mode)
        for (int n : { 64, 512, 1024 })
        {
            if (mode == 2 && n != 512) continue;
            Rig rig; rig.threads = mode == 2;
            for (int i = 0; i < 4; ++i) rig.add (48000.0, n);
            auto& M = *rig.inst[0];
            setChoice (M, "Skip Satellite Lanes", 0, 2);                 // el Main manda todo, para comparar
            setChoice (*rig.inst[3], "Satellite Lanes", 0x30, 256);         // un satélite con dos carriles (5 y 6)
            M.p->setCurrentProgram (0);                                     // First Light: carriles 1, 2, 3, 5 y 6
            setChoice (M, "Clock", mode == 0 ? 1 : 0, 2);
            rig.run (0.3);                                                  // se sincronizan con el Main parado
            rig.playing = mode == 0;                                        // con el reloj del host, arrancar el transporte ya es Play (1.0.6)
            setChoice (M, "Play", 1, 2);
            rig.run (20.0);
            setChoice (M, "Density", 80, 101);                              // cambios en marcha
            rig.run (10.0);
            M.p->setCurrentProgram (1);                                     // otro preset en marcha (Tape Loops)
            setChoice (M, "Play", 1, 2);
            rig.run (20.0);
            setChoice (M, "Play", 0, 2);
            rig.run (2.0);
            juce::String d; bool ok = true; int total = 0;
            const std::vector<std::set<int>> chans { { 1 }, { 2 }, { 5, 6 } };
            for (int s = 1; s < 4; ++s)
            {
                const auto want = onChannels (rig.midi[0], chans[(size_t) s - 1]);
                const int r = sameMidi (want, rig.midi[(size_t) s]);
                total += (int) want.size();
                ok = ok && r == -1 && stuckNotes (rig.midi[(size_t) s]).empty() && want.size() > 20;
                d << (r == -1 ? juce::String ((int) want.size()) + "/" + juce::String ((int) want.size()) : juce::String (r) + "/" + juce::String ((int) want.size())) << " ";
            }
            static const char* modeName[3] = { "host en marcha", "reloj propio", "reloj propio, hilos" };
            report ("roles", fmt ("3 satélites = Main, %s, bloque %d", modeName[mode], n), ok ? "PASS" : "FAIL", U ("mensajes iguales por satélite: ") + d);
        }
    // 4. Skip: el Main no manda los carriles de sus satélites; 5. bypass de un satélite y del Main: sin notas colgadas.
    {
        Rig rig; for (int i = 0; i < 3; ++i) rig.add (48000.0, 256);
        auto& M = *rig.inst[0];
        setChoice (*rig.inst[1], "Satellite Lanes", 0x04, 256);         // la melodía (carril 3)
        setChoice (*rig.inst[2], "Satellite Lanes", 0x10, 256);         // la nube (carril 5)
        M.p->setCurrentProgram (0);
        setChoice (M, "Clock", 0, 2);
        rig.run (0.2);
        setChoice (M, "Play", 1, 2);
        rig.run (15.0);
        std::map<int, int> perCh; for (auto& e : rig.midi[0]) if (e.m.isNoteOn()) perCh[e.m.getChannel()]++;
        const bool skip = perCh[3] == 0 && perCh[5] == 0 && perCh[1] > 0 && perCh[2] > 0;
        std::map<int, int> satCh; for (auto& e : rig.midi[1]) if (e.m.isNoteOn()) satCh[e.m.getChannel()]++;
        report ("roles", U ("4 el Main salta los carriles de los satélites"), skip && satCh[3] > 0 ? "PASS" : "FAIL",
                fmt ("notas del Main por canal: 1=%d 2=%d 3=%d 5=%d 6=%d; el satélite del carril 3: %d", perCh[1], perCh[2], perCh[3], perCh[5], perCh[6], satCh[3]));
        auto onsBetween = [&] (int i, juce::int64 a, juce::int64 b) { int k = 0; for (auto& e : rig.midi[(size_t) i]) if (e.m.isNoteOn() && e.sample >= a && e.sample < b) ++k; return k; };
        const juce::int64 b0 = rig.pos;
        rig.bypassed[1] = 1; rig.run (3.0);
        const juce::int64 b1 = rig.pos;
        rig.bypassed[1] = 0; rig.run (6.0);
        const juce::int64 b2 = rig.pos;
        auto offs = [&] (int i, juce::int64 at) { int k = 0; for (auto& e : rig.midi[(size_t) i]) if (e.sample == at && (e.m.isNoteOff() || e.m.isAllNotesOff())) ++k; return k; };
        report ("roles", U ("5 bypass de un satélite"), onsBetween (1, b0 + 256, b1) == 0 && onsBetween (1, b1, b2) > 0 ? "PASS" : "FAIL",
                fmt ("%d note-off al entrar, %d notas en bypass, %d al volver", offs (1, b0), onsBetween (1, b0 + 256, b1), onsBetween (1, b1, b2)));
        rig.bypassed[0] = 1; rig.run (3.0);
        const juce::int64 b3 = rig.pos;
        rig.bypassed[0] = 0; rig.run (15.0);
        const juce::int64 b4 = rig.pos;
        report ("roles", U ("5 bypass del Main: los satélites callan y vuelven"), onsBetween (1, b2 + 512, b3) + onsBetween (2, b2 + 512, b3) == 0 && onsBetween (1, b3, b4) + onsBetween (2, b3, b4) > 0 ? "PASS" : "FAIL",
                fmt ("%d notas de satélites sin el Main, %d al volver (el Main: %d)", onsBetween (1, b2 + 512, b3) + onsBetween (2, b2 + 512, b3), onsBetween (1, b3, b4) + onsBetween (2, b3, b4), onsBetween (0, b3, b4)));
        setChoice (M, "Play", 0, 2);
        rig.run (2.0);
        int stuck = 0; for (int i = 0; i < 3; ++i) stuck += (int) stuckNotes (rig.midi[(size_t) i]).size();
        report ("roles", U ("5 al parar, nada colgado"), stuck == 0 ? "PASS" : "FAIL", fmt ("%d notas sin note-off en las tres", stuck));
    }
    if (paramNamed (*load (pluginPath), "Play") == nullptr) return;
    // 6. QA en DAWs (2026-10-22). La misma variación con el reloj del host, como en REAPER: un rato con el host parado y
    // Play puesto (cuánto, depende del reloj de pared), después el transporte desde 0, con o sin pre-roll y con otro
    // buffer: las mismas notas en la misma muestra (H2). Arrancar el transporte es Play y pararlo es Stop (O1).
    {
        struct V { int n; int stoppedBlocks; int preroll; };
        std::vector<std::vector<Ev>> runs; juce::String d; bool ok = true, edges = true;
        for (auto v : { V { 512, 0, 0 }, V { 64, 900, 0 }, V { 333, 37, 3 * 333 }, V { 1024, 5, 12345 }, V { 768, 0, 0 } })
        {
            Rig rig; rig.add (48000.0, v.n);
            auto& P = *rig.inst[0];
            setChoice (P, "Role", 1, 4);                                   // Independent
            P.p->setCurrentProgram (0);
            setChoice (P, "Play", 1, 2);
            for (int b = 0; b < v.stoppedBlocks; ++b) rig.cycle();
            rig.midi[0].clear();
            const juce::int64 start = rig.pos + v.preroll;                 // la muestra del pulso 0
            rig.ppq = -(double) v.preroll * 100.0 / 60.0 / 48000.0;
            rig.playing = true;
            rig.run (20.0);
            edges = edges && pluginParamValue (P, "run") == "1";
            rig.playing = false; rig.run (0.5);
            edges = edges && pluginParamValue (P, "run") == "0" && stuckNotes (rig.midi[0]).empty();
            std::vector<Ev> r;
            // Desde 2 muestras antes del pulso 0: el pulso que el rig suma bloque a bloque puede redondear una muestra.
            for (auto& e : rig.midi[0]) if (e.sample - start >= -2 && e.sample - start < S (19.0, 48000.0) && ! (e.m.isNoteOff() && e.sample == start - v.preroll))
                r.push_back ({ e.sample - start, e.m });
            runs.push_back (r);
        }
        for (size_t k = 1; k < runs.size(); ++k)
        {
            int m = 0; const auto& a0 = runs[0]; const auto& b0 = runs[k];
            std::vector<bool> used (b0.size(), false);
            juce::String miss;
            for (auto& x : a0)
            {
                bool found = false;
                for (size_t j = 0; j < b0.size() && ! found; ++j)
                    if (! used[j] && std::abs ((double) (b0[j].sample - x.sample)) <= 1.0 && b0[j].m.getRawDataSize() == x.m.getRawDataSize()
                        && std::memcmp (b0[j].m.getRawData(), x.m.getRawData(), (size_t) x.m.getRawDataSize()) == 0) { used[j] = true; ++m; found = true; }
                if (! found && miss.isEmpty()) miss = fmt (" (falta en %.4f s: ", (double) x.sample / 48000.0) + x.m.getDescription() + ")";
            }
            ok = ok && m == (int) a0.size() && b0.size() == a0.size() && a0.size() > 20;
            d << m << "/" << (int) a0.size() << miss << " ";
        }
        report ("roles", U ("6 la misma variación: buffers 64/333/1024/768, pre-roll, espera con el host parado"), ok ? "PASS" : "FAIL", U ("mensajes iguales (±1 muestra): ") + d);
        report ("roles", U ("6 el transporte arranca y para (Play lo muestra)"), edges ? "PASS" : "FAIL", edges ? U ("Play sigue al transporte, nada colgado") : U ("Play no siguió al transporte o quedaron notas"));
    }
    // 7. Duplicate Track (Live copia el estado a una instancia nueva mientras la original sigue abierta): la copia no
    // hereda el rol; resuelve Auto de nuevo (O2). Un proyecto que se reabre conserva los roles (prueba 1).
    {
        Rig rig; for (int i = 0; i < 2; ++i) rig.add (48000.0, 512);
        rig.run (0.1);
        juce::MemoryBlock satState, mainState;
        rig.inst[1]->p->getStateInformation (satState); rig.inst[0]->p->getStateInformation (mainState);
        rig.add (48000.0, 512); rig.inst[2]->p->setStateInformation (satState.getData(), (int) satState.getSize());
        rig.add (48000.0, 512); rig.inst[3]->p->setStateInformation (mainState.getData(), (int) mainState.getSize());
        rig.run (0.1);
        juce::StringArray roles, lanes; for (auto& p : rig.inst) { roles.add (pluginStateAttr (*p, "autoRole")); lanes.add (pluginStateAttr (*p, "autoLanes")); }
        const bool ok = roles.joinIntoString (",") == "2,3,3,3" && lanes.joinIntoString (",") == "0,1,2,4";
        report ("roles", U ("7 copias de un satélite y del Main: satélites en los carriles libres"), ok ? "PASS" : "FAIL",
                "rol " + roles.joinIntoString (",") + U (", carriles ") + lanes.joinIntoString (","));
    }
}

// 6. Los carriles como puertos MIDI virtuales (macOS y Linux): se crean, dos instancias no chocan, se borran al cerrar.
void testPorts()
{
    {
        auto probe = load (pluginPath);
        if (paramNamed (*probe, "Publish Lanes As MIDI Ports") == nullptr) return;
    }
    auto pump = [] (int ms) {
       #if JUCE_MODAL_LOOPS_PERMITTED
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
       #else
        juce::Thread::sleep (ms);
       #endif
    };
    auto names = [] { juce::StringArray s; for (auto& d : juce::MidiInput::getAvailableDevices()) s.add (d.name); return s; };
    auto count = [&] (const juce::String& prefix) { int k = 0; for (auto& n : names()) if (n.startsWith (prefix + " ")) ++k; return k; };
    const int before = count ("Orrery");
    {
        auto a = load (pluginPath), b = load (pluginPath);
        for (auto* p : { a.get(), b.get() }) { setChoice (*p, "Role", 1, 4); setChoice (*p, "Publish Lanes As MIDI Ports", 1, 2); p->p->releaseResources(); p->p->prepareToPlay (p->sr, p->block); }
        pump (1500);
        const int one = count ("Orrery") - count ("Orrery 2") - before, two = count ("Orrery 2");
        auto r = render (*a, {}, S (0.5, a->sr));
       #if JUCE_WINDOWS
        report ("ports", U ("6 puertos virtuales"), one == 0 && two == 0 ? "PASS" : "FAIL", U ("Windows no tiene puertos virtuales propios (loopMIDI)"));
       #else
        if (one == 0 && two == 0) report ("ports", U ("6 puertos virtuales"), "WARN", U ("el sistema no creó puertos (¿sin servidor MIDI?)"));
        else report ("ports", U ("6 puertos virtuales, dos instancias"), one == 8 && two == 8 ? "PASS" : "FAIL",
                     fmt ("%d de la primera, %d de la segunda: ", one, two) + names().joinIntoString (", ").substring (0, 160));
       #endif
    }
    pump (1500);
    const int after = count ("Orrery") - before;
    report ("ports", U ("6 puertos borrados al cerrar"), after == 0 ? "PASS" : "FAIL", fmt ("%d puertos de Orrery quedan", after));
}

void testMidiOut()
{
    auto probe = load (pluginPath);
    if (! probe->p->producesMidi()) return;
    probe.reset();
    const double sr = 48000.0;
    auto check = [&] (const juce::String& name, Out& r, bool expectNotes) {
        auto st = stuckNotes (r.midi);
        int ons = 0, chans = 0; std::set<int> ch;
        for (auto& e : r.midi) { if (e.m.isNoteOn()) ++ons; if (e.m.getChannel() > 0) ch.insert (e.m.getChannel()); }
        chans = (int) ch.size();
        juce::String d = fmt ("%d mensajes, %d note-on, %d canales, %d sin note-off, %d fuera del bloque", (int) r.midi.size(), ons, chans, (int) st.size(), r.badOffsets);
        if (! st.empty()) { d << " ("; for (size_t i = 0; i < std::min<size_t> (4, st.size()); ++i) d << "ch" << st[i].first << " n" << st[i].second << " "; d << ")"; }
        juce::String status = ! st.empty() || r.badOffsets > 0 ? "FAIL" : (expectNotes && ons == 0 ? "WARN" : "PASS");
        report ("midiout", name, status, d);
    };
    // Notas de entrada con el transporte parado.
    { auto pl = load (pluginPath); auto r = render (*pl, phrase (sr), S (6.0, sr), {}); check ("frase de entrada, transporte parado", r, false); }
    // Transporte: play, saltos de loop, cambio de tempo, stop.
    {
        auto pl = load (pluginPath);
        std::vector<Ev> ev;
        for (int i = 0; i < 16; ++i) { ev.push_back ({ S (0.2 + i * 0.5, sr), on (48 + (i * 5) % 24) }); ev.push_back ({ S (0.6 + i * 0.5, sr), off (48 + (i * 5) % 24) }); }
        RenderOpts o; o.transport = { { 0, 0, 0 }, { S (3.0, sr), 2, 0.0 }, { S (5.0, sr), 3, 87.0 }, { S (6.0, sr), 2, 2.0 }, { S (8.5, sr), 1, 0 } };
        auto r = render (*pl, ev, S (12.0, sr), o);
        check ("transporte con saltos, tempo y stop", r, false);
    }
    // Parámetros al azar con transporte corriendo y notas; al final stop + CC123 en todos los canales.
    for (int seed : { 1, 2, 3, 4 })
    {
        auto pl = load (pluginPath);
        auto params = realParams (*pl);
        juce::Random rnd (seed * 13);
        std::vector<Ev> ev;
        for (double s = 0.1; s < 18.0; s += 0.1 + rnd.nextDouble() * 0.4)
        { const int n = 36 + rnd.nextInt (48); ev.push_back ({ S (s, sr), on (n, 1 + rnd.nextInt (127)) }); ev.push_back ({ S (s + 0.05 + rnd.nextDouble(), sr), off (n) }); }
        RenderOpts o;
        o.transport = { { 0, 0, 0 }, { S (7.0, sr), 1, 0 }, { S (8.0, sr), 0, 0 }, { S (12.0, sr), 2, 1.0 }, { S (19.0, sr), 1, 0 } };
        auto* play = isGenerator (*pl) ? paramNamed (*pl, "Play") : nullptr;     // Orrery con reloj Free no para con el transporte
        o.perBlock = [&] (juce::int64 pos, int) {
            if (pos > S (18.5, sr)) { if (play != nullptr && pos > S (19.0, sr)) play->setValue (0.0f); return; }
            if (rnd.nextInt (8) == 0) params[rnd.nextInt (params.size())]->setValue (rnd.nextFloat());
        };
        for (int ch = 1; ch <= 16; ++ch) ev.push_back ({ S (19.0, sr), cc (123, 0, ch) });
        auto r = render (*pl, ev, S (24.0, sr), o);
        check (U ("parámetros al azar + transporte, semilla ") + juce::String (seed), r, false);
    }
}

// ───────── cadena: generador → sinte ─────────
void chain (const juce::String& genPath, const juce::String& synthPath, std::vector<int> genPrograms, int synthProgram, double secs)
{
    auto probe = load (genPath);
    if (genPrograms.empty()) for (int k = 0; k < probe->p->getNumPrograms(); ++k) genPrograms.push_back (k);
    probe.reset();
    pluginPath = synthPath;
    double restDb;      // reposo del sinte con el mismo programa (un drone suena sin notas)
    {
        auto r = load (synthPath);
        if (synthProgram >= 0) r->p->setCurrentProgram (synthProgram);
        auto o = render (*r, {}, S (secs + waitSec (*r) + 9.0, r->sr), {});
        restDb = db (stats (o, durSec (o, r->sr) - 0.5, durSec (o, r->sr), r->sr).rms);
    }
    for (int gp : genPrograms)
    {
        auto g = load (genPath), s = load (synthPath);
        g->p->setCurrentProgram (gp);
        if (synthProgram >= 0) s->p->setCurrentProgram (synthProgram);
        juce::AudioProcessorParameter* playParam = nullptr;      // Orrery: Play encendido, y apagado al final
        for (auto* prm : realParams (*g)) if (prm->getName (32) == "Play") playParam = prm;
        if (playParam != nullptr) playParam->setValue (1.0f);
        const juce::String gname = g->p->getProgramName (gp);
        const double sr = g->sr; const int n = g->block;
        const juce::int64 total = S (secs + waitSec (*s) + 9.0, sr), stopAt = S (secs, sr);
        juce::AudioBuffer<float> gb (std::max (2, g->outs), n), sb (std::max ({ 2, s->ins, s->outs }), n);
        juce::MidiBuffer mb;
        std::vector<Ev> genOut; Out so; double maxMs = 0, sumMs = 0; int blocks = 0, bad = 0;
        g->ph.playing = s->ph.playing = true;
        for (juce::int64 pos = 0; pos < total; pos += n)
        {
            if (pos >= stopAt) { g->ph.playing = s->ph.playing = false; if (playParam != nullptr) playParam->setValue (0.0f); }
            for (auto* pl : { g.get(), s.get() }) pl->ph.pos = pos;
            mb.clear(); gb.clear();
            g->p->processBlock (gb, mb);
            for (const auto md : mb) { genOut.push_back ({ pos + md.samplePosition, md.getMessage() }); if (md.samplePosition < 0 || md.samplePosition >= n) ++bad; }
            sb.clear();
            const auto t0 = Clock::now();
            s->p->processBlock (sb, mb);
            const double ms = std::chrono::duration<double, std::milli> (Clock::now() - t0).count();
            maxMs = std::max (maxMs, ms); sumMs += ms; ++blocks;
            so.L.insert (so.L.end(), sb.getReadPointer (0), sb.getReadPointer (0) + n);
            so.R.insert (so.R.end(), sb.getReadPointer (s->outs > 1 ? 1 : 0), sb.getReadPointer (s->outs > 1 ? 1 : 0) + n);
            for (auto* pl : { g.get(), s.get() }) if (pl->ph.playing) pl->ph.ppq += n * pl->ph.bpm / 60.0 / sr;
        }
        const double T = durSec (so, sr);
        auto a = stats (so, 0, T, sr), play = stats (so, 5.0, secs, sr), end = stats (so, T - 0.5, T, sr);
        auto st = stuckNotes (genOut);
        int notes = 0; for (auto& e : genOut) notes += e.m.isNoteOn();
        juce::String d = fmt ("%d notas, nivel %.0f dB, pico %.2f, final %.0f dB (reposo %.0f), CPU sinte %.1f %% (máx bloque %.0f %%)",
                                                  notes, db (play.rms), a.peak, db (end.rms), restDb, 100.0 * sumMs / blocks / (1000.0 * n / sr), 100.0 * maxMs / (1000.0 * n / sr));
        juce::String status = "PASS";
        if (a.nan + a.inf > 0) { status = "FAIL"; d << ", NaN/Inf"; }
        if (a.peak > 4.0) { status = "FAIL"; d << ", pico excesivo"; } else if (a.peak > 1.6) status = status == "FAIL" ? status : "WARN";
        if (! st.empty()) { status = "FAIL"; d << U (", el generador dejó ") << (int) st.size() << " notas sin note-off"; }
        if (bad > 0) { status = "FAIL"; d << ", " << bad << " eventos fuera del bloque"; }
        if (db (end.rms) > std::max (restDb + 6.0, -70.0)) { status = "FAIL"; d << ", el sinte sigue sonando"; }
        if (notes == 0) { status = "WARN"; d << ", sin notas"; }
        plugName = g->p->getName() + "_" + s->p->getName();
        if (status != "PASS") writeWav (so, sr, "chain_" + gname);
        report ("chain", juce::String (gp) + " " + gname, status, d);
    }
}

void writeJson()
{
    juce::Array<juce::var> arr;
    for (auto& r : results)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("test", r.test); o->setProperty ("name", r.name); o->setProperty ("status", r.status); o->setProperty ("detail", r.detail);
        arr.add (juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("plugin", plugName); root->setProperty ("fails", fails); root->setProperty ("warns", warns); root->setProperty ("results", arr);
    outDir.getChildFile (plugName.replaceCharacters (" /", "__") + ".json").replaceWithText (juce::JSON::toString (juce::var (root)));
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (juce::String::fromUTF8 (argv[i]));
    auto opt = [&] (const char* n, juce::String d) { const int i = args.indexOf (n); return i >= 0 && i + 1 < args.size() ? args[i + 1] : d; };
    outDir = juce::File::getCurrentWorkingDirectory().getChildFile (opt ("--out", "hosttest-out"));
    outDir.createDirectory();
    const bool quick = args.contains ("--quick");
    if (args.isEmpty()) { std::printf ("uso: athosttest <plugin> [--tests a,b] [--out dir] [--quick] | --chain <gen> <sinte> [...]\n"); return 2; }

    if (args[0] == "--chain" && args.size() >= 3)
    {
        std::vector<int> gps;
        const auto g = opt ("--gen-programs", "all");
        if (g != "all") for (auto& x : juce::StringArray::fromTokens (g, ",", "")) gps.push_back (x.getIntValue());
        plugName = "chain";
        chain (juce::File::getCurrentWorkingDirectory().getChildFile (args[1]).getFullPathName(), juce::File::getCurrentWorkingDirectory().getChildFile (args[2]).getFullPathName(),
               gps, opt ("--synth-program", "-1").getIntValue(), opt ("--secs", "120").getDoubleValue());
        plugName = "chain_" + juce::File (args[1]).getFileNameWithoutExtension() + "_" + juce::File (args[2]).getFileNameWithoutExtension();
        writeJson();
        std::printf ("\n%d FAIL, %d WARN\n", fails, warns);
        return fails > 0 ? 1 : 0;
    }

    pluginPath = juce::File::getCurrentWorkingDirectory().getChildFile (args[0]).getFullPathName();
    plugName = juce::File (pluginPath).getFileNameWithoutExtension();
   #if JUCE_MAC
    if (pluginPath.endsWithIgnoreCase (".component") && ! pluginPath.contains ("/Library/Audio/Plug-Ins/Components"))
    {
        const auto id = registerBuiltAu (pluginPath);
        if (id.isEmpty()) { std::printf ("no se pudo registrar el AU compilado %s\n", pluginPath.toRawUTF8()); return 2; }
        std::printf ("AU compilado registrado en este proceso como %s (no la versión instalada)\n", id.toRawUTF8());
        pluginPath = id;
        plugName << "_AU";
    }
   #endif
    // Estado de cada programa tal como lo guarda un host VST3 (el flujo de IComponent::getState): <dir>/<n>.bin y
    // <dir>/programs.txt. Sirve para armar sesiones de un DAW con los presets ya cargados (Ableton lo guarda en hex).
    if (args.contains ("--dump-states"))
    {
        auto pl = load (pluginPath); auto& p = *pl->p;
        juce::File dir (opt ("--dump-states", "states"));
        dir.createDirectory();
        juce::String list;
        for (int k = 0; k < juce::jmax (1, p.getNumPrograms()); ++k)
        {
            if (p.getNumPrograms() > 0) p.setCurrentProgram (k);
            juce::MemoryBlock mb; p.getStateInformation (mb);
            auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
            juce::MemoryBlock comp;
            if (xml != nullptr)
                if (auto* c = xml->getChildByName ("IComponent")) comp.fromBase64Encoding (c->getAllSubText().trim());
            dir.getChildFile (juce::String (k) + ".bin").replaceWithData (comp.getData(), comp.getSize());
            list << k << "\t" << p.getProgramName (k) << "\t" << (int) comp.getSize() << "\n";
        }
        dir.getChildFile ("programs.txt").replaceWithText (list);
        std::printf ("%d programas en %s\n", juce::jmax (1, p.getNumPrograms()), dir.getFullPathName().toRawUTF8());
        return 0;
    }
    if (args.contains ("--list-params"))
    {
        auto pl = load (pluginPath); int i = 0, autom = 0;
        for (auto* p : realParams (*pl))
        {
            std::printf ("%3d %s (%d pasos)%s\n", i++, p->getName (40).toRawUTF8(), p->getNumSteps(), p->isAutomatable() ? "" : " [no automatizable]");
            autom += p->isAutomatable() ? 1 : 0;
        }
        std::printf ("%d parámetros, %d automatizables (Ableton Live llena el panel del dispositivo con hasta 64)\n", i, autom);
        return 0;
    }
    fxProgram = opt ("--fx-program", "-1").getIntValue();
    juce::StringArray tests = juce::StringArray::fromTokens (opt ("--tests", "info,programs,midi,rates,state,fuzz,params,cpu,reprepare,midiout,midisync,stopped,bypass,hostgap,freeze,roles,ports"), ",", "");
    std::printf ("%s\n", pluginPath.toRawUTF8());
    const auto t0 = Clock::now();
    for (auto& t : tests)
    {
        std::printf ("── %s\n", t.toRawUTF8());
        if (t == "info") testInfo();
        else if (t == "programs") testPrograms (quick);
        else if (t == "midi") testMidi();
        else if (t == "rates") testRates (quick);
        else if (t == "state") testState();
        else if (t == "fuzz") testFuzz (quick);
        else if (t == "params") testParams();
        else if (t == "cpu") testCpu();
        else if (t == "reprepare") testReprepare();
        else if (t == "midiout") testMidiOut();
        else if (t == "stopped") testStopped();
        else if (t == "bypass") testBypass();
        else if (t == "hostgap") testHostGap();
        else if (t == "editor") testEditor();
        else if (t == "midifx") testMidiFx();
        else if (t == "progorder") testProgramOrder();
        else if (t == "statebytes") testStateBytes();
        else if (t == "repro") testRepro();
        else if (t == "midilatency") testMidiLatency();
        else if (t == "midisync") testMidiSync();
        else if (t == "freeze") testFreezeState();
        else if (t == "roles") testConstellation();
        else if (t == "ports") testPorts();
    }
    writeJson();
    std::printf ("\n%s: %d FAIL, %d WARN en %.0f s\n", plugName.toRawUTF8(), fails, warns, std::chrono::duration<double> (Clock::now() - t0).count());
    return fails > 0 ? 1 : 0;
}
