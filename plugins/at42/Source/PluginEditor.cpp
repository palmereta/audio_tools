#include "PluginEditor.h"

// El manual con los colores de la serie (el acento de este producto en la web).
static dh::manual::Theme manualTheme() { dh::manual::Theme t; t.accent = juce::Colour (0xffea8c3f); return t; }

AT42Editor::AT42Editor (AT42Processor& p)
    : AudioProcessorEditor (&p), proc (p), bar (p), panel (p), ext (p), lower (p)
{
    baseW = panel.getWidth();
    baseH = kBarH + panel.getHeight() + kLowerH;
    addAndMakeVisible (content);
    for (juce::Component* c : { (juce::Component*) &bar, (juce::Component*) &panel, (juce::Component*) &ext, (juce::Component*) &lower })
        content.addAndMakeVisible (c);
    content.setSize (baseW, baseH);
    bar.setBounds (0, 0, baseW, kBarH);
    panel.setTopLeftPosition (0, kBarH);
    ext.setTopLeftPosition (0, kBarH);
    ext.setVisible (false);
    lower.setBounds (0, kBarH + panel.getHeight(), baseW, kLowerH);

    bar.onTab = [this] (int t) { panel.setVisible (t == 0); ext.setVisible (t == 1); };
    bar.onManual = [this] { manualHost.toggle (*this, manualTheme()); };
    proc.onPresetChanged = [this] { bar.refresh(); repaint(); };
    proc.onCablesChanged = [this] { panel.rebuildCables(); };

    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.4), (int) (baseH * 1.4));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.72), (int) (baseH * 0.72));

    // Verificación sin capturar la pantalla: AT42_SNAPSHOT=/ruta.png guarda el editor a 2x;
    // AT42_PRESET=n, AT42_TAB=0/1, AT42_PANEL=0..3 (color), AT42_KEYS=1 aprieta dos teclas drone.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("AT42_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        const bool manualOpen = juce::SystemStats::getEnvironmentVariable ("AT42_MANUAL", "0").getIntValue() != 0;      // con el manual abierto
        const int presetIndex = juce::SystemStats::getEnvironmentVariable ("AT42_PRESET", "-1").getIntValue();
        const int tab = juce::SystemStats::getEnvironmentVariable ("AT42_TAB", "0").getIntValue();
        const int pc = juce::SystemStats::getEnvironmentVariable ("AT42_PANEL", "-1").getIntValue();
        const bool keys = juce::SystemStats::getEnvironmentVariable ("AT42_KEYS", "0").getIntValue() != 0;
        juce::Component::SafePointer<AT42Editor> safe (this);
        juce::Timer::callAfterDelay (800, [safe, manualOpen, presetIndex, tab, pc, keys] {
            if (safe == nullptr) return;
            if (presetIndex >= 0) safe->proc.loadPreset (presetIndex);
            if (pc >= 0) safe->proc.setExtValue ("panel", (float) pc);
            safe->bar.setTab (tab);
            if (manualOpen) safe->manualHost.open (*safe, manualTheme());
            if (keys) { safe->proc.droneKeyUi[1].store (true); safe->proc.droneKeyUi[4].store (true); safe->proc.platePress (2, 0.8f); }
        });
        juce::Timer::callAfterDelay (4000, [safe, snap] {
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

AT42Editor::~AT42Editor() { proc.onPresetChanged = nullptr; proc.onCablesChanged = nullptr; }

void AT42Editor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0e0e0f)); }

void AT42Editor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW)); manualHost.layout (getLocalBounds()); }
