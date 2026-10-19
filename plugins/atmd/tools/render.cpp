// Render offline del ATMD, sin host: un preset tocado desde un MIDI (o una frase de prueba), a WAV.
//   atmdrender preset.json out.wav [--midi archivo.mid] [--secs N] [--tail S] [--os 4] [--sr 48000] [--bpm 120]
// Imprime la CPU (porcentaje de un núcleo). Sin --midi toca una línea de bajo y un acorde.
#include <JuceHeader.h>
#include "MidiEvents.h"
#include "ParamsMd.h"

using namespace dh::md;

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uso: atmdrender preset.json out.wav [--midi f.mid] [--secs N] [--tail S] [--os N] [--sr N] [--bpm N]\n"); return 1; }
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const double sr = opt ("--sr", "48000").getDoubleValue();

    // Valores: los del preset sobre los por defecto de la tabla.
    const auto pv = juce::JSON::parse (cwd.getChildFile (args[0]));
    const auto* obj = pv.getProperty ("params", {}).getDynamicObject();
    std::vector<float> vals;
    for (auto& d : params()) vals.push_back (obj != nullptr && obj->hasProperty (d.id) ? (float) obj->getProperty (d.id) : d.def);
    const int os = args.contains ("--os") ? opt ("--os", "4").getIntValue() : oversamplingFactor (vals[(size_t) Ix::get().oversampling]);

    Engine eng;
    eng.prepare (sr, 512, os);
    applyParams (vals.data(), eng);
    eng.modWheel = juce::jlimit (0.0f, 1.0f, (float) pv.getProperty ("mod_wheel", 0.0));     // la rueda inicial del preset, como en el plugin
    eng.bpm = opt ("--bpm", "120").getDoubleValue();

    std::vector<dh::MidiEv> evs;
    double len = 0.0;
    if (const auto mp = opt ("--midi", ""); mp.isNotEmpty()) evs = dh::loadMidiFile (cwd.getChildFile (mp), sr, len);
    else
    {
        // Frase de prueba: bajo en corcheas y un acorde largo.
        const int line[] = { 36, 36, 48, 36, 39, 36, 43, 46 };
        double t = 0.0;
        for (int k = 0; k < 16; ++k)
        {
            evs.push_back ({ (int) (t * sr), juce::MidiMessage::noteOn (1, line[k % 8], (juce::uint8) 100) });
            evs.push_back ({ (int) ((t + 0.2) * sr), juce::MidiMessage::noteOff (1, line[k % 8]) });
            t += 0.25;
        }
        for (int nn : { 48, 55, 60 }) evs.push_back ({ (int) (t * sr), juce::MidiMessage::noteOn (1, nn, (juce::uint8) 90) });
        for (int nn : { 48, 55, 60 }) evs.push_back ({ (int) ((t + 2.0) * sr), juce::MidiMessage::noteOff (1, nn) });
        len = t + 2.0;
        std::stable_sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.sample < b.sample; });
    }
    const double secs = opt ("--secs", juce::String (len)).getDoubleValue() + opt ("--tail", "3").getDoubleValue();
    const int total = (int) (secs * sr);

    juce::AudioBuffer<float> out (2, total);
    out.clear();
    size_t me = 0;
    const int block = 256;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; )
    {
        while (me < evs.size() && evs[me].sample <= pos)
        {
            const auto& m = evs[me++].msg;
            if (m.isNoteOn()) eng.noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) eng.noteOff (m.getNoteNumber());
            else if (m.isPitchWheel()) eng.bend = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8191.0f);
            else if (m.isController() && m.getControllerNumber() == 1) eng.modWheel = (float) m.getControllerValue() / 127.0f;
            else if (m.isController() && m.getControllerNumber() == 64) eng.setSustain (m.getControllerValue() >= 64);
        }
        int n = std::min (block, total - pos);
        if (me < evs.size()) n = std::min (n, std::max (1, evs[me].sample - pos));
        eng.process (nullptr, out.getWritePointer (0, pos), out.getWritePointer (1, pos), n);
        pos += n;
    }
    const double cpu = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001 / secs * 100.0;

    auto f = cwd.getChildFile (args[1]); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os2 (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os2.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) { std::printf ("no se pudo escribir %s\n", f.getFullPathName().toRawUTF8()); return 1; }
    os2.release();
    w->writeFromAudioSampleBuffer (out, 0, total);
    std::printf ("%s: %.1f s, pico %.3f, CPU %.2f %% de un núcleo (%dx)\n", pv.getProperty ("name", "?").toString().toRawUTF8(), secs,
                 out.getMagnitude (0, total), cpu, os);
    return 0;
}
