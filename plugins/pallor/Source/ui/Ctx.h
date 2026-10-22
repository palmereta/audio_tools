#pragma once
// Lo que comparten el panel y las páginas de la pantalla: la voz elegida, el patrón que se edita, el mensaje del
// display y los niveles de cada voz (leídos una vez por cuadro del procesador).

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace dh::pal::ui {

struct Ctx
{
    explicit Ctx (PallorProcessor& p) : proc (p) {}
    PallorProcessor& proc;
    int voice = 0, group = 0, var = 0;
    bool follow = false;                                   // la grilla sigue al patrón que suena
    std::array<float, kNumSlots> level {};                 // 0..1 con caída lenta (medidores y pads)
    std::array<float, kNumSlots> flash {};                 // destello de cada golpe (pads)
    std::vector<std::function<void()>> onVoice, onTarget, onPatterns, onPreset;

    void selectVoice (int v)
    {
        v = juce::jlimit (0, kNumSlots - 1, v);
        if (v == voice) return;
        voice = v; for (auto& f : onVoice) f();
    }
    void setTarget (int g, int va)
    {
        g = juce::jlimit (0, kGroups - 1, g); va = juce::jlimit (0, 2, va);
        if (g == group && va == var) return;
        group = g; var = va; for (auto& f : onTarget) f();
    }
    int editPattern() const { return patIndex (group, var); }
    static juce::String patternName (int idx) { return juce::String (idx / 3 + 1) + (idx % 3 == 0 ? "A" : (idx % 3 == 1 ? "B" : " FILL")); }

    juce::String message; juce::uint32 messageUntil = 0;
    void say (const juce::String& m, int ms = 1600) { message = m; messageUntil = juce::Time::getMillisecondCounter() + (juce::uint32) ms; }
    bool hasMessage() const { return message.isNotEmpty() && juce::Time::getMillisecondCounter() < messageUntil; }

    juce::RangedAudioParameter* param (const juce::String& id) const { return dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)); }
    juce::RangedAudioParameter* vparam (const char* suffix, int v = -1) const { return param (params::vid (v < 0 ? voice : v, suffix)); }
    float vvalue (const char* suffix, int v = -1) const { return proc.value (params::vid (v < 0 ? voice : v, suffix)); }
    void setParam (const juce::String& id, float realValue) const
    {
        if (auto* p = param (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (realValue)); p->endChangeGesture(); }
    }

    // Etiquetas de los dos controles de carácter de la voz (dependen del modelo o del motor digital).
    std::pair<juce::String, juce::String> charLabels (int v = -1) const
    {
        v = v < 0 ? voice : v;
        if (vvalue ("engine", v) > 0.5f) return { "Start", "Pitch env" };
        const auto& mi = analogModels (familyOf (v))[(size_t) juce::jlimit (0, numAnalogModels (v) - 1, (int) vvalue ("model", v))];
        return { mi.charA, mi.charB };
    }

    std::function<void (const juce::String&)> showPage;
    std::function<void()> toggleManual;
    std::function<void (int voice, const juce::File&)> loadSample;      // carga y avisa
    juce::String page { "pattern" };
};

} // namespace dh::pal::ui
