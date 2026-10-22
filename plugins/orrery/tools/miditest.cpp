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

    // Los carriles como puertos virtuales (1.0.5): cada carril sale por su puerto ("Orrery Drone"...). Se escucha
    // cada uno y se compara con lo que el procesador manda al host por el canal de ese carril.
    struct PerPort : juce::MidiInputCallback
    {
        std::atomic<int> notes { 0 };
        void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override { if (m.isNoteOn()) ++notes; }
    };
    OrreryProcessor q;
    q.setPlayConfigDetails (0, 2, 48000.0, 512);
    q.prepareToPlay (48000.0, 512);
    if (q.getNumPrograms() > 1) q.loadPreset (0);
    q.setValue ("role", 1.0f); q.setValue ("midi_ports", 1.0f); q.setValue ("run", 1.0f); q.setValue ("clock", 0.0f); q.setValue ("fade_in", 0.0f);
    q.housekeeping();
    const auto names = q.direct.virtualPortNames();
    std::array<PerPort, dh::orr::kLanes> per;
    std::vector<std::unique_ptr<juce::MidiInput>> ins;
    for (int tries = 0; tries < 30; ++tries)
    {
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.1, false);
        int found = 0; for (auto& d : juce::MidiInput::getAvailableDevices()) found += names.contains (d.name);
        if (found >= dh::orr::kLanes) break;
    }
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (const int l = names.indexOf (d.name); l >= 0)
            if (auto mi = juce::MidiInput::openDevice (d.identifier, &per[(size_t) l])) { mi->start(); ins.push_back (std::move (mi)); }
    std::printf ("puertos publicados: %s (%d abiertos)\n", names.joinIntoString (", ").toRawUTF8(), (int) ins.size());
    std::array<int, 16> hostPerCh {};
    const int blocks2 = (int) (20.0 * 48000.0 / 512);
    const double t1 = juce::Time::getMillisecondCounterHiRes();
    for (int b = 0; b < blocks2; ++b)
    {
        q.processBlock (buf, mb);
        for (const auto meta : mb) if (meta.getMessage().isNoteOn()) hostPerCh[(size_t) meta.getMessage().getChannel() - 1]++;
        const double target = t1 + (b + 1) * 512 * 1000.0 / 48000.0;
        while (juce::Time::getMillisecondCounterHiRes() < target) juce::Thread::sleep (1);
    }
    juce::Thread::sleep (500);
    bool portsOk = (int) ins.size() == dh::orr::kLanes;
    int totalPort = 0, totalHost = 0;
    for (int l = 0; l < dh::orr::kLanes; ++l)
    {
        const int ch = (int) q.value ("l" + juce::String (l + 1) + "_channel");
        const int h = hostPerCh[(size_t) juce::jlimit (1, 16, ch) - 1], got = per[(size_t) l].notes.load();
        std::printf ("  carril %d (%s): host %d, puerto %d\n", l + 1, names[l].toRawUTF8(), h, got);
        portsOk = portsOk && got == h; totalPort += got; totalHost += h;
    }
    portsOk = portsOk && totalHost > 10;
    std::printf ("puertos virtuales: %d de %d notas: %s\n", totalPort, totalHost, portsOk ? "OK" : "FALLA");
    for (auto& mi : ins) mi->stop();
    return ok && portsOk ? 0 : 1;
}
