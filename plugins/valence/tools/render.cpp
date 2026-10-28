// Render offline de Valence, sin host: lo que "toca" la persona (un archivo MIDI) y un preset.
//   valencerender preset.json input.mid outdir [--secs 60] [--bpm 124] [--sr 48000] [--seed n] [--block 256] [--from-beat 0]
//   valencerender preset.json auto outdir ...        (sin entrada: la armonía automática toca sola)
//   valencerender --bench                            (CPU del motor solo: 4 capas a 1/32 con ratchets, 10 minutos)
// El tiempo es el de un DAW con el transporte andando desde el pulso 0 (o --from-beat) al tempo dado (--bpm, si no
// el del preset): la grilla de las capas cae en los pulsos de la canción y la entrada se lee en pulsos.
// Escribe en outdir:
//   all.mid          todas las capas (una pista por capa, en su canal: el 1 salvo que el preset use canales por capa)
//   layerN.mid       la capa N sola, en el canal 1 (para tocarla con el render de un sinte)
//   preview.wav      el sonido de prueba (24 bits)
//   events.json      cada nota con su capa, tiempo (s, muestras, pulsos), nota, velocidad, duración, nucleón de origen
//                    (protón o neutrón y la nota del acorde de la que sale), paso y flags; los CC, los acordes y las
//                    fusiones (ver README "valencerender").
#include <JuceHeader.h>
#include "ParamsVal.h"
#include "Preview.h"
#include <chrono>

using namespace dh::val;

namespace {

int bench()
{
    Engine e; e.g.hostClock = false; e.g.tempo = 174.0f; e.g.complete = CompNinth; e.g.perLayerChannels = true; e.g.key = 9; e.g.scale = 1;
    for (int l = 0; l < kLayers; ++l)
    {
        auto& L = e.layers[(size_t) l]; L.on = true; L.figure = 9 + (l % 3); L.length = 16 - 3 * l; L.pattern = l * 3; L.octaves = 3;
        L.mutate = 0.5f; L.humanizeMs = 8.0f; L.ccNum = 74; L.gate = 0.6f; L.channel = l + 1;
        for (int k = 0; k < kMaxSteps; ++k) { auto& s = L.steps[(size_t) k]; s.ratchet = (uint8_t) (1 + k % 2); s.cc = (int8_t) (k * 4); s.tie = k % 7 == 3; s.prob = 90; }
    }
    e.prepare (48000.0);
    std::vector<OutEvent> out; std::vector<InEvent> in;
    const int block = 256; const double secs = 600.0; const int64_t total = (int64_t) (secs * 48000.0);
    size_t events = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int64_t pos = 0; pos < total; pos += block)
    {
        in.clear();
        const int64_t period = 4 * 48000;          // un acorde de tres notas cada 4 s
        if (pos % period < block) { for (int n : { 45 + (int) (pos / period) % 7, 52, 57 }) in.push_back ({ (int) (pos % period), { 0x90, (uint8_t) n, 100 }, 3 }); }
        if ((pos + 4800) % period < block && pos > 0) { for (int n : { 45 + (int) ((pos + 4800) / period - 1) % 7, 52, 57 }) in.push_back ({ (int) ((pos + 4800) % period), { 0x80, (uint8_t) n, 0 }, 3 }); }
        e.process (block, {}, in.data(), (int) in.size(), out);
        events += out.size();
    }
    const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("motor solo, 4 capas a 1/32 con ratchets, CC, mutación y humanizado: %zu eventos en 10 minutos, CPU %.4f %% de un núcleo\n", events, 100.0 * el / secs);
    return 0;
}

juce::String jnum (double x, int dec = 6) { return juce::String (x, dec); }

} // namespace

int main (int argc, char** argv)
{
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    if (args.contains ("--bench")) return bench();
    if (args.size() < 3) { std::printf ("uso: valencerender preset.json input.mid|auto outdir [--secs 60] [--bpm X] [--sr 48000] [--seed n] [--block 256] [--from-beat 0]\n"); return 1; }
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto pf = cwd.getChildFile (args[0]);
    const auto pv = juce::JSON::parse (pf);
    if (! pv.isObject()) { std::printf ("no se pudo leer el preset %s\n", pf.getFullPathName().toRawUTF8()); return 1; }
    const bool autoMode = args[1] == "auto";
    const auto outDir = cwd.getChildFile (args[2]); outDir.createDirectory();
    const double sr = opt ("--sr", "48000").getDoubleValue(), secs = opt ("--secs", "60").getDoubleValue();
    const int block = juce::jlimit (16, 8192, opt ("--block", "256").getIntValue());
    const double fromBeat = opt ("--from-beat", "0").getDoubleValue();

    const auto& ps = params();
    std::vector<float> vals (ps.size());
    const auto* obj = pv.getProperty ("params", {}).getDynamicObject();
    for (size_t k = 0; k < ps.size(); ++k) vals[k] = obj != nullptr && obj->hasProperty (ps[k].id) ? (float) obj->getProperty (ps[k].id) : ps[k].def;
    if (args.contains ("--seed")) vals[(size_t) paramIndex ("seed")] = opt ("--seed", "1").getFloatValue();
    if (autoMode) vals[(size_t) paramIndex ("source")] = 1.0f;
    vals[(size_t) paramIndex ("clock")] = 1.0f;
    const double bpm = args.contains ("--bpm") ? opt ("--bpm", "120").getDoubleValue() : (double) vals[(size_t) paramIndex ("tempo")];

    Engine eng; eng.prepare (sr); applyParams (vals.data(), eng.g, eng.layers);
    eng.logChords = true;
    const auto rows = rowsFromVar (pv.getProperty ("rows", {}));
    for (int l = 0; l < kLayers; ++l) { eng.layers[(size_t) l].steps = rows[(size_t) l]; eng.layers[(size_t) l].rowsVersion = 1; }
    eng.g.run = autoMode || eng.g.source == SrcAuto;
    const auto voices = voicesOf (vals.data());

    // La entrada: notas, pedal y CC de todas las pistas, en pulsos → muestras con el mismo mapa que la grilla.
    struct In { int64_t at; uint8_t d[3]; };
    std::vector<In> input;
    const double spb = 60.0 / bpm * sr;
    auto sampleAtBeat = [&] (double beat) { return (int64_t) std::floor ((beat - fromBeat) * spb + 0.5 + 1.0e-9); };
    if (! autoMode)
    {
        juce::FileInputStream is (cwd.getChildFile (args[1]));
        juce::MidiFile mf;
        if (! is.openedOk() || ! mf.readFrom (is)) { std::printf ("no se pudo leer el MIDI de entrada %s\n", args[1].toRawUTF8()); return 1; }
        const double tpq = mf.getTimeFormat() > 0 ? (double) mf.getTimeFormat() : 480.0;
        for (int t = 0; t < mf.getNumTracks(); ++t)
            for (auto* m : *mf.getTrack (t))
            {
                const auto& msg = m->message;
                if (msg.isMetaEvent() || msg.isSysEx() || msg.getRawDataSize() > 3 || msg.getRawDataSize() < 2) continue;
                const int64_t at = sampleAtBeat (msg.getTimeStamp() / tpq);
                if (at < 0) continue;
                In x { at, { msg.getRawData()[0], msg.getRawData()[1], (uint8_t) (msg.getRawDataSize() > 2 ? msg.getRawData()[2] : 0) } };
                input.push_back (x);
            }
        std::stable_sort (input.begin(), input.end(), [] (const In& a, const In& b) { return a.at < b.at; });
    }

    Preview prev; prev.prepare ((float) sr, block);
    const float pvol = vals[(size_t) paramIndex ("preview_vol")], pspace = vals[(size_t) paramIndex ("preview_space")];
    const int64_t total = (int64_t) (secs * sr), tail = (int64_t) (2.5 * sr);
    juce::AudioBuffer<float> audio (2, (int) (total + tail + block)); audio.clear();
    struct Rec { int64_t at; OutEvent e; };
    std::vector<Rec> recs; std::vector<OutEvent> ev; std::vector<InEvent> bi;
    // Acordes (protones y neutrones, del registro del motor) y fusiones.
    struct ChordRec { int64_t at; double beat; std::vector<int> p, n; };
    std::vector<ChordRec> chords; std::vector<double> fusions; int64_t nextFusion = -1;
    size_t ii = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int64_t pos = 0; pos < total + tail; pos += block)
    {
        const bool playing = pos < total;
        Transport tr; tr.playing = playing; tr.bpm = bpm; tr.ppq = fromBeat + (double) pos / spb;
        bi.clear();
        while (ii < input.size() && input[ii].at < pos + block)
        {
            InEvent x; x.offset = (int) std::max<int64_t> (0, input[ii].at - pos); x.size = 3; std::memcpy (x.data, input[ii].d, 3);
            if (playing) bi.push_back (x);
            ++ii;
        }
        eng.process (block, tr, bi.data(), (int) bi.size(), ev);
        int done = 0;
        auto* L = audio.getWritePointer (0) + pos; auto* R = audio.getWritePointer (1) + pos;
        for (auto& e : ev)
        {
            recs.push_back ({ pos + e.offset, e });
            if (e.offset > done) { prev.render (L, R, done, e.offset, pvol, pspace); done = e.offset; }
            prev.handle (e, voices);
        }
        prev.render (L, R, done, block, pvol, pspace);
        const auto& v = eng.view;
        // Una fusión predicha que ya pasó es una fusión que sonó; después, la predicción siguiente.
        if (playing && v.running)
        {
            if (nextFusion >= 0 && v.beat * (double) kTPB + 0.5 >= (double) nextFusion) fusions.push_back ((double) nextFusion / (double) kTPB);
            nextFusion = v.fusionTick;
        }
        else nextFusion = -1;
    }
    const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();

    // Los acordes en la muestra en que cambiaron (antes del final del render).
    // Las notas de una mano que llegan en la misma muestra son un solo cambio: queda el último estado de esa muestra.
    for (auto& c : eng.chordLog)
    {
        if (c.at >= total) break;
        if (! chords.empty() && chords.back().at == c.at) chords.pop_back();
        if (chords.empty() ? ! c.protons.empty() : (chords.back().p != c.protons || chords.back().n != c.neutrons))
            chords.push_back ({ c.at, fromBeat + (double) c.at / spb, c.protons, c.neutrons });
    }

    // MIDI.
    auto writeMidi = [&] (const juce::File& f, int layerFilter, bool toCh1) {
        juce::MidiFile mf; mf.setTicksPerQuarterNote (960);
        juce::MidiMessageSequence tempo; tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0); mf.addTrack (tempo);
        int count = 0;
        for (int l = 0; l < kLayers; ++l)
        {
            if (layerFilter >= 0 && l != layerFilter) continue;
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Layer " + juce::String (l + 1)), 0.0);
            for (auto& r : recs)
            {
                if (r.e.layer != l) continue;
                auto m = juce::MidiMessage (r.e.data, r.e.size);
                if (! (m.isNoteOnOrOff() || m.isController())) continue;
                if (toCh1) m.setChannel (1);
                m.setTimeStamp ((double) r.at / spb * 960.0);
                seq.addEvent (m); ++count;
            }
            seq.updateMatchedPairs(); mf.addTrack (seq);
        }
        f.deleteFile(); juce::FileOutputStream os (f); mf.writeTo (os);
        return count;
    };
    std::printf ("%s: %.0f s a %.1f BPM, %d eventos MIDI, CPU %.2f %% de un núcleo (motor + sonido de prueba)\n",
                 pv.getProperty ("name", "?").toString().toRawUTF8(), secs, bpm, (int) recs.size(), 100.0 * cpu / (secs + 2.5));
    writeMidi (outDir.getChildFile ("all.mid"), -1, false);
    for (int l = 0; l < kLayers; ++l)
        if (eng.layers[(size_t) l].on)
            std::printf ("  capa %d (%s, %d pasos, %s): %d eventos\n", l + 1, figureName (eng.layers[(size_t) l].figure), eng.layers[(size_t) l].length,
                         patternName (eng.layers[(size_t) l].pattern), writeMidi (outDir.getChildFile ("layer" + juce::String (l + 1) + ".mid"), l, true));

    // Audio.
    {
        const auto wavF = outDir.getChildFile ("preview.wav"); wavF.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (wavF), sr, 2, 24, {}, 0));
        if (w != nullptr) w->writeFromAudioSampleBuffer (audio, 0, (int) (total + tail));
    }

    // JSON: notas con su duración (cada note-on con su note-off en el mismo canal), CC, acordes y fusiones.
    juce::String js;
    js << "{\n  \"preset\": " << juce::JSON::toString (pv.getProperty ("name", "")) << ",\n  \"category\": " << juce::JSON::toString (pv.getProperty ("category", ""))
       << ",\n  \"input\": " << juce::JSON::toString (autoMode ? juce::String ("auto") : args[1]) << ",\n  \"bpm\": " << jnum (bpm, 4) << ",\n  \"sampleRate\": " << (int) sr
       << ",\n  \"seconds\": " << jnum (secs, 3) << ",\n  \"fromBeat\": " << jnum (fromBeat, 4) << ",\n  \"ticksPerBeat\": " << (int) kTPB << ",\n  \"layers\": [\n";
    for (int l = 0; l < kLayers; ++l)
    {
        const auto& L = eng.layers[(size_t) l];
        js << "    { \"layer\": " << (l + 1) << ", \"on\": " << (L.on ? "true" : "false") << ", \"channel\": " << (eng.g.perLayerChannels ? L.channel : 1)
           << ", \"length\": " << L.length << ", \"rate\": \"" << figureName (L.figure) << "\", \"multiplier\": " << L.mult
           << ", \"stepBeats\": " << jnum ((double) Engine::stepTicks (L) / (double) kTPB, 6) << ", \"pattern\": \"" << patternName (L.pattern) << "\""
           << ", \"cycleBeats\": " << jnum ((double) Engine::stepTicks (L) * L.length / (double) kTPB, 6) << " }" << (l + 1 < kLayers ? "," : "") << "\n";
    }
    js << "  ],\n  \"notes\": [\n";
    std::map<int, size_t> open;          // canal·128 + nota → índice en notes
    struct Note { int64_t at, off; OutEvent e; };
    std::vector<Note> notes;
    std::vector<Rec> ccs;
    for (auto& r : recs)
    {
        const int st = r.e.data[0] & 0xF0, key = (r.e.data[0] & 15) * 128 + r.e.data[1];
        if (st == 0x90 && r.e.data[2] > 0) { if (open.count (key)) notes[open[key]].off = r.at; open[key] = notes.size(); notes.push_back ({ r.at, -1, r.e }); }
        else if (st == 0x80 || st == 0x90) { if (auto it = open.find (key); it != open.end()) { notes[it->second].off = r.at; open.erase (it); } }
        else if (st == 0xB0 && r.e.layer >= 0) ccs.push_back (r);
    }
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        const int64_t off = n.off >= 0 ? n.off : n.at;
        js << "    { \"layer\": " << (n.e.layer + 1) << ", \"t\": " << jnum ((double) n.at / sr) << ", \"sample\": " << n.at << ", \"beat\": " << jnum (fromBeat + (double) n.at / spb)
           << ", \"dur\": " << jnum ((double) (off - n.at) / sr) << ", \"ch\": " << ((n.e.data[0] & 15) + 1) << ", \"note\": " << (int) n.e.data[1] << ", \"vel\": " << (int) n.e.data[2]
           << ", \"nucleon\": \"" << (n.e.nucleon == NucProton ? "proton" : (n.e.nucleon == NucNeutron ? "neutron" : "none")) << "\", \"src\": " << (int) n.e.src
           << ", \"step\": " << (int) n.e.step << ", \"accent\": " << ((n.e.flags & FlagAccent) ? "true" : "false") << ", \"tie\": " << ((n.e.flags & FlagTie) ? "true" : "false")
           << ", \"ratchet\": " << ((n.e.flags & FlagRatchet) ? "true" : "false") << ", \"fixed\": " << ((n.e.flags & FlagFixed) ? "true" : "false") << " }"
           << (i + 1 < notes.size() ? "," : "") << "\n";
    }
    js << "  ],\n  \"cc\": [\n";
    for (size_t i = 0; i < ccs.size(); ++i)
        js << "    { \"layer\": " << (ccs[i].e.layer + 1) << ", \"t\": " << jnum ((double) ccs[i].at / sr) << ", \"sample\": " << ccs[i].at << ", \"cc\": " << (int) ccs[i].e.data[1]
           << ", \"value\": " << (int) ccs[i].e.data[2] << ", \"step\": " << (int) ccs[i].e.step << " }" << (i + 1 < ccs.size() ? "," : "") << "\n";
    js << "  ],\n  \"chords\": [\n";
    auto arr = [] (const std::vector<int>& v) { juce::StringArray s; for (int x : v) s.add (juce::String (x)); return "[" + s.joinIntoString (", ") + "]"; };
    for (size_t i = 0; i < chords.size(); ++i)
        js << "    { \"t\": " << jnum ((double) chords[i].at / sr) << ", \"sample\": " << chords[i].at << ", \"beat\": " << jnum (chords[i].beat)
           << ", \"protons\": " << arr (chords[i].p) << ", \"neutrons\": " << arr (chords[i].n) << " }" << (i + 1 < chords.size() ? "," : "") << "\n";
    js << "  ],\n  \"fusions\": [\n";
    for (size_t i = 0; i < fusions.size(); ++i)
        js << "    { \"beat\": " << jnum (fusions[i]) << ", \"t\": " << jnum ((fusions[i] - fromBeat) * 60.0 / bpm) << ", \"sample\": " << sampleAtBeat (fusions[i]) << " }" << (i + 1 < fusions.size() ? "," : "") << "\n";
    js << "  ]\n}\n";
    outDir.getChildFile ("events.json").replaceWithText (js);
    std::printf ("  %zu notas, %zu CC, %zu acordes, %zu fusiones en events.json\n", notes.size(), ccs.size(), chords.size(), fusions.size());
    return 0;
}
