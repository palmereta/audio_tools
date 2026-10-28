#pragma once
// Parámetros del host de Valence: una sola tabla para el plugin, la interfaz, los presets y valencerender.
// Los primeros 64 son los automatizables (Live arma su panel con los primeros 64): 20 globales y 11 por capa. Detrás,
// los ajustes finos de cada capa y los de la sesión, que no se automatizan. Las filas de pasos van en el estado.

#include <JuceHeader.h>
#include "gen/Engine.h"
#include "gen/Preview.h"

namespace dh::val {

struct PDef
{
    juce::String id, name;
    int steps;              // > 0 con choices: elección; > 0 sin choices: entero; 0: continuo
    float lo, hi, def;
    juce::String choices;   // separadas por |
    juce::String unit;
    bool global = false;    // no lo pisan los presets (el cableado y la sesión)
    bool automatable = true;
};

inline juce::String scaleChoices() { juce::StringArray s; for (auto& x : scales()) s.add (x.name); return s.joinIntoString ("|"); }
inline juce::String figureChoices() { juce::StringArray s; for (int f = 0; f < kFigures; ++f) s.add (figureName (f)); return s.joinIntoString ("|"); }
inline juce::String patternChoices() { juce::StringArray s; for (int p = 0; p < PatternCount; ++p) s.add (patternName (p)); return s.joinIntoString ("|"); }
inline juce::String voiceChoices() { juce::StringArray s; for (int v = 0; v < VoiceCount; ++v) s.add (voiceName (v)); return s.joinIntoString ("|"); }

// Por defecto: cuatro capas de largos primos entre sí sobre el mismo acorde, todas al canal 1.
struct LayerDefault { bool on; int figure; int length; int pattern; int octave; int voice; };
inline const std::array<LayerDefault, kLayers>& layerDefaults()
{
    static const std::array<LayerDefault, kLayers> d { {
        { true, 6, 16, PatUp, 0, VoicePluck }, { true, 6, 15, PatUpDown, -1, VoiceBass },
        { false, 3, 13, PatChord, 0, VoiceStab }, { false, 3, 7, PatPedal, -2, VoiceSub },
    } };
    return d;
}

inline const std::vector<PDef>& params()
{
    static const std::vector<PDef> all = [] {
        std::vector<PDef> v;
        auto add = [&] (PDef d) { v.push_back (d); };
        // ── automatizables: globales (20)
        add ({ "run", "Play", 2, 0, 1, 0, "Stopped|Playing", "", true });
        add ({ "clock", "Clock", 2, 0, 1, 1, "Free|Host", "", true });
        add ({ "tempo", "Tempo", 0, 40, 240, 122, "", " BPM" });
        add ({ "key", "Key", 12, 0, 11, 9, "C|C#|D|Eb|E|F|F#|G|Ab|A|Bb|B", "" });
        add ({ "scale", "Scale", (int) scales().size(), 0, (float) scales().size() - 1, 1, scaleChoices(), "" });
        add ({ "seed", "Variation", 9999, 1, 9999, 1, "", "" });
        add ({ "phrase", "Phrase", 17, 0, 16, 0, "", " bars" });
        add ({ "source", "Harmony", 2, 0, 1, 0, "Played|Auto", "" });
        add ({ "complete", "Complete", 5, 0, 4, 0, "Off|Triad|Seventh|Ninth|Octaves", "" });
        add ({ "order", "Chord Order", 4, 0, 3, 1, "As played|Low to high|High to low|Random", "" });
        add ({ "latch", "Latch", 2, 0, 1, 0, "Off|On", "" });
        add ({ "transpose", "Transpose", 49, -24, 24, 0, "", " st" });
        add ({ "chord_bars", "Chord Length", 16, 1, 16, 2, "", " bars" });
        add ({ "progression", "Progression", 3, 0, 2, 0, "Walk|Two chords|Drone", "" });
        add ({ "accent", "Accent", 61, 0, 60, 24, "", "" });
        add ({ "out_mode", "Output Channels", 2, 0, 1, 0, "All on 1|Per layer", "", true });
        add ({ "preview", "Preview Sound", 2, 0, 1, 1, "Off|On", "", true });
        add ({ "preview_vol", "Preview Volume", 0, 0, 1, 0.6f, "", "", true });
        add ({ "clock_out", "MIDI Clock Out", 2, 0, 1, 0, "Off|On", "", true });
        add ({ "link", "Link", 3, 0, 2, 0, "Off|Lead|Follow", "", true });
        // ── automatizables: lo principal de cada capa (11 × 4)
        for (int l = 1; l <= kLayers; ++l)
        {
            const auto& D = layerDefaults()[(size_t) l - 1];
            const juce::String p = "l" + juce::String (l) + "_", n = "Layer " + juce::String (l) + " ";
            add ({ p + "on", n + "On", 2, 0, 1, D.on ? 1.0f : 0.0f, "Off|On", "" });
            add ({ p + "channel", n + "Channel", 16, 1, 16, (float) l, "", "" });
            add ({ p + "rate", n + "Rate", kFigures, 0, kFigures - 1, (float) D.figure, figureChoices(), "" });
            add ({ p + "length", n + "Length", 32, 1, 32, (float) D.length, "", " steps" });
            add ({ p + "pattern", n + "Pattern", PatternCount, 0, PatternCount - 1, (float) D.pattern, patternChoices(), "" });
            add ({ p + "octaves", n + "Octaves", 4, 1, 4, 1, "", "" });
            add ({ p + "octave", n + "Register", 7, -3, 3, (float) D.octave, "", "" });
            add ({ p + "gate", n + "Gate", 0, 0.05f, 1.0f, 0.5f, "", "" });
            add ({ p + "swing", n + "Swing", 26, 50, 75, 50, "", " %" });
            add ({ p + "eu_pulses", n + "Euclid Pulses", 33, 0, 32, 0, "", "" });
            add ({ p + "mutate", n + "Mutate", 0, 0, 1, 0, "", "" });
        }
        // ── no automatizables: el resto de cada capa
        for (int l = 1; l <= kLayers; ++l)
        {
            const auto& D = layerDefaults()[(size_t) l - 1];
            const juce::String p = "l" + juce::String (l) + "_", n = "Layer " + juce::String (l) + " ";
            add ({ p + "mult", n + "Rate Multiplier", 16, 1, 16, 1, "", "", false, false });
            add ({ p + "oct_mode", n + "Octave Mode", 3, 0, 2, 0, "Stack|Alternate|Random", "", false, false });
            add ({ p + "shift", n + "Index Shift", 16, 0, 15, 0, "", "", false, false });
            add ({ p + "vel_mode", n + "Velocity Source", 3, 0, 2, 0, "Fixed|Played|Steps", "", false, false });
            add ({ p + "velocity", n + "Velocity", 127, 1, 127, 100, "", "", false, false });
            add ({ p + "humanize", n + "Humanize", 0, 0, kMaxHumanizeMs, 0, "", " ms", false, false });
            add ({ p + "eu_rot", n + "Euclid Rotate", 32, 0, 31, 0, "", "", false, false });
            add ({ p + "restart", n + "New Chord", 2, 0, 1, 1, "Keep phase|Restart", "", false, false });
            add ({ p + "cc", n + "CC Number", 121, -1, 119, -1, "", "", false, false });      // 120-127 son mensajes de modo de canal
            add ({ p + "voice", n + "Preview Voice", VoiceCount, 0, VoiceCount - 1, (float) D.voice, voiceChoices(), "", false, false });
        }
        add ({ "preview_space", "Preview Space", 0, 0, 1, 0.35f, "", "", true, false });
        add ({ "midi_ports", "Publish Layers As MIDI Ports", 2, 0, 1, 0, "Off|On", "", true, false });
        return v;
    }();
    return all;
}

inline int paramIndex (const juce::String& id)
{
    const auto& ps = params();
    for (size_t i = 0; i < ps.size(); ++i) if (ps[i].id == id) return (int) i;
    return -1;
}

inline juce::String formatParam (const PDef& d, float v)
{
    if (d.choices.isNotEmpty()) return juce::StringArray::fromTokens (d.choices, "|", "")[juce::jlimit (0, d.steps - 1, (int) std::lround (v - d.lo))];
    if (d.id.endsWith ("_cc")) { const int c = (int) std::lround (v); return c < 0 ? juce::String ("Off") : "CC " + juce::String (c); }
    if (d.id == "phrase") { const int b = (int) std::lround (v); return b == 0 ? juce::String ("Off") : juce::String (b) + (b == 1 ? " bar" : " bars"); }
    if (d.id.endsWith ("eu_pulses")) { const int p = (int) std::lround (v); return p == 0 ? juce::String ("Steps") : juce::String (p); }
    if (d.id.endsWith ("_mult")) return juce::String::charToString (0xd7) + juce::String ((int) std::lround (v));
    if (d.id.endsWith ("octave") || d.id == "transpose") { const int o = (int) std::lround (v); return (o > 0 ? "+" : "") + juce::String (o) + d.unit; }
    if (d.steps > 0) return juce::String ((int) std::lround (v)) + d.unit;
    if (d.unit.isNotEmpty()) return juce::String (v, d.hi > 50 ? 0 : 1) + d.unit;     // String (x, 0) imprime todos los decimales: nunca 0
    return juce::String ((int) std::lround (v * 100.0f)) + " %";
}

// Índices de cada parámetro en params(), calculados una vez (el hilo de audio no busca por texto).
struct Ix
{
    int run, clock, tempo, key, scale, seed, phrase, source, complete, order, latch, transpose, chordBars, progression, accent, outMode,
        preview, previewVol, clockOut, link, previewSpace, midiPorts;
    struct Layer { int on, channel, rate, length, pattern, octaves, octave, gate, swing, euPulses, mutate,
                   mult, octMode, shift, velMode, velocity, humanize, euRot, restart, cc, voice; };
    std::array<Layer, kLayers> layer {};
    static const Ix& get() { static Ix x; return x; }
private:
    Ix()
    {
        auto f = [] (const char* id) { return paramIndex (id); };
        run = f ("run"); clock = f ("clock"); tempo = f ("tempo"); key = f ("key"); scale = f ("scale"); seed = f ("seed"); phrase = f ("phrase");
        source = f ("source"); complete = f ("complete"); order = f ("order"); latch = f ("latch"); transpose = f ("transpose");
        chordBars = f ("chord_bars"); progression = f ("progression"); accent = f ("accent"); outMode = f ("out_mode");
        preview = f ("preview"); previewVol = f ("preview_vol"); clockOut = f ("clock_out"); link = f ("link");
        previewSpace = f ("preview_space"); midiPorts = f ("midi_ports");
        for (int l = 0; l < kLayers; ++l)
        {
            const juce::String p = "l" + juce::String (l + 1) + "_";
            auto g = [&] (const char* id) { return paramIndex (p + id); };
            auto& L = layer[(size_t) l];
            L.on = g ("on"); L.channel = g ("channel"); L.rate = g ("rate"); L.length = g ("length"); L.pattern = g ("pattern");
            L.octaves = g ("octaves"); L.octave = g ("octave"); L.gate = g ("gate"); L.swing = g ("swing"); L.euPulses = g ("eu_pulses");
            L.mutate = g ("mutate"); L.mult = g ("mult"); L.octMode = g ("oct_mode"); L.shift = g ("shift"); L.velMode = g ("vel_mode");
            L.velocity = g ("velocity"); L.humanize = g ("humanize"); L.euRot = g ("eu_rot"); L.restart = g ("restart"); L.cc = g ("cc"); L.voice = g ("voice");
        }
    }
};

// Vuelca los valores (en el orden de params()) en los ajustes del motor (sin las filas de pasos).
inline void applyParams (const float* v, GlobalParams& g, std::array<LayerParams, kLayers>& layers)
{
    const auto& x = Ix::get();
    auto i = [v] (int k) { return (int) std::lround (v[k]); };
    g.run = i (x.run) != 0; g.hostClock = i (x.clock) != 0; g.tempo = v[x.tempo]; g.key = i (x.key); g.scale = i (x.scale);
    g.seed = (uint32_t) std::max (1, i (x.seed)); g.phraseBars = i (x.phrase); g.source = i (x.source); g.complete = i (x.complete);
    g.order = i (x.order); g.latch = i (x.latch) != 0; g.transpose = i (x.transpose); g.chordBars = i (x.chordBars);
    g.progression = i (x.progression); g.accent = i (x.accent); g.perLayerChannels = i (x.outMode) != 0;
    for (int l = 0; l < kLayers; ++l)
    {
        const auto& k = x.layer[(size_t) l];
        auto& L = layers[(size_t) l];
        L.on = i (k.on) != 0; L.channel = i (k.channel); L.figure = i (k.rate); L.length = i (k.length); L.pattern = i (k.pattern);
        L.octaves = i (k.octaves); L.octave = i (k.octave); L.gate = v[k.gate]; L.swing = i (k.swing); L.euPulses = i (k.euPulses);
        L.mutate = v[k.mutate]; L.mult = i (k.mult); L.octMode = i (k.octMode); L.shift = i (k.shift); L.velMode = i (k.velMode);
        L.velocity = i (k.velocity); L.humanizeMs = v[k.humanize]; L.euRotate = i (k.euRot); L.restart = i (k.restart) != 0; L.ccNum = i (k.cc);
    }
}
inline std::array<int, kLayers> voicesOf (const float* v)
{
    std::array<int, kLayers> r {};
    for (int l = 0; l < kLayers; ++l) r[(size_t) l] = juce::jlimit (0, VoiceCount - 1, (int) std::lround (v[Ix::get().layer[(size_t) l].voice]));
    return r;
}

// ───────── filas de pasos: texto estable (el estado) y JSON (los presets) ─────────
using Rows = std::array<std::array<Step, kMaxSteps>, kLayers>;

inline Rows defaultRows() { Rows r; for (auto& L : r) { LayerParams p; L = p.steps; } return r; }

// "on vel acc tie rat prob oct fixed cc finger" por paso, pasos separados por ';'. Siempre los 32: el mismo estado
// da los mismos bytes.
inline juce::String rowsToString (const std::array<Step, kMaxSteps>& s)
{
    juce::String out;
    for (int k = 0; k < kMaxSteps; ++k)
    {
        const auto& x = s[(size_t) k];
        if (k > 0) out << ';';
        out << (int) x.on << ' ' << (int) x.vel << ' ' << (int) x.accent << ' ' << (int) x.tie << ' ' << (int) x.ratchet << ' '
            << (int) x.prob << ' ' << (int) x.oct << ' ' << (int) x.fixed << ' ' << (int) x.cc << ' ' << (int) x.finger;
    }
    return out;
}
inline std::array<Step, kMaxSteps> rowsFromString (const juce::String& str)
{
    LayerParams def; auto s = def.steps;
    const auto steps = juce::StringArray::fromTokens (str, ";", "");
    for (int k = 0; k < kMaxSteps && k < steps.size(); ++k)
    {
        const auto f = juce::StringArray::fromTokens (steps[k], " ", "");
        if (f.size() < 10) continue;
        auto& x = s[(size_t) k];
        x.on = f[0].getIntValue() != 0; x.vel = (uint8_t) juce::jlimit (1, 127, f[1].getIntValue()); x.accent = f[2].getIntValue() != 0;
        x.tie = f[3].getIntValue() != 0; x.ratchet = (uint8_t) juce::jlimit (1, 4, f[4].getIntValue()); x.prob = (uint8_t) juce::jlimit (0, 100, f[5].getIntValue());
        x.oct = (int8_t) juce::jlimit (-2, 2, f[6].getIntValue()); x.fixed = (int8_t) juce::jlimit (-1, 21, f[7].getIntValue());
        x.cc = (int8_t) juce::jlimit (-1, 127, f[8].getIntValue()); x.finger = (int8_t) juce::jlimit (0, 15, f[9].getIntValue());
    }
    return s;
}
// Presets: {"l1": {"on": [1,0,...], "vel": [...], "accent", "tie", "ratchet", "prob", "oct", "fixed", "cc", "finger"}}. Una fila
// más corta que 32 se repite (un acento cada 4 es [1,0,0,0]); las que faltan quedan por defecto.
inline Rows rowsFromVar (const juce::var& v)
{
    Rows r = defaultRows();
    if (! v.isObject()) return r;
    for (int l = 0; l < kLayers; ++l)
    {
        const auto lv = v.getProperty ("l" + juce::String (l + 1), {});
        if (! lv.isObject()) continue;
        auto row = [&] (const char* name, auto&& set) {
            const auto a = lv.getProperty (name, {});
            if (const auto* arr = a.getArray(); arr != nullptr && ! arr->isEmpty())
                for (int k = 0; k < kMaxSteps; ++k) set (r[(size_t) l][(size_t) k], (int) (*arr)[k % arr->size()]);
        };
        row ("on", [] (Step& s, int x) { s.on = x != 0; });
        row ("vel", [] (Step& s, int x) { s.vel = (uint8_t) juce::jlimit (1, 127, x); });
        row ("accent", [] (Step& s, int x) { s.accent = x != 0; });
        row ("tie", [] (Step& s, int x) { s.tie = x != 0; });
        row ("ratchet", [] (Step& s, int x) { s.ratchet = (uint8_t) juce::jlimit (1, 4, x); });
        row ("prob", [] (Step& s, int x) { s.prob = (uint8_t) juce::jlimit (0, 100, x); });
        row ("oct", [] (Step& s, int x) { s.oct = (int8_t) juce::jlimit (-2, 2, x); });
        row ("fixed", [] (Step& s, int x) { s.fixed = (int8_t) juce::jlimit (-1, 21, x); });
        row ("cc", [] (Step& s, int x) { s.cc = (int8_t) juce::jlimit (-1, 127, x); });
        row ("finger", [] (Step& s, int x) { s.finger = (int8_t) juce::jlimit (0, 15, x); });
    }
    return r;
}

} // namespace dh::val
