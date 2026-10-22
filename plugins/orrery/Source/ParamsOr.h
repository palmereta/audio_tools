#pragma once
// Parámetros del host de Orrery: los globales y una plantilla repetida para los 8 carriles (lN_...).
// Una sola tabla para el plugin, la interfaz, los presets y orreryrender.

#include <JuceHeader.h>
#include "gen/Generator.h"

namespace dh::orr {

struct PDef
{
    juce::String id, name;
    int steps;              // > 0 con choices: elección; > 0 sin choices: entero; 0: continuo
    float lo, hi, def;
    juce::String choices;   // separadas por |
    juce::String unit;
    bool global = false;    // no lo pisan los presets
    bool automatable = true;
};

inline juce::String scaleChoices() { juce::StringArray s; for (auto& x : scales()) s.add (x.name); return s.joinIntoString ("|"); }
inline juce::String roleChoices() { juce::StringArray s; for (int r = 0; r < RoleCount; ++r) s.add (roleName (r)); return s.joinIntoString ("|"); }

// Valores por defecto de cada carril: una orquesta tranquila lista para sonar.
struct LaneDefault { bool on; int role; int ch; float dens; bool loop; float loopBeats; int echoes; };
inline const std::array<LaneDefault, kLanes>& laneDefaults()
{
    static const std::array<LaneDefault, kLanes> d { {
        { true, Drone, 1, 0.5f, false, 23.0f, 0 }, { true, Pad, 2, 0.5f, false, 29.0f, 0 },
        { true, Melody, 3, 0.35f, true, 17.0f, 1 }, { false, Arp, 4, 0.4f, true, 13.0f, 0 },
        { true, Cloud, 5, 0.3f, false, 31.0f, 3 }, { true, Bass, 6, 0.4f, false, 37.0f, 0 },
        { false, Pulse, 7, 0.5f, false, 19.0f, 0 }, { false, Cloud, 8, 0.2f, true, 41.0f, 2 },
    } };
    return d;
}

inline const std::vector<PDef>& params()
{
    static const std::vector<PDef> all = [] {
        std::vector<PDef> v;
        auto add = [&] (PDef d) { v.push_back (d); };
        add ({ "run", "Play", 2, 0, 1, 0, "Stopped|Playing", "", true });
        add ({ "clock", "Clock", 2, 0, 1, 1, "Free|Host", "", true });
        add ({ "run_stopped", "Play With Host Stopped", 2, 0, 1, 1, "Off|On", "", true });
        add ({ "tempo", "Tempo", 0, 20, 140, 60, "", " BPM" });
        add ({ "key", "Key", 12, 0, 11, 2, "C|C#|D|Eb|E|F|F#|G|Ab|A|Bb|B", "" });
        add ({ "scale", "Scale", (int) scales().size(), 0, (float) scales().size() - 1, 2, scaleChoices(), "" });
        add ({ "density", "Density", 0, 0, 1, 0.5f, "", "" });
        add ({ "motion", "Motion", 0, 0, 1, 0.5f, "", "" });
        add ({ "space", "Space", 0, 0, 1, 0.5f, "", "" });
        add ({ "brightness", "Brightness", 0, 0, 1, 0.5f, "", "" });
        add ({ "tension", "Tension", 0, 0, 1, 0.2f, "", "" });
        add ({ "chord_beats", "Chord Length", 63, 2, 64, 16, "", " beats" });
        add ({ "color", "Chord Color", 0, 0, 1, 0.5f, "", "" });
        add ({ "progression", "Progression", 3, 0, 2, 0, "Slow walk|Drone|Two chords", "" });
        add ({ "seed", "Variation", 9999, 1, 9999, 1, "", "" });
        add ({ "breath", "Breathing", 13, 0, 12, 0, "", " /min" });
        add ({ "session", "Session", 121, 0, 120, 0, "", " min" });
        add ({ "fade_in", "Fade In", 0, 0, 120, 20, "", " s" });
        add ({ "fade_out", "Fade Out", 0, 0, 300, 60, "", " s" });
        add ({ "gentle", "Gentle", 2, 0, 1, 0, "Off|On", "" });
        add ({ "fade_cc7", "Fade With CC7", 2, 0, 1, 1, "Off|On", "" });
        add ({ "humanize", "Humanize", 0, 0, 30, 8, "", " ms" });
        add ({ "level", "Level", 0, 0, 1, 0.85f, "", "", true });
        add ({ "ref_pitch", "Reference Pitch", 0, 415, 466, 440, "", " Hz", true });
        add ({ "tune_bend", "Tune By Pitch Bend", 2, 0, 1, 0, "Off|On", "", true });
        add ({ "bend_range", "Synth Bend Range", 5, 0, 4, 0, "2|3|12|24|48", "", true });
        add ({ "preview", "Preview Sound", 2, 0, 1, 1, "Off|On", "", true });
        add ({ "preview_vol", "Preview Volume", 0, 0, 1, 0.6f, "", "", true });
        add ({ "preview_rev", "Preview Reverb", 0, 0, 1, 0.6f, "", "", true });
        add ({ "clock_out", "MIDI Clock Out", 2, 0, 1, 0, "Off|On", "", true });
        add ({ "link", "Link", 3, 0, 2, 0, "Off|Lead|Follow", "", true });
        for (int l = 1; l <= kLanes; ++l)
        {
            const auto& D = laneDefaults()[(size_t) l - 1];
            const juce::String p = "l" + juce::String (l) + "_", n = "Lane " + juce::String (l) + " ";
            add ({ p + "on", n + "On", 2, 0, 1, D.on ? 1.0f : 0.0f, "Off|On", "" });
            add ({ p + "role", n + "Role", RoleCount, 0, RoleCount - 1, (float) D.role, roleChoices(), "" });
            add ({ p + "channel", n + "Channel", 16, 1, 16, (float) D.ch, "", "" });
            add ({ p + "octave", n + "Octave", 7, -3, 3, 0, "", "" });
            add ({ p + "range", n + "Range", 45, 4, 48, 19, "", " st" });
            add ({ p + "density", n + "Density", 0, 0, 1, D.dens, "", "" });
            add ({ p + "length", n + "Length", 0, 0, 1, 0.5f, "", "" });
            add ({ p + "velocity", n + "Velocity", 0, 0, 1, 0.6f, "", "" });
            add ({ p + "vel_rand", n + "Velocity Random", 0, 0, 1, 0.25f, "", "" });
            add ({ p + "prob", n + "Probability", 0, 0, 1, 1.0f, "", "" });
            add ({ p + "loop", n + "Loop", 2, 0, 1, D.loop ? 1.0f : 0.0f, "Off|On", "" });
            add ({ p + "loop_beats", n + "Loop Length", 127, 2, 128, D.loopBeats, "", " beats" });
            add ({ p + "mutate", n + "Mutate", 0, 0, 1, 0.15f, "", "" });
            add ({ p + "rate", n + "Rate", 5, 0, 4, 2, "2|1|1/2|1/4|1/8", "" });
            add ({ p + "eu_steps", n + "Euclid Steps", 32, 1, 32, 8, "", "" });
            add ({ p + "eu_pulses", n + "Euclid Pulses", 33, 0, 32, 0, "", "" });
            add ({ p + "eu_rot", n + "Euclid Rotate", 32, 0, 31, 0, "", "" });
            add ({ p + "arp", n + "Arp Pattern", 4, 0, 3, ArpUpDown, "Up|Down|Up-down|Random", "" });
            add ({ p + "echoes", n + "Echoes", 9, 0, 8, (float) D.echoes, "", "" });
            add ({ p + "echo_time", n + "Echo Time", 0, 0.125f, 4, 0.75f, "", " beats" });
            add ({ p + "echo_decay", n + "Echo Decay", 0, 0, 0.95f, 0.55f, "", "" });
            add ({ p + "follow", n + "Follow", 3, 0, 2, 0, "Chord|Scale|Root", "" });
            add ({ p + "voices", n + "Voices", 8, 1, 8, 4, "", "" });
            add ({ p + "cc", n + "CC Number", 121, -1, 119, -1, "", "" });     // 120-127 son mensajes de modo de canal
            add ({ p + "cc_shape", n + "CC Shape", 4, 0, 3, 0, "Sine|Drift|Breath|Swell", "" });
            add ({ p + "cc_beats", n + "CC Period", 128, 1, 128, 16, "", " beats" });
            add ({ p + "cc_depth", n + "CC Depth", 0, 0, 1, 0.4f, "", "" });
            add ({ p + "cc_center", n + "CC Center", 0, 0, 1, 0.5f, "", "" });
            add ({ p + "drift", n + "Drift", 0, 0, 50, 0, "", " cents" });
        }
        // 1.0.5: Main y satélites, y los carriles como puertos MIDI. Al final, para no mover los anteriores; no se
        // automatizan (son el cableado de la sesión, como los puertos directos).
        add ({ "role", "Role", 4, 0, 3, 0, "Auto|Independent|Main|Satellite", "", true, false });
        add ({ "sat_lanes", "Satellite Lanes", 256, 0, 255, 0, "", "", true, false });
        add ({ "skip_sat", "Skip Satellite Lanes", 2, 0, 1, 1, "Off|On", "", true, false });
        add ({ "midi_ports", "Publish Lanes As MIDI Ports", 2, 0, 1, 0, "Off|On", "", true, false });
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
    if (d.id.endsWith ("_cc") || d.id.endsWith ("cc")) { const int c = (int) std::lround (v); return c < 0 ? juce::String ("Off") : "CC " + juce::String (c); }
    if (d.id == "session") { const int m = (int) std::lround (v); return m == 0 ? juce::String ("Endless") : juce::String (m) + " min"; }
    if (d.id == "sat_lanes")
    {
        const int m = (int) std::lround (v);
        if (m == 0) return "Auto";
        juce::StringArray s; for (int l = 0; l < kLanes; ++l) if ((m >> l) & 1) s.add (juce::String (l + 1));
        return s.joinIntoString ("+");
    }
    if (d.id == "breath") { const int b = (int) std::lround (v); return b == 0 ? juce::String ("Off") : juce::String (b) + " /min"; }
    if (d.id.endsWith ("octave")) { const int o = (int) std::lround (v); return (o > 0 ? "+" : "") + juce::String (o); }
    if (d.steps > 0) return juce::String ((int) std::lround (v)) + d.unit;
    if (d.unit.isNotEmpty()) return juce::String (v, d.hi > 50 ? 0 : (d.hi > 5 ? 1 : 2)) + d.unit;
    return juce::String ((int) std::lround (v * 100.0f)) + " %";
}

// Índices de cada parámetro en params(), calculados una vez (el hilo de audio no busca por texto).
struct Ix
{
    int run, clock, runStopped, tempo, key, scale, density, motion, space, brightness, tension, chordBeats, color, progression, seed,
        breath, session, fadeIn, fadeOut, gentle, fadeCC7, humanize, level, refPitch, tuneBend, bendRange, preview, previewVol, previewRev, clockOut, link,
        role, satLanes, skipSat, midiPorts;
    struct Lane { int on, role, channel, octave, range, density, length, velocity, velRand, prob, loop, loopBeats, mutate, rate, euSteps, euPulses, euRot,
                  arp, echoes, echoTime, echoDecay, follow, voices, cc, ccShape, ccBeats, ccDepth, ccCenter, drift; };
    std::array<Lane, kLanes> lane {};
    static const Ix& get() { static Ix x; return x; }
private:
    Ix()
    {
        auto f = [] (const char* id) { return paramIndex (id); };
        run = f ("run"); clock = f ("clock"); runStopped = f ("run_stopped"); tempo = f ("tempo"); key = f ("key"); scale = f ("scale");
        density = f ("density"); motion = f ("motion"); space = f ("space"); brightness = f ("brightness"); tension = f ("tension");
        chordBeats = f ("chord_beats"); color = f ("color"); progression = f ("progression"); seed = f ("seed"); breath = f ("breath");
        session = f ("session"); fadeIn = f ("fade_in"); fadeOut = f ("fade_out"); gentle = f ("gentle"); fadeCC7 = f ("fade_cc7");
        humanize = f ("humanize"); level = f ("level"); refPitch = f ("ref_pitch"); tuneBend = f ("tune_bend"); bendRange = f ("bend_range");
        preview = f ("preview"); previewVol = f ("preview_vol"); previewRev = f ("preview_rev"); clockOut = f ("clock_out"); link = f ("link");
        role = f ("role"); satLanes = f ("sat_lanes"); skipSat = f ("skip_sat"); midiPorts = f ("midi_ports");
        for (int l = 0; l < kLanes; ++l)
        {
            const juce::String p = "l" + juce::String (l + 1) + "_";
            auto g = [&] (const char* id) { return paramIndex (p + id); };
            auto& L = lane[(size_t) l];
            L.on = g ("on"); L.role = g ("role"); L.channel = g ("channel"); L.octave = g ("octave"); L.range = g ("range"); L.density = g ("density");
            L.length = g ("length"); L.velocity = g ("velocity"); L.velRand = g ("vel_rand"); L.prob = g ("prob"); L.loop = g ("loop");
            L.loopBeats = g ("loop_beats"); L.mutate = g ("mutate"); L.rate = g ("rate"); L.euSteps = g ("eu_steps"); L.euPulses = g ("eu_pulses");
            L.euRot = g ("eu_rot"); L.arp = g ("arp"); L.echoes = g ("echoes"); L.echoTime = g ("echo_time"); L.echoDecay = g ("echo_decay");
            L.follow = g ("follow"); L.voices = g ("voices"); L.cc = g ("cc"); L.ccShape = g ("cc_shape"); L.ccBeats = g ("cc_beats");
            L.ccDepth = g ("cc_depth"); L.ccCenter = g ("cc_center"); L.drift = g ("drift");
        }
    }
};

// Vuelca los valores (en el orden de params()) en los ajustes del generador.
inline void applyParams (const float* v, GlobalParams& g, std::array<LaneParams, kLanes>& lanes)
{
    const auto& x = Ix::get();
    auto i = [v] (int k) { return (int) std::lround (v[k]); };
    g.run = i (x.run) != 0; g.hostClock = i (x.clock) != 0; g.runWhenStopped = i (x.runStopped) != 0;
    g.tempo = v[x.tempo]; g.key = i (x.key); g.scale = i (x.scale);
    g.density = v[x.density]; g.motion = v[x.motion]; g.space = v[x.space]; g.brightness = v[x.brightness]; g.tension = v[x.tension];
    g.chordBeats = v[x.chordBeats]; g.color = v[x.color]; g.progression = i (x.progression); g.seed = (uint32_t) i (x.seed);
    g.breathRate = v[x.breath]; g.sessionMinutes = v[x.session]; g.fadeIn = v[x.fadeIn]; g.fadeOut = v[x.fadeOut];
    g.gentle = i (x.gentle) != 0; g.fadeCC7 = i (x.fadeCC7) != 0; g.humanizeMs = v[x.humanize]; g.level = v[x.level];
    g.refPitch = v[x.refPitch]; g.tuneByBend = i (x.tuneBend) != 0;
    static const int BR[5] = { 2, 3, 12, 24, 48 }; g.bendRange = BR[juce::jlimit (0, 4, i (x.bendRange))];
    for (int l = 0; l < kLanes; ++l)
    {
        const auto& k = x.lane[(size_t) l];
        auto& L = lanes[(size_t) l];
        L.on = i (k.on) != 0; L.role = i (k.role); L.channel = i (k.channel); L.octave = i (k.octave); L.range = i (k.range);
        L.density = v[k.density]; L.length = v[k.length]; L.velocity = v[k.velocity]; L.velRand = v[k.velRand];
        L.probability = v[k.prob]; L.loop = i (k.loop) != 0; L.loopBeats = v[k.loopBeats]; L.mutate = v[k.mutate];
        L.rate = i (k.rate); L.euclidSteps = i (k.euSteps); L.euclidPulses = i (k.euPulses); L.euclidRotate = i (k.euRot);
        L.arpPattern = i (k.arp); L.echoes = i (k.echoes); L.echoBeats = v[k.echoTime]; L.echoDecay = v[k.echoDecay];
        L.follow = i (k.follow); L.voices = i (k.voices); L.ccNum = i (k.cc); L.ccShape = i (k.ccShape);
        L.ccBeats = v[k.ccBeats]; L.ccDepth = v[k.ccDepth]; L.ccCenter = v[k.ccCenter]; L.driftCents = v[k.drift];
    }
}

inline void applyParams (const float* v, Generator& gen) { applyParams (v, gen.g, gen.lanes); }

} // namespace dh::orr
