#pragma once
// La línea de ayuda (y el LCD del ATFS1) van al editor bajo el mouse. Antes era un solo destino global: con dos
// ventanas del mismo plugin abiertas, la ayuda aparecía en la otra y cerrar una apagaba la de la otra.
// Sólo desde el hilo de mensajes.

#include <JuceHeader.h>

namespace dh {

struct HelpSink
{
    virtual ~HelpSink() = default;
    virtual void routeHelp (const juce::String&) = 0;
    virtual void routeLcd (const juce::String&, const juce::String&) {}
};

struct HelpRouter
{
    static std::vector<HelpSink*>& sinks() { static std::vector<HelpSink*> v; return v; }
    static void add (HelpSink* s) { sinks().push_back (s); }
    static void remove (HelpSink* s) { auto& v = sinks(); v.erase (std::remove (v.begin(), v.end(), s), v.end()); }

    static HelpSink* underMouse()
    {
        auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        for (; c != nullptr; c = c->getParentComponent())
            if (auto* s = dynamic_cast<HelpSink*> (c))
                if (std::find (sinks().begin(), sinks().end(), s) != sinks().end()) return s;
        return sinks().size() == 1 ? sinks().front() : nullptr;      // una sola ventana: va ahí
    }
    static void help (const juce::String& t)
    {
        if (auto* s = underMouse()) s->routeHelp (t);
        else if (t.isEmpty()) for (auto* s : sinks()) s->routeHelp (t);  // el mouse salió de la ventana: se limpia
    }
    static void lcd (const juce::String& n, const juce::String& v) { if (auto* s = underMouse()) s->routeLcd (n, v); }
};

} // namespace dh
