#pragma once
// Motor del AT303: la voz sobremuestreada (×4 por defecto, con el diezmador compartido), el secuenciador con el
// reloj de 24 ppq (del host o interno), el modo MIDI (acento por velocidad, slide por legato) y una salida con
// distorsión y delay opcionales. Sin JUCE: lo prueba tools/at303test.cpp.

#include "Voice303.h"
#include "Seq303.h"
#include "Oversampling.h"

namespace dh::tb {

struct EngineParams
{
    VoiceParams voice;
    int oversample = 4;
    bool seqMode = true;                  // true: secuenciador; false: se toca por MIDI
    int transpose = 0;
    int accentVelocity = 100;             // MIDI: velocidad desde la que es acento
    float volume = 0.8f;
    // Salida
    int driveType = 0;                    // 0 apagado, 1 suave (op-amp con diodos), 2 recorte duro, 3 fuzz asimétrico
    float drive = 0.4f, driveTone = 0.6f, driveMix = 1.0f;
    float delayMix = 0.0f, delayFeedback = 0.35f, delayBeats = 0.75f, delayTone = 0.5f;   // 0.75 = corchea con puntillo
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        fs = (float) sampleRate; maxB = std::max (16, maxBlock);
        setOversample (P.oversample);
        delayL.assign ((size_t) (4.0 * fs) + 8, 0.0f); delayR = delayL;
        reset();
    }
    void reset()
    {
        voice.reset(); seq.restart(); dw = 0; dlpL = dlpR = 0; dcL = dcR = 0; heldN = 0; wasRunning = false; driveLp = 0; dcIn2 = dcL2 = 0;
        std::fill (delayL.begin(), delayL.end(), 0.0f); std::fill (delayR.begin(), delayR.end(), 0.0f);
    }
    void set (const EngineParams& p)
    {
        if (p.oversample != P.oversample) setOversample (p.oversample);
        P = p; voice.set (p.voice); seq.setTranspose (p.transpose);
    }
    void setPattern (const Pattern& pt) { seq.setPattern (pt); }

    // MIDI: en modo MIDI siempre; en modo secuenciador, mientras el secuenciador está parado (las teclas del
    // instrumento). Las notas sostenidas dan legato con slide.
    void midiNoteOn (int note, int vel)
    {
        if (P.seqMode && wasRunning) return;
        // La misma tecla dos veces no se duplica: un solo note-off la suelta, como en un sinte monofónico.
        for (int i = 0; i < heldN; ++i) if (held[(size_t) i] == note) { for (int j = i; j < heldN - 1; ++j) held[(size_t) j] = held[(size_t) j + 1]; --heldN; break; }
        const bool legato = heldN > 0;
        if (heldN < 16) held[(size_t) heldN++] = note;
        voice.setSlide (legato);
        voice.noteOn (2.0f + (float) (note - 36 + P.transpose) / 12.0f, vel >= P.accentVelocity, legato);
    }
    void midiNoteOff (int note)
    {
        // En cualquier modo: una nota que entró antes de cambiar de modo también tiene que soltarse.
        bool found = false;
        for (int i = 0; i < heldN; ++i) if (held[(size_t) i] == note) { for (int j = i; j < heldN - 1; ++j) held[(size_t) j] = held[(size_t) j + 1]; --heldN; found = true; break; }
        if (! found) return;
        if (heldN == 0) voice.gateOff();
        else { voice.setSlide (true); voice.noteOn (2.0f + (float) (held[(size_t) heldN - 1] - 36 + P.transpose) / 12.0f, false, true); }
    }
    void allNotesOff() { heldN = 0; voice.gateOff(); }

    // Procesa `n` muestras. `running`: el secuenciador corre; `clock0`: posición del reloj (pulsos de 24 ppq) en
    // la primera muestra; `clocksPerSample`: el avance del reloj por muestra (tempo).
    void process (float* l, float* r, int n, bool running, double clock0, double clocksPerSample)
    {
        if (running && ! wasRunning) { seq.restart(); if (P.seqMode) heldN = 0; }
        if (! running && wasRunning && P.seqMode) voice.gateOff();
        wasRunning = running;
        for (int i0 = 0; i0 < n; i0 += kChunk)
        {
            const int m = std::min (kChunk, n - i0);
            for (int i = 0; i < m; ++i)
            {
                if (running && P.seqMode)
                {
                    const double c0 = clock0 + (double) (i0 + i) * clocksPerSample, c1 = c0 + clocksPerSample;
                    seq.advance (c0, c1, [&] (const SeqEvent& e, double) {
                        if (e.kind == SeqEvent::kSlide) voice.setSlide (e.slide);
                        else if (e.kind == SeqEvent::kNoteOn) voice.noteOn (e.cv, e.accent, e.legato);
                        else voice.gateOff();
                    });
                }
                voice.processBase (osBuf.data() + (size_t) (i * os));
            }
            std::fill (dec.begin(), dec.begin() + m, 0.0f);
            decim.process (osBuf.data(), dec.data(), m, 1.0f);
            for (int i = 0; i < m; ++i)
            {
                float x = dec[(size_t) i];
                // Pasa altos de salida (C20/C21): 47 Hz, o 4,7 Hz con la corrección de graves.
                const float hpA = std::exp (-2.0f * kPi * (P.voice.bassFix ? 4.7f : 47.0f) / fs);
                const float y = x - dcIn + hpA * dcL; dcIn = x; dcL = y; x = y;
                x = driveStage (x);
                // El recorte de una onda asimétrica corre el promedio: otro pasa altos (5 Hz) después de la distorsión.
                if (P.driveType > 0) { const float a5 = std::exp (-2.0f * kPi * 5.0f / fs); const float y2 = x - dcIn2 + a5 * dcL2; dcIn2 = x; dcL2 = y2; x = y2; }
                float L = x, R = x;
                delayStage (L, R, clocksPerSample);
                // +9 dB para que el patrón por defecto quede cerca de −18 dBFS RMS, y un techo suave en 0 dBFS.
                l[i0 + i] = ceiling (L * P.volume * kMakeup); r[i0 + i] = ceiling (R * P.volume * kMakeup);
            }
        }
    }

    const Voice& voiceView() const { return voice; }
    int currentStep() const { return seq.currentStep(); }

private:
    static constexpr int kChunk = 64;
    EngineParams P;
    Voice voice; Sequencer seq;
    dh::Decimator decim;
    std::vector<float> osBuf, dec;
    float fs = 48000; int maxB = 512, os = 4;
    std::vector<float> delayL, delayR; int dw = 0; float dlpL = 0, dlpR = 0;
    float dcL = 0, dcR = 0, dcIn = 0, driveLp = 0, dcIn2 = 0, dcL2 = 0;
    std::array<int, 16> held {}; int heldN = 0; bool wasRunning = false;

    void setOversample (int f)
    {
        os = f >= 8 ? 8 : (f >= 4 ? 4 : (f >= 2 ? 2 : 1));
        decim.prepare (os);
        osBuf.assign ((size_t) (kChunk * os + 8), 0.0f); dec.assign ((size_t) kChunk + 8, 0.0f);
        voice.prepare (fs, os);
    }

    // Distorsión: tres circuitos genéricos (sin marcas), con un tono antes de la mezcla.
    static constexpr float kMakeup = 2.8f;
    static inline float ceiling (float x)
    {
        const float m = std::fabs (x);
        return m < 0.9f ? x : std::copysign (0.9f + 0.1f * std::tanh ((m - 0.9f) * 10.0f), x);
    }
    inline float driveStage (float x)
    {
        if (P.driveType <= 0) return x;
        const float g = 1.0f + 30.0f * P.drive * P.drive;
        float y;
        const float v = x * g;
        if (P.driveType == 1) y = v / (1.0f + std::fabs (v)) * 1.4f;                      // diodos en la realimentación
        else if (P.driveType == 2) y = clampf (v, -0.7f, 0.7f) / 0.7f;                      // recorte duro
        else y = std::tanh (v + 0.35f * v * v * (v > 0 ? 1.0f : 0.2f)) - std::tanh (0.0f); // asimétrico
        const float a = std::exp (-2.0f * kPi * (800.0f + 9000.0f * P.driveTone) / fs);
        driveLp = y + a * (driveLp - y);
        y = driveLp * 0.7f / std::sqrt (g * 0.25f + 0.75f);
        return x + (y - x) * P.driveMix;
    }

    // Delay sincronizado (en negras del tempo) con un pasa bajos en la realimentación, cruzado entre canales.
    inline void delayStage (float& L, float& R, double clocksPerSample)
    {
        if (P.delayMix <= 0.0f) return;
        const double samplesPerBeat = clocksPerSample > 0 ? 24.0 / clocksPerSample : fs * 0.5;
        const int N = (int) delayL.size();
        const int d = std::clamp ((int) (P.delayBeats * samplesPerBeat), 1, N - 2);
        const int rd = (dw - d + N) % N;
        const float a = std::exp (-2.0f * kPi * (1200.0f + 8000.0f * P.delayTone) / fs);
        dlpL = delayR[(size_t) rd] + a * (dlpL - delayR[(size_t) rd]);
        dlpR = delayL[(size_t) rd] + a * (dlpR - delayL[(size_t) rd]);
        delayL[(size_t) dw] = L + P.delayFeedback * dlpL;
        delayR[(size_t) dw] = R * 0.0f + P.delayFeedback * dlpR;
        dw = (dw + 1) % N;
        L += P.delayMix * dlpL; R += P.delayMix * dlpR;
    }
};

} // namespace dh::tb
