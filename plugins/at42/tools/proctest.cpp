// Pruebas del procesador completo, sin host (QA en DAWs 2026-10-22):
//   P1-P3  estado byte a byte (H4): una instancia nueva guardada dos veces, guardar → cargar → guardar (en la misma y en otra
//          instancia), y lo mismo después de cargar otro programa y tocar;
//   P4     una instancia nueva es el programa 0 (X2);
//   P5     Init suena con un acorde por MIDI (C1);
//   P10    una instancia que ya tocó y se prepara de nuevo suena igual que una nueva (programas 0 y 5);
//   P6-P8  hueco del host (X1): el host deja de llamar (bypass de REAPER) y el note-off se pierde; al volver la nota tiene
//          que soltarse. Con el transporte andando, parado (hueco de reloj) y con el arpegiador.
//   at42proctest            → 0 si todo pasa
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <thread>

namespace {

constexpr double kSr = 48000.0;
constexpr int kBlock = 512;
int fails = 0;

void report (const char* id, const char* what, bool ok, const juce::String& detail)
{
    std::printf ("  [%s] %s %-46s %s\n", ok ? "PASS" : "FAIL", id, what, detail.toRawUTF8());
    if (! ok) ++fails;
}

struct Head : juce::AudioPlayHead
{
    bool playing = false;
    juce::int64 samples = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setBpm (120.0);
        p.setTimeInSamples (samples);
        p.setTimeInSeconds ((double) samples / kSr);
        p.setPpqPosition ((double) samples / kSr * 2.0);
        return p;
    }
};

struct Rig
{
    std::unique_ptr<AT42Processor> p { std::make_unique<AT42Processor>() };
    Head head;
    Rig()
    {
        p->setPlayConfigDetails (0, 2, kSr, kBlock);
        p->setPlayHead (&head);
        p->prepareToPlay (kSr, kBlock);
    }
    // Procesa `secs` segundos; las notas se mandan al principio del primer bloque. Devuelve el RMS (dBFS) de los últimos `tail` s.
    float run (double secs, std::vector<juce::MidiMessage> msgs = {}, double tail = 0.5)
    {
        juce::AudioBuffer<float> buf (2, kBlock);
        const int blocks = (int) std::ceil (secs * kSr / kBlock), tailBlocks = std::max (1, (int) (tail * kSr / kBlock));
        double sum = 0.0; long count = 0;
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            juce::MidiBuffer midi;
            if (b == 0) for (auto& m : msgs) midi.addEvent (m, 0);
            p->processBlock (buf, midi);
            if (head.playing) head.samples += kBlock;
            if (b >= blocks - tailBlocks)
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < kBlock; ++i) { const double x = buf.getSample (c, i); sum += x * x; ++count; }
        }
        return (float) juce::Decibels::gainToDecibels (std::sqrt (sum / std::max (1L, count)), -240.0);
    }
};

juce::MemoryBlock save (AT42Processor& p) { juce::MemoryBlock m; p.getStateInformation (m); return m; }
juce::String firstDiff (const juce::MemoryBlock& a, const juce::MemoryBlock& b)
{
    if (a == b) return "identical, " + juce::String ((int) a.getSize()) + " bytes";
    const auto sa = a.toString(), sb = b.toString();
    int i = 0;
    while (i < sa.length() && i < sb.length() && sa[i] == sb[i]) ++i;
    return juce::String ((int) a.getSize()) + " vs " + juce::String ((int) b.getSize()) + " bytes, first difference at " + juce::String (i)
           + ": '" + sa.substring (std::max (0, i - 20), i + 30) + "' / '" + sb.substring (std::max (0, i - 20), i + 30) + "'";
}

std::vector<juce::MidiMessage> chordOn()  { return { juce::MidiMessage::noteOn (1, 60, 0.8f), juce::MidiMessage::noteOn (1, 64, 0.8f), juce::MidiMessage::noteOn (1, 67, 0.8f) }; }
std::vector<juce::MidiMessage> chordOff() { return { juce::MidiMessage::noteOff (1, 60), juce::MidiMessage::noteOff (1, 64), juce::MidiMessage::noteOff (1, 67) }; }

// Nota, hueco sin llamar a processBlock (el note-off se pierde ahí), y 4 s más. Control: el mismo recorrido con el note-off.
void gapTest (const char* id, const char* what, bool playing, std::function<void (AT42Processor&)> setup)
{
    float level = 0, withGap = 0, control = 0;
    for (int pass = 0; pass < 2; ++pass)
    {
        Rig r; if (setup) setup (*r.p);
        r.head.playing = playing;
        r.run (0.3);
        level = r.run (1.0, chordOn(), 0.3);
        if (pass == 0)
        {
            if (playing) r.head.samples += (juce::int64) (1.0 * kSr);          // el transporte siguió sin nosotros
            else std::this_thread::sleep_for (std::chrono::milliseconds (400));
            withGap = r.run (4.0, {}, 0.5);
        }
        else control = r.run (4.0, chordOff(), 0.5);
    }
    // Suelta como con el note-off (las colas del efector pueden durar más que la ventana): hasta 3 dB por encima del control.
    report (id, what, level > -60.0f && (withGap < -90.0f || withGap <= control + 3.0f),
            juce::String::formatted ("note %.1f dB; end %.1f dB with the note-off lost, %.1f dB with it", level, withGap, control));
}


// Renderiza `secs` segundos con eventos en segundos; devuelve el canal izquierdo.
std::vector<float> capture (Rig& r, double secs, const std::vector<std::pair<double, juce::MidiMessage>>& ev)
{
    juce::AudioBuffer<float> buf (2, kBlock);
    std::vector<float> out;
    const int blocks = (int) std::ceil (secs * kSr / kBlock);
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        juce::MidiBuffer m;
        for (auto& e : ev) { const int s = (int) (e.first * kSr); if (s >= b * kBlock && s < (b + 1) * kBlock) m.addEvent (e.second, s - b * kBlock); }
        r.p->processBlock (buf, m);
        if (r.head.playing) r.head.samples += kBlock;
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + kBlock);
    }
    return out;
}

// Una instancia que ya tocó y el host vuelve a preparar (lo que hace antes de un render) tiene que sonar igual que
// una nueva (QA en DAWs 2026-10-22: athosttest repro, +4 dB de diferencia).
void reprepareTest (const char* id, int program)
{
    const std::vector<std::pair<double, juce::MidiMessage>> ev { { 0.1, juce::MidiMessage::noteOn (1, 60, 0.8f) }, { 0.1, juce::MidiMessage::noteOn (1, 67, 0.8f) },
        { 1.2, juce::MidiMessage::noteOff (1, 60) }, { 1.2, juce::MidiMessage::noteOff (1, 67) }, { 1.5, juce::MidiMessage::noteOn (1, 64, 0.6f) }, { 2.2, juce::MidiMessage::noteOff (1, 64) } };
    Rig a, b;
    if (program > 0) { a.p->setCurrentProgram (program); b.p->setCurrentProgram (program); }
    b.run (2.0, { juce::MidiMessage::noteOn (1, 52, 1.0f), juce::MidiMessage::noteOn (1, 71, 0.5f) });
    b.run (1.3, { juce::MidiMessage::noteOff (1, 52), juce::MidiMessage::noteOff (1, 71) });
    b.p->releaseResources(); b.p->prepareToPlay (kSr, kBlock);
    a.p->releaseResources(); a.p->prepareToPlay (kSr, kBlock);
    const auto x = capture (a, 3.5, ev), y = capture (b, 3.5, ev);
    double d = 0, e = 0;
    for (size_t i = 0; i < x.size(); ++i) { d += (double) (x[i] - y[i]) * (x[i] - y[i]); e += (double) x[i] * x[i]; }
    const double nullDb = d == 0.0 ? -999.0 : 10.0 * std::log10 (d / (e + 1e-30));
    report (id, (juce::String ("re-prepared instance == fresh, program ") + juce::String (program)).toRawUTF8(), nullDb < -100.0,
            juce::String::formatted ("null %.1f dB (level %.1f dBFS)", nullDb, 10.0 * std::log10 (e / (double) std::max<size_t> (1, x.size()) + 1e-30)));
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("AT42 %s: processor tests\n", JucePlugin_VersionString);

    {
        Rig a;
        const auto s1 = save (*a.p), s2 = save (*a.p);
        report ("P1", "fresh instance saved twice", s1 == s2, firstDiff (s1, s2));
        a.p->setStateInformation (s1.getData(), (int) s1.getSize());
        const auto s3 = save (*a.p);
        report ("P2", "save -> load -> save (same instance)", s1 == s3, firstDiff (s1, s3));
        Rig b;
        b.p->setStateInformation (s1.getData(), (int) s1.getSize());
        const auto s4 = save (*b.p);
        report ("P2b", "save -> load -> save (new instance)", s1 == s4, firstDiff (s1, s4));
    }
    {
        Rig a;
        a.p->setCurrentProgram (5);
        a.run (0.5, chordOn());
        a.p->setValue (dh::P42::master, 0.5f);                        // modificado: el nombre lleva "*"
        const auto s1 = save (*a.p);
        Rig b;
        b.p->setStateInformation (s1.getData(), (int) s1.getSize());
        b.run (0.2);
        const auto s2 = save (*b.p);
        b.p->setStateInformation (s2.getData(), (int) s2.getSize());
        const auto s3 = save (*b.p);
        report ("P3", "program 5 modified: save -> load -> save x2", s1 == s2 && s2 == s3, firstDiff (s1, s2) + " / " + firstDiff (s2, s3));
    }
    {
        Rig fresh, p0;
        p0.p->setCurrentProgram (3);
        p0.p->setCurrentProgram (0);
        const auto a = save (*fresh.p), b = save (*p0.p);
        report ("P4", "fresh instance == program 0", a == b && fresh.p->getCurrentProgram() == 0,
                fresh.p->getProgramName (0) + ", " + firstDiff (a, b));
    }
    {
        Rig r;
        const float lvl = r.run (2.0, chordOn(), 1.5);
        report ("P5", "Init plays a MIDI chord (C3 E3 G3)", lvl > -40.0f, juce::String::formatted ("RMS %.1f dBFS", lvl));
    }
    gapTest ("P6", "host gap, transport running: note released", true, {});
    gapTest ("P7", "wall-clock gap, transport stopped: released", false, {});
    gapTest ("P8", "host gap with the arpeggiator (hold off)", true, [] (AT42Processor& p) { p.setExtValue ("kb_mode", 1.0f); });
    reprepareTest ("P10a", 0);
    reprepareTest ("P10b", 5);

    std::printf ("%s: %d FAIL\n", fails == 0 ? "OK" : "FAILED", fails);
    return fails == 0 ? 0 : 1;
}
