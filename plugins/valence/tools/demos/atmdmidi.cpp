// atmdmidirender: the ATMD engine played from a .mid (a Valence layer), to WAV, with controllers of the file mapped
// onto knobs. Built only for the Valence demos (extra.cmake); atmdrender reads only CC 1 and 64.
//
//   atmdmidirender preset.json out.wav --midi layer.mid [--tail 3] [--sr 48000] [--set id=value ...]
//                  [--cc 74=filter_cutoff:-2:3 ...]
//
// --cc maps a controller onto a parameter in its own units (value 0 -> lo, 127 -> hi) from the sample of the event,
// glided over about 3 ms: a step of the cutoff with emphasis up clicks when it lands in one sample.
#include <JuceHeader.h>
#include "MidiEvents.h"
#include "ParamsMd.h"

using namespace dh::md;

int main (int argc, char** argv)
{
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };
    if (args.size() < 4 || opt ("--midi", "").isEmpty())
    {
        std::printf ("usage: atmdmidirender preset.json out.wav --midi f.mid [--tail s] [--sr hz] [--set id=v ...] [--cc n=id:lo:hi ...]\n");
        return 1;
    }
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const double sr = opt ("--sr", "48000").getDoubleValue();

    const auto pv = juce::JSON::parse (cwd.getChildFile (args[0]));
    const auto* obj = pv.getProperty ("params", {}).getDynamicObject();
    std::vector<float> vals;
    for (auto& d : params()) vals.push_back (obj != nullptr && obj->hasProperty (d.id) ? (float) obj->getProperty (d.id) : d.def);
    for (int i = 0; i + 1 < args.size(); ++i)
        if (args[i] == "--set")
        {
            const int ix = paramIndex (args[i + 1].upToFirstOccurrenceOf ("=", false, false));
            if (ix < 0) { std::printf ("unknown parameter: %s\n", args[i + 1].toRawUTF8()); return 2; }
            vals[(size_t) ix] = args[i + 1].fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        }
    struct CcMap { int cc, ix; float lo, hi, cur = 0, target = 0; bool seen = false; };
    std::vector<CcMap> ccs;
    for (int i = 0; i + 1 < args.size(); ++i)
        if (args[i] == "--cc")
        {
            const auto s = args[i + 1]; const auto r = juce::StringArray::fromTokens (s.fromFirstOccurrenceOf ("=", false, false), ":", "");
            const int ix = r.size() == 3 ? paramIndex (r[0]) : -1;
            if (ix < 0) { std::printf ("bad --cc %s\n", s.toRawUTF8()); return 2; }
            ccs.push_back ({ s.getIntValue(), ix, r[1].getFloatValue(), r[2].getFloatValue() });
        }
    const int os = oversamplingFactor (vals[(size_t) Ix::get().oversampling]);

    Engine eng;
    eng.prepare (sr, 512, os);
    applyParams (vals.data(), eng);
    eng.modWheel = juce::jlimit (0.0f, 1.0f, (float) pv.getProperty ("mod_wheel", 0.0));
    eng.bpm = opt ("--bpm", "120").getDoubleValue();

    double len = 0.0;
    const auto evs = dh::loadMidiFile (cwd.getChildFile (opt ("--midi", "")), sr, len);
    const int total = (int) ((len + opt ("--tail", "3").getDoubleValue()) * sr);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    size_t me = 0;
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
            else if (m.isController())
                for (auto& c : ccs)
                    if (c.cc == m.getControllerNumber())
                    {
                        c.target = c.lo + (c.hi - c.lo) * (float) m.getControllerValue() / 127.0f;
                        if (! c.seen) { c.seen = true; c.cur = c.target; vals[(size_t) c.ix] = c.cur; applyParams (vals.data(), eng); }
                    }
        }
        int n = std::min (256, total - pos);
        if (me < evs.size()) n = std::min (n, std::max (1, evs[me].sample - pos));
        bool moving = false;
        for (auto& c : ccs) moving |= c.seen && c.cur != c.target;
        if (moving)
        {
            n = std::min (n, 16);
            const float k = 1.0f - std::exp (-(float) n / (0.003f * (float) sr));
            for (auto& c : ccs)
                if (c.seen && c.cur != c.target)
                {
                    c.cur += (c.target - c.cur) * k;
                    if (std::abs (c.target - c.cur) < 1e-4f * (std::abs (c.hi - c.lo) + 1e-6f)) c.cur = c.target;
                    vals[(size_t) c.ix] = c.cur;
                }
            applyParams (vals.data(), eng);
        }
        eng.process (nullptr, out.getWritePointer (0, pos), out.getWritePointer (1, pos), n);
        pos += n;
    }
    auto f = cwd.getChildFile (args[1]); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os2 (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os2.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) { std::printf ("cannot write %s\n", f.getFullPathName().toRawUTF8()); return 1; }
    os2.release();
    w->writeFromAudioSampleBuffer (out, 0, total);
    std::printf ("%s: %.1f s, peak %.3f\n", pv.getProperty ("name", "?").toString().toRawUTF8(), total / sr, out.getMagnitude (0, total));
    return 0;
}
