#include "PluginEditor.h"

// El manual con los colores de la serie (el acento de este producto en la web).
static dh::manual::Theme manualTheme() { dh::manual::Theme t; t.accent = juce::Colour (0xffe0b341); return t; }

AT700Editor::AT700Editor (AT700Processor& p)
    : AudioProcessorEditor (&p), proc (p), bar (p), panel (p), port (panel), ext (p), lower (p)
{
    addAndMakeVisible (content);
    for (juce::Component* c : { (juce::Component*) &bar, (juce::Component*) &port, (juce::Component*) &ext, (juce::Component*) &lower })
        content.addAndMakeVisible (c);
    content.setSize (baseW, baseH);
    bar.setBounds (0, 0, baseW, kBarH);
    port.setBounds (0, kBarH, baseW, kPortH);
    ext.setBounds (0, kBarH, baseW, kPortH);
    ext.setVisible (false);
    lower.setBounds (0, kBarH + kPortH, baseW, kLowerH);

    bar.onTab = [this] (int t) { port.setVisible (t == 0); ext.setVisible (t == 1); };
    bar.onManual = [this] { manualHost.toggle (*this, manualTheme()); };
    bar.onView = [this] (int v) { showView (v); };
    proc.onPresetChanged = [this] { bar.refresh(); repaint(); };
    proc.onCablesChanged = [this] { panel.rebuildCables(); };
    bar.setView (1);
    showView (1, false);

    setResizable (true, true);
    setResizeLimits (baseW / 3, baseH / 3, (int) (baseW * 1.3), (int) (baseH * 1.3));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.62), (int) (baseH * 0.62));

    // Verificación sin capturar la pantalla: AT700_SNAPSHOT=/ruta.png guarda el editor a 2x;
    // AT700_PRESET=n, AT700_TAB=0/1, AT700_VIEW=0..3, AT700_KEYS=1 aprieta un acorde, AT700_RUN=1 arranca el secuenciador.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("AT700_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        const bool manualOpen = juce::SystemStats::getEnvironmentVariable ("AT700_MANUAL", "0").getIntValue() != 0;      // con el manual abierto
        const int presetIndex = juce::SystemStats::getEnvironmentVariable ("AT700_PRESET", "-1").getIntValue();
        const int tab = juce::SystemStats::getEnvironmentVariable ("AT700_TAB", "0").getIntValue();
        const int view = juce::SystemStats::getEnvironmentVariable ("AT700_VIEW", "1").getIntValue();
        const bool keys = juce::SystemStats::getEnvironmentVariable ("AT700_KEYS", "0").getIntValue() != 0;
        const bool run = juce::SystemStats::getEnvironmentVariable ("AT700_RUN", "0").getIntValue() != 0;
        const bool selftest = juce::SystemStats::getEnvironmentVariable ("AT700_SELFTEST", "0").getIntValue() != 0;
        juce::Component::SafePointer<AT700Editor> safe (this);
        juce::Timer::callAfterDelay (800, [safe, manualOpen, presetIndex, tab, view, keys, run, selftest] {
            if (safe == nullptr) return;
            if (presetIndex >= 0) safe->proc.loadPreset (presetIndex);
            safe->bar.setView (view);
            safe->bar.setTab (tab);
            if (manualOpen) safe->manualHost.open (*safe, manualTheme());
            if (keys) for (int n : { 48, 55, 64 }) safe->proc.sendCommand (AT700Processor::CmdNoteOn, n);
            if (run) safe->proc.sendCommand (AT700Processor::CmdSeqStart);
            if (selftest)
            {
                safe->panel.selfTest();
                juce::String s;
                for (auto& c : safe->proc.getCables()) s << dh::jk700::jacks[c.src].id << " -> " << dh::jk700::jacks[c.dst].id << "\n";
                s << "vcf1_cutoff " << safe->proc.value (dh::P700::vcf1_cutoff) << "  vco1_pw " << safe->proc.value (dh::P700::vco1_pw)
                  << "  vco1_wave " << safe->proc.value (dh::P700::vco1_wave) << "  vcf2_mode " << safe->proc.value (dh::P700::vcf2_mode) << "\n";
                std::printf ("%s", s.toRawUTF8());
                std::fflush (stdout);
            }
        });
        juce::Timer::callAfterDelay (4500, [safe, snap] {
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

AT700Editor::~AT700Editor() { proc.onPresetChanged = nullptr; proc.onCablesChanged = nullptr; }

void AT700Editor::showView (int v, bool animate)
{
    const auto views = dh::Assets700::get().layout.getProperty ("views", {});
    static const char* names[4] = { "sequencer", "console", "keyboard", "all" };
    const auto r = views.getProperty (names[juce::jlimit (0, 3, v)], {});
    if (! r.isArray() || r.size() < 4) return;
    port.showRegion (juce::Rectangle<float>::leftTopRightBottom ((float) r[0], (float) r[1], (float) r[2], (float) r[3]), animate);
}

void AT700Editor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0e0e0f)); }

void AT700Editor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW)); manualHost.layout (getLocalBounds()); }
