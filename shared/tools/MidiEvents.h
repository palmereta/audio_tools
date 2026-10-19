#pragma once
// Lectura de un .mid para los renderizadores offline: todos los eventos de todas las pistas, con la
// posición en muestras (respetando los cambios de tempo del archivo).

#include <JuceHeader.h>
#include <vector>

namespace dh {

struct MidiEv { int sample; juce::MidiMessage msg; };

inline std::vector<MidiEv> loadMidiFile (const juce::File& file, double sampleRate, double& lengthSeconds)
{
    std::vector<MidiEv> out;
    lengthSeconds = 0.0;
    juce::FileInputStream in (file);
    juce::MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) { std::printf ("no se pudo leer %s\n", file.getFullPathName().toRawUTF8()); return out; }
    mf.convertTimestampTicksToSeconds();
    for (int t = 0; t < mf.getNumTracks(); ++t)
        for (auto* e : *mf.getTrack (t))
        {
            const auto& m = e->message;
            if (! (m.isNoteOnOrOff() || m.isPitchWheel() || m.isController())) continue;
            out.push_back ({ (int) std::lround (m.getTimeStamp() * sampleRate), m });
            lengthSeconds = std::max (lengthSeconds, m.getTimeStamp());
        }
    std::stable_sort (out.begin(), out.end(), [] (const MidiEv& a, const MidiEv& b) { return a.sample < b.sample; });
    return out;
}

} // namespace dh
