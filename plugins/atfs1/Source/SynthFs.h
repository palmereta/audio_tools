#pragma once
// El sintetizador completo sin la parte de plugin: motor + efectos + controladores MIDI. Lo usan el
// procesador y atfs1render (render offline de presets y muestras), así suenan igual.

#include <JuceHeader.h>
#include "ParamsFs.h"

namespace dh::fs {

class Synth
{
public:
    Engine engine;
    Effects fx;
    PartParams part;
    FxParams fxp;
    float outputGain = 1.0f;

    void prepare (double sampleRate, int maxBlock)
    {
        fs = (float) sampleRate;
        block = std::max (64, maxBlock);
        engine.prepare (fs);
        fx.prepare (fs, kChunk);
        L.assign ((size_t) kChunk, 0.0f); R.assign ((size_t) kChunk, 0.0f);
    }

    template <typename Get> void setParams (Get&& get) { applyHost (get, part, fxp, outputGain); }

    // Controladores del FS1R: CC 1 rueda, 2 (BC), 4 (FC); 80/81 perillas FORMANT/FM del sistema por defecto;
    // 71 resonancia, 72 release, 73 attack, 74 brillo (corte); 64 sustain; 5/65 portamento.
    // ccToParam devuelve el id del parámetro del host que el CC mueve (o nullptr si lo maneja el motor).
    static const char* ccToParam (int cc)
    {
        switch (cc)
        {
            case 71: return "resonance"; case 72: return "release"; case 73: return "attack"; case 74: return "cutoff";
            case 80: return "formant"; case 81: return "fm"; case 5: return "porta_time"; case 65: return "porta";
            case 91: return "rev_send"; case 93: return "var_send"; case 7: return "volume";
            default: return nullptr;
        }
    }

    void handleMidi (const juce::MidiMessage& m, const VoiceData& v)
    {
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getVelocity(), v, part);
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber(), part);
        else if (m.isPitchWheel()) engine.setBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isChannelPressure()) engine.setAftertouch ((float) m.getChannelPressureValue() / 127.0f);
        else if (m.isAftertouch()) engine.setAftertouch ((float) m.getAfterTouchValue() / 127.0f);
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), val = m.getControllerValue();
            if (cc == 1) engine.setModWheel ((float) val / 127.0f);
            else if (cc == 64) engine.setSustain (val >= 64);
            else if (cc == 120 || cc == 123) engine.allNotesOff();
            else if (cc == 121) engine.resetControllers();
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }

    void render (float* outL, float* outR, int n, const VoiceData& v, const FseqData* f)
    {
        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int m = std::min (kChunk, n - pos);
            engine.process (L.data(), R.data(), m, v, f, part);
            fx.process (L.data(), R.data(), m, fxp);
            for (int i = 0; i < m; ++i) { outL[pos + i] = L[(size_t) i] * outputGain; outR[pos + i] = R[(size_t) i] * outputGain; }
        }
    }

private:
    static constexpr int kChunk = 256;
    float fs = 48000.0f;
    int block = 512;
    std::vector<float> L, R;
};

} // namespace dh::fs
