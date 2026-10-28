// at303midirender: the complete AT303 processor in MIDI mode, played from a .mid (a Valence layer), to WAV. Built only
// for the Valence demos (extra.cmake); at303render plays the preset's own pattern and takes no MIDI.
//
//   at303midirender out.wav --midi layer.mid --preset "Squelch Line" [--tail 2] [--sr 48000]
//                    [--set id=value ...] [--cc 74=cutoff:20:85 ...]
//
// Overlapping notes slide (the engine's legato) and velocity >= the preset's accent velocity is an accent. --cc maps a
// controller of the file onto a knob, in the knob's own units (value 0 -> lo, 127 -> hi), at the sample of the event.
// The output is shifted by the processor's latency so the notes land on the grid of the file.
#include <JuceHeader.h>
#include "MidiEvents.h"
#include "PluginProcessor.h"

class HostClock : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying (false); return p; }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (juce::String::fromUTF8 (argv[i]));
    auto opt = [&] (const char* n, juce::String d) { const int i = args.indexOf (n); return i >= 0 && i + 1 < args.size() ? args[i + 1] : d; };
    if (args.size() < 3 || opt ("--midi", "").isEmpty())
    {
        std::printf ("usage: at303midirender out.wav --midi f.mid [--preset name] [--tail s] [--sr hz] [--set id=v ...] [--cc n=id:lo:hi ...]\n");
        return 1;
    }
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const double sr = opt ("--sr", "48000").getDoubleValue();
    double len = 0.0;
    const auto evs = dh::loadMidiFile (cwd.getChildFile (opt ("--midi", "")), sr, len);

    At303Processor p; HostClock clock;
    p.setPlayHead (&clock); p.setPlayConfigDetails (0, 2, sr, 512); p.prepareToPlay (sr, 512);
    if (const auto name = opt ("--preset", ""); name.isNotEmpty())
    {
        int idx = -1; for (int i = 0; i < (int) p.getPresets().size(); ++i) if (p.getPresets()[(size_t) i].name.equalsIgnoreCase (name)) idx = i;
        if (idx < 0) { std::printf ("unknown preset: %s\n", name.toRawUTF8()); return 3; }
        p.loadPreset (idx);
    }
    p.setValue ("sync", 0.0f); p.setValue ("run", 0.0f); p.setValue ("mode", 1.0f);
    for (int i = 0; i + 1 < args.size(); ++i)
        if (args[i] == "--set") p.setValue (args[i + 1].upToFirstOccurrenceOf ("=", false, false), args[i + 1].fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    struct CcMap { int cc; juce::String id; float lo, hi; };
    std::vector<CcMap> ccs;
    for (int i = 0; i + 1 < args.size(); ++i)
        if (args[i] == "--cc")
        {
            const auto s = args[i + 1]; const auto r = juce::StringArray::fromTokens (s.fromFirstOccurrenceOf ("=", false, false), ":", "");
            if (r.size() == 3) ccs.push_back ({ s.getIntValue(), r[0], r[1].getFloatValue(), r[2].getFloatValue() });
        }

    // A first block lets the parameter smoothing settle on the preset before the first note.
    { juce::AudioBuffer<float> warm (2, 512); juce::MidiBuffer none; for (int k = 0; k < 8; ++k) { warm.clear(); p.processBlock (warm, none); } }
    const int lat = p.getLatencySamples();
    const int total = (int) ((len + opt ("--tail", "2").getDoubleValue()) * sr) + lat;
    juce::AudioBuffer<float> buf (2, total); buf.clear();
    juce::MidiBuffer midi;
    size_t me = 0;
    for (int pos = 0; pos < total; )
    {
        // A segment ends at the block size or at the next controller, so a mapped knob moves at its own sample.
        int end = std::min (pos + 512, total);
        for (size_t k = me; k < evs.size() && evs[k].sample < end; ++k)
            if (evs[k].msg.isController() && evs[k].sample > pos) { end = evs[k].sample; break; }
        midi.clear();
        for (; me < evs.size() && evs[me].sample < end; ++me)
        {
            const auto& m = evs[me].msg;
            if (m.isController())
            {
                for (auto& c : ccs)
                    if (c.cc == m.getControllerNumber()) p.setValue (c.id, c.lo + (c.hi - c.lo) * (float) m.getControllerValue() / 127.0f);
            }
            else midi.addEvent (m, std::max (0, evs[me].sample - pos));
        }
        juce::AudioBuffer<float> blk (buf.getArrayOfWritePointers(), 2, pos, end - pos);
        p.processBlock (blk, midi);
        pos = end;
    }
    // Remove the latency (the decimator delay) so the audio lines up with the file.
    juce::AudioBuffer<float> outb (2, total - lat);
    for (int c = 0; c < 2; ++c) outb.copyFrom (c, 0, buf, c, lat, total - lat);
    juce::File out = cwd.getChildFile (args[0]); out.deleteFile();
    std::unique_ptr<juce::OutputStream> os (out.createOutputStream());
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (! w || ! w->writeFromAudioSampleBuffer (outb, 0, outb.getNumSamples())) { std::printf ("cannot write\n"); return 4; }
    std::printf ("%s: %.1f s, peak %.3f, latency %d\n", out.getFileName().toRawUTF8(), outb.getNumSamples() / sr, outb.getMagnitude (0, outb.getNumSamples()), lat);
    return 0;
}
