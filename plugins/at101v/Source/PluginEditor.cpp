#include "PluginEditor.h"

// El manual con los colores de la serie (el acento de este producto en la web).
static dh::manual::Theme manualTheme() { dh::manual::Theme t; t.accent = juce::Colour (0xff3a7bd5); return t; }

AT101VEditor::AT101VEditor (AT101VProcessor& p)
    : AudioProcessorEditor (&p), proc (p), bar (p), panel (p), seq (p), ext (p), lower (p)
{
    baseW = panel.getWidth();
    baseH = kBarH + panel.getHeight() + kLowerH;
    addAndMakeVisible (content);
    for (juce::Component* c : { (juce::Component*) &bar, (juce::Component*) &panel, (juce::Component*) &seq,
                                (juce::Component*) &ext, (juce::Component*) &lower })
        content.addAndMakeVisible (c);
    content.setSize (baseW, baseH);
    bar.setBounds (0, 0, baseW, kBarH);
    for (juce::Component* c : { (juce::Component*) &panel, (juce::Component*) &seq, (juce::Component*) &ext })
        c->setTopLeftPosition (0, kBarH);
    seq.setVisible (false);
    ext.setVisible (false);
    lower.setBounds (0, kBarH + panel.getHeight(), baseW, kLowerH);

    panel.keyboardOctave = [this] { return octave; };
    bar.onTab = [this] (int t) { panel.setVisible (t == 0); seq.setVisible (t == 1); ext.setVisible (t == 2); };
    bar.onManual = [this] { manualHost.toggle (*this, manualTheme()); };
    lower.onOctave = [this] (int d) { octave = juce::jlimit (-3, 3, octave + d); lower.setOctaveText (octave); };
    proc.onPresetChanged = [this] { bar.refresh(); repaint(); };

    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.5), (int) (baseH * 1.5));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.8), (int) (baseH * 0.8));

    // Verificación sin capturar la pantalla: AT101V_SNAPSHOT=/ruta.png guarda el editor a 2x;
    // AT101V_PRESET=n carga el preset n, AT101V_TAB=0/1/2 elige la página, AT101V_HOLD=n aprieta
    // n, n+4, n+7 y AT101V_BODY=0/1/2 cambia el color.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("AT101V_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        const bool manualOpen = juce::SystemStats::getEnvironmentVariable ("AT101V_MANUAL", "0").getIntValue() != 0;      // con el manual abierto
        const int presetIndex = juce::SystemStats::getEnvironmentVariable ("AT101V_PRESET", "-1").getIntValue();
        const int tab = juce::SystemStats::getEnvironmentVariable ("AT101V_TAB", "0").getIntValue();
        const int held = juce::SystemStats::getEnvironmentVariable ("AT101V_HOLD", "-1").getIntValue();
        const int body = juce::SystemStats::getEnvironmentVariable ("AT101V_BODY", "-1").getIntValue();
        juce::Component::SafePointer<AT101VEditor> safe (this);
        juce::Timer::callAfterDelay (800, [safe, manualOpen, presetIndex, tab, held, body] {
            if (safe == nullptr) return;
            if (presetIndex >= 0) safe->proc.loadPreset (presetIndex);
            if (body >= 0) safe->proc.setExtValue ("body", (float) body);
            safe->bar.setTab (tab);
            if (manualOpen) safe->manualHost.open (*safe, manualTheme());
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

AT101VEditor::~AT101VEditor() { proc.onPresetChanged = nullptr; }

void AT101VEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0e0e0f)); }

void AT101VEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    manualHost.layout (getLocalBounds());
}
