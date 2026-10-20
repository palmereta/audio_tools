#pragma once
// Preset "modificado": al cargar un preset se toma una foto de los valores de todos los parámetros; el preset está
// modificado si alguno difiere de la foto (un clic, la automatización del host, un undo). La foto viaja con el
// estado, así que al reabrir una sesión el nombre vuelve con su marca si la tenía. Los parámetros de interpretación
// que no son parte del sonido guardado (el botón de stop, la comparación A/B) se excluyen por id.
//
// La foto la escribe el hilo que carga el preset (puede ser el del host) y la lee la interfaz: va bajo un candado.

#include <JuceHeader.h>

namespace dh {

class PresetTracker
{
public:
    explicit PresetTracker (juce::StringArray ignoredIds = {}) : ignored (std::move (ignoredIds)) {}
    // Ajustes de la sesión que los presets no cambian (calidad, salida, tonalidad…): moverlos no "modifica" el preset.
    void ignore (const juce::StringArray& ids) { const juce::SpinLock::ScopedLockType l (lock); ignored.addArray (ids); }

    void capture (juce::AudioProcessor& p)
    {
        std::vector<float> v;
        for (auto* prm : p.getParameters()) v.push_back (prm->getValue());
        const juce::SpinLock::ScopedLockType l (lock);
        snapshot = std::move (v);
    }

    bool modified (juce::AudioProcessor& p) const
    {
        const juce::SpinLock::ScopedLockType l (lock);
        const auto& ps = p.getParameters();
        if (snapshot.size() != (size_t) ps.size()) return false;
        for (int i = 0; i < ps.size(); ++i)
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[i]))
                if (ignored.contains (rp->getParameterID())) continue;
            if (std::abs (ps[i]->getValue() - snapshot[(size_t) i]) > 1.0e-4f) return true;
        }
        return false;
    }

    // El nombre a mostrar: "Nombre *" si cambió algo desde que se cargó.
    juce::String displayName (juce::AudioProcessor& p, const juce::String& name) const { return modified (p) ? name + " *" : name; }

    // Estado: la foto como texto (valores normalizados separados por espacios).
    juce::String toString() const
    {
        const juce::SpinLock::ScopedLockType l (lock);
        juce::String s;
        for (float x : snapshot) s << juce::String (x, 6) << ' ';
        return s.trimEnd();
    }
    void fromString (juce::AudioProcessor& p, const juce::String& s)
    {
        juce::StringArray t; t.addTokens (s, " ", "");
        t.removeEmptyStrings();
        if (t.size() != p.getParameters().size()) { capture (p); return; }   // otra versión del plugin: se toma la actual
        std::vector<float> v;
        for (auto& x : t) v.push_back (x.getFloatValue());
        const juce::SpinLock::ScopedLockType l (lock);
        snapshot = std::move (v);
    }

private:
    juce::StringArray ignored;
    std::vector<float> snapshot;
    mutable juce::SpinLock lock;
};

} // namespace dh
