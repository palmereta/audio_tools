#include "PluginEditor.h"

// El manual con los colores de la serie (el acento de este producto en la web).
static dh::manual::Theme manualTheme() { dh::manual::Theme t; t.accent = juce::Colour (0xffd24a35); return t; }

AT20VEditor::AT20VEditor (AT20VProcessor& p)
    : AudioProcessorEditor (&p), proc (p), bar (p), panel (p), ext (p), keyboard (p), lower (p)
{
    baseW = panel.getWidth();
    baseH = kBarH + panel.getHeight() + keyboard.getHeight() + kLowerH;
    addAndMakeVisible (content);
    for (juce::Component* c : { (juce::Component*) &bar, (juce::Component*) &panel, (juce::Component*) &ext,
                                (juce::Component*) &keyboard, (juce::Component*) &lower })
        content.addAndMakeVisible (c);
    content.setSize (baseW, baseH);
    bar.setBounds (0, 0, baseW, kBarH);
    panel.setTopLeftPosition (0, kBarH);
    ext.setTopLeftPosition (0, kBarH);
    ext.setVisible (false);
    keyboard.setTopLeftPosition (0, kBarH + panel.getHeight());
    lower.setBounds (0, keyboard.getBottom(), baseW, kLowerH);

    bar.onTab = [this] (int t) { panel.setVisible (t == 0); ext.setVisible (t == 1); };
    bar.onManual = [this] { manualHost.toggle (*this, manualTheme()); };
    lower.onOctave = [this] (int d) { keyboard.setOctave (keyboard.getOctave() + d); lower.setOctaveText (keyboard.getOctave()); };
    proc.onCablesChanged = [this] { panel.rebuildCables(); };
    proc.onPresetChanged = [this] { bar.refresh(); };

    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.4), (int) (baseH * 1.4));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.78), (int) (baseH * 0.78));

    // Verificación sin capturar la pantalla: AT20V_SNAPSHOT=/ruta.png guarda una imagen del editor
    // (a 2x); AT20V_PRESET=n carga antes el preset n.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("AT20V_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        const bool manualOpen = juce::SystemStats::getEnvironmentVariable ("AT20V_MANUAL", "0").getIntValue() != 0;      // con el manual abierto
        const int presetIndex = juce::SystemStats::getEnvironmentVariable ("AT20V_PRESET", "-1").getIntValue();
        const int tab = juce::SystemStats::getEnvironmentVariable ("AT20V_TAB", "0").getIntValue();
        const int held = juce::SystemStats::getEnvironmentVariable ("AT20V_HOLD", "-1").getIntValue();
        juce::Component::SafePointer<AT20VEditor> safe (this);
        juce::Timer::callAfterDelay (800, [safe, manualOpen, presetIndex, tab, held] {
            if (safe == nullptr) return;
            if (presetIndex >= 0) safe->proc.loadPreset (presetIndex);
            safe->bar.setTab (tab);
            if (manualOpen) safe->manualHost.open (*safe, manualTheme());
            // AT20V_HOLD=n mantiene apretadas n+0, n+4, n+7 (para ver teclas abajo en la captura).
            if (held >= 0) for (int d : { 0, 4, 7 }) safe->proc.keyboardState.noteOn (1, held + d, 0.8f);
        });
        juce::Timer::callAfterDelay (3500, [safe, snap] {
            if (safe == nullptr) return;
            const bool man = safe->manualHost.isOpen();
            juce::Component& target = man ? (juce::Component&) *safe : (juce::Component&) safe->content;
            const auto img = target.createComponentSnapshot (target.getLocalBounds(), true, man ? 2.0f * (float) safe->baseW / (float) safe->getWidth() : 2.0f);
            juce::File f (snap);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

AT20VEditor::~AT20VEditor()
{
    proc.onCablesChanged = nullptr;
    proc.onPresetChanged = nullptr;
}

void AT20VEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0e0e0f)); }

void AT20VEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content.setTransform (juce::AffineTransform::scale (s));
    manualHost.layout (getLocalBounds());
}
