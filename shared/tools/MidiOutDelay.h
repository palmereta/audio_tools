#pragma once
// El MIDI que sale de un plugin con latencia tiene que salir atrasado esa misma latencia: el host compensa el audio
// (lo adelanta) pero no el MIDI de salida, y una pista que dobla por MIDI From quedaba adelantada 1-2 ms (QA en DAWs,
// 2026-10-22, P1: Pallor 78 muestras, Thrum 55). Una cola con tiempo absoluto en muestras: lo que entra en este
// bloque sale `delay` muestras más tarde, en este bloque o en los siguientes. Sin reservas en el hilo de audio
// mientras la cola no pase de la capacidad de prepare().
// Uso, al final de processBlock: midiOutDelay.process (midiOut, n);  y en prepareToPlay: setDelay (latencia).

#include <JuceHeader.h>
#include <vector>

namespace dh {

class MidiOutDelay
{
public:
    void prepare (int capacity = 4096) { queue.clear(); queue.reserve ((size_t) capacity); now = 0; }
    void setDelay (int samples) { delay = std::max (0, samples); }
    int getDelay() const { return delay; }
    void reset() { queue.clear(); now = 0; }

    // Reemplaza el contenido de `midi` (eventos de este bloque, con posición dentro del bloque) por lo que toca
    // sacar en este bloque. Con delay 0 no toca nada.
    void process (juce::MidiBuffer& midi, int numSamples)
    {
        if (delay == 0 && queue.empty()) { now += numSamples; return; }
        for (const auto meta : midi)
            if (queue.size() < queue.capacity())
                queue.push_back ({ now + meta.samplePosition + delay, meta.getMessage() });
        midi.clear();

        const juce::int64 end = now + numSamples;
        size_t keep = 0;
        for (size_t i = 0; i < queue.size(); ++i)
        {
            if (queue[i].when < end) midi.addEvent (queue[i].msg, (int) std::max<juce::int64> (0, queue[i].when - now));
            else queue[keep++] = queue[i];
        }
        queue.resize (keep);
        now = end;
    }

    // Al parar o en bypass: lo pendiente se descarta salvo los note-off, que salen ya para no dejar notas colgadas.
    void flushNoteOffs (juce::MidiBuffer& midi)
    {
        for (auto& e : queue) if (e.msg.isNoteOff()) midi.addEvent (e.msg, 0);
        queue.clear();
    }

private:
    struct Ev { juce::int64 when; juce::MidiMessage msg; };
    std::vector<Ev> queue;
    juce::int64 now = 0;
    int delay = 0;
};

} // namespace dh
