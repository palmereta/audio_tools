// Prueba de las salidas MIDI directas (macOS): crea un puerto virtual, conecta un carril de Orrery a él,
// genera 30 s de MIDI con el procesador real y verifica que lleguen todos los mensajes y a tiempo.
#include <CoreFoundation/CoreFoundation.h>   // antes de JUCE (su Point choca con el de MacTypes)
#include <JuceHeader.h>
#include "PluginProcessor.h"

struct Catcher : juce::MidiInputCallback
{
    std::atomic<int> notes { 0 }, clocks { 0 }, other { 0 };
    std::vector<double> lateness;
    juce::CriticalSection lock;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        if (m.isMidiClock()) ++clocks;
        else if (m.isNoteOn()) ++notes;
        else ++other;
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    Catcher catcher;
    auto in = juce::MidiInput::createNewDevice ("Orrery Test Port", &catcher);
    if (in == nullptr) { std::printf ("no se pudo crear el puerto virtual\n"); return 1; }
    in->start();
    juce::String id;
    for (int tries = 0; tries < 30 && id.isEmpty(); ++tries)
    {
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.1, false);
        for (auto& d : juce::MidiOutput::getAvailableDevices()) if (d.name.contains ("Orrery Test Port")) id = d.identifier;
    }
    if (id.isEmpty()) for (auto& d : juce::MidiOutput::getAvailableDevices()) std::printf ("  salida disponible: %s\n", d.name.toRawUTF8());
    if (id.isEmpty()) { std::printf ("el puerto no aparece como salida\n"); return 1; }
    OrreryProcessor p;
    p.setPlayConfigDetails (0, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    if (p.getNumPrograms() > 1) p.loadPreset (0);
    for (int l = 0; l < dh::orr::kLanes; ++l) p.direct.setLanePort (l, id);
    p.setValue ("clock_out", 1.0f); p.setValue ("run", 1.0f); p.setValue ("clock", 0.0f);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer mb;
    int expected = 0;
    const int blocks = (int) (30.0 * 48000.0 / 512);
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (int b = 0; b < blocks; ++b)
    {
        p.processBlock (buf, mb);
        for (const auto meta : mb) if (meta.getMessage().isNoteOn()) ++expected;
        // Tiempo real: el hilo de salida manda cada mensaje a su hora.
        const double target = t0 + (b + 1) * 512 * 1000.0 / 48000.0;
        while (juce::Time::getMillisecondCounterHiRes() < target) juce::Thread::sleep (1);
    }
    juce::Thread::sleep (500);
    const int clocks = catcher.clocks.load();
    std::printf ("notas generadas %d, recibidas por el puerto %d; reloj recibido %d (esperado ~%d)\n", expected, catcher.notes.load(), clocks, (int) (30.0 * 62 / 60 * 24));
    const bool ok = expected > 20 && catcher.notes.load() == expected && clocks > 600;
    std::printf ("%s\n", ok ? "OK" : "FALLA");
    in->stop();
    return ok ? 0 : 1;
}
