#include "PluginEditor.h"
#if DH_HAS_MANUAL
 #include "ManualView.h"
#endif

namespace
{
namespace col
{
    const juce::Colour bg (0xff14110f), bar (0xff0e0c0a), text (0xffefe4cf), dim (0xff9a8d78), line (0xff5a4f42), accent (0xffe2a43b),
                       panel (0xff1c1815), well (0xff0b0908), green (0xff5fbf6a), red (0xffe0452b);
}
juce::Typeface::Ptr typeface (bool semi)
{
    static juce::Typeface::Ptr med = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize);
    static juce::Typeface::Ptr sb = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    return semi ? sb : med;
}
juce::Font font (float h, bool semi = false) { return juce::Font (juce::FontOptions (typeface (semi)).withHeight (h)); }
juce::Image img (const char* d, int n) { return juce::ImageCache::getFromMemory (d, n); }

// La disposición que escribe tools/blender/at330.py (px de la ventana a 1x).
struct Layout
{
    struct Ctl { juce::String id, kind, label, sub; juce::Point<float> c; float top = 0, bottom = 0, r = 0, step = 0; juce::Rectangle<float> rect; };
    struct Key { int note = 0; bool black = false; juce::Path path; juce::Rectangle<float> bounds; };
    struct Sec { juce::String id, title; float x0, x1, y; };
    juce::Rectangle<float> image, bands, nameboard;
    std::vector<Ctl> ctls; std::vector<Key> keys; std::vector<Sec> secs;
    int scale = 2;
    const Ctl* find (const juce::String& id) const { for (auto& c : ctls) if (c.id == id) return &c; return nullptr; }

    Layout()
    {
        const auto v = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::layout_json, BinaryData::layout_jsonSize));
        auto pt = [] (const juce::var& a) { return juce::Point<float> ((float) a[0], (float) a[1]); };
        auto rc = [] (const juce::var& a) { return juce::Rectangle<float> ((float) a[0], (float) a[1], (float) a[2], (float) a[3]); };
        image = rc (v["image"]); bands = rc (v["bands"]); nameboard = rc (v["nameboard"]); scale = (int) v["scale"];
        for (auto& s : *v["sections"].getArray()) secs.push_back ({ s["id"].toString(), s["title"].toString(), (float) s["x0"], (float) s["x1"], (float) s["y"] });
        for (auto& c : *v["controls"].getArray())
        {
            Ctl k; k.id = c["id"].toString(); k.kind = c["kind"].toString(); k.label = c["label"].toString(); k.sub = c["sub"].toString(); k.c = pt (c["c"]);
            if (k.kind == "slider") { k.top = (float) c["top"]; k.bottom = (float) c["bottom"]; }
            if (k.kind == "tablet") k.rect = rc (c["rect"]);
            if (k.kind == "knob") k.r = (float) c["r"];
            if (k.kind == "switch3") k.step = (float) c["step"];
            ctls.push_back (k);
        }
        for (auto& k : *v["keys"].getArray())
        {
            Key key; key.note = (int) k["note"]; key.black = (bool) k["black"];
            auto& p = *k["poly"].getArray();
            for (int i = 0; i < p.size(); ++i) { const auto q = pt (p[i]); if (i == 0) key.path.startNewSubPath (q); else key.path.lineTo (q); }
            key.path.closeSubPath(); key.bounds = key.path.getBounds();
            keys.push_back (key);
        }
    }
};
const Layout& layout() { static Layout l; return l; }

constexpr int kTop = 44, kBottom = 36;
juce::String upperLabel (const juce::String& s) { return s.toUpperCase(); }
}

// ── Aspecto de los controles invisibles y de los de la página Extended ──
struct At330Editor::Lnf : public juce::LookAndFeel_V4
{
    Lnf()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::panel); setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.25f)); setColour (juce::PopupMenu::highlightedTextColourId, col::text);
        setColour (juce::PopupMenu::headerTextColourId, col::accent);
        setColour (juce::ComboBox::backgroundColourId, col::well); setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::outlineColourId, col::line); setColour (juce::ComboBox::arrowColourId, col::accent);
        setColour (juce::TooltipWindow::backgroundColourId, col::panel); setColour (juce::TooltipWindow::textColourId, col::text);
    }
    juce::Font getPopupMenuFont() override { return font (17.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (16.0f); }
    // Sobre el panel: nada (la pieza la pinta el editor). En Extended: perilla oscura con arco ámbar.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        if (s.getProperties()["hidden"]) return;
        const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6);
        const float rad = std::min (r.getWidth(), r.getHeight()) / 2, cx = r.getCentreX(), cy = r.getCentreY(), a = a0 + pos * (a1 - a0);
        juce::Path track; track.addCentredArc (cx, cy, rad, rad, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path val; val.addCentredArc (cx, cy, rad, rad, 0, a0, a, true);
        g.setColour (col::accent); g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff201c19)); g.fillEllipse (cx - rad * 0.72f, cy - rad * 0.72f, rad * 1.44f, rad * 1.44f);
        g.setColour (col::text);
        g.drawLine (cx + std::sin (a) * rad * 0.25f, cy - std::cos (a) * rad * 0.25f, cx + std::sin (a) * rad * 0.66f, cy - std::cos (a) * rad * 0.66f, 2.4f);
    }
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override {}
    int getSliderThumbRadius (juce::Slider&) override { return 12; }
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::textColourId, col::dim); l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack); l->setFont (font (15.0f));
        return l;
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        if (b.getProperties()["hidden"]) return;
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::accent.withAlpha (0.22f) : (over || down ? col::line.withAlpha (0.4f) : juce::Colours::transparentBlack));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? col::accent : col::line); g.drawRoundedRectangle (r, 5.0f, 1.2f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        if (b.getProperties()["hidden"]) return;
        g.setFont (font (17.0f, true)); g.setColour (b.getToggleState() ? col::accent : col::text.withAlpha (b.isEnabled() ? 0.85f : 0.4f));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        if (b.getProperties()["hidden"]) return;
        auto r = b.getLocalBounds().toFloat();
        auto box = r.removeFromLeft (r.getHeight()).reduced (5);
        g.setColour (b.getToggleState() ? col::accent : col::line); g.drawRoundedRectangle (box, 3.0f, 1.5f);
        if (b.getToggleState()) g.fillRoundedRectangle (box.reduced (4), 2.0f);
        g.setColour (col::text); g.setFont (font (16.0f)); g.drawText (b.getButtonText(), r.withTrimmedLeft (4), juce::Justification::centredLeft);
    }
};

// ── El teclado: notas con el mouse (velocidad según la altura del clic) ──
class At330Editor::Keys : public juce::Component
{
public:
    explicit Keys (At330Processor& p) : proc (p) {}
    std::function<void()> onChange;
    void mouseDown (const juce::MouseEvent& e) override { press (e); }
    void mouseDrag (const juce::MouseEvent& e) override { press (e); }
    void mouseUp (const juce::MouseEvent&) override { release(); }
    void paint (juce::Graphics&) override {}

private:
    At330Processor& proc; int note = -1;
    int keyAt (juce::Point<float> p) const
    {
        const auto q = p + getPosition().toFloat();
        for (auto& k : layout().keys) if (k.black && k.path.contains (q)) return k.note;
        for (auto& k : layout().keys) if (! k.black && k.path.contains (q)) return k.note;
        return -1;
    }
    void press (const juce::MouseEvent& e)
    {
        const int n = keyAt (e.position);
        if (n == note) return;
        release();
        if (n < 0) return;
        const auto& kb = std::find_if (layout().keys.begin(), layout().keys.end(), [n] (auto& k) { return k.note == n; })->bounds;
        const float vel = juce::jlimit (0.25f, 1.0f, 0.3f + 0.7f * (e.position.y + (float) getY() - kb.getY()) / kb.getHeight());
        note = n; proc.keyboardState.noteOn (1, n, vel);
        if (onChange) onChange();
    }
    void release() { if (note >= 0) { proc.keyboardState.noteOff (1, note, 0.0f); note = -1; if (onChange) onChange(); } }
};

// ── Página Extended: lo que el original no tenía ──
class At330Editor::Extended : public juce::Component
{
public:
    Extended (At330Processor& p, juce::Image background) : proc (p), back (std::move (background))
    {
        struct Item { const char* id; const char* help; };
        groups = {
            { "ENVELOPES AND KEYS", {
                { "keying", "Classic: one envelope per section, as on the original; every new key dips 50 ms and attacks the whole chord again. Poly: each key has its own." },
                { "velocity", "How much the key velocity sets the level (Poly only; the original had no velocity)." },
                { "split", "Where the upper half of the keyboard starts (the original: C4)." },
                { "bend_range", "Pitch bend range, up and down. The original's Pitch Shift only bends down." },
                { "mod_vib", "How much vibrato the mod wheel adds to the choir and the vocoder." } } },
            { "VOCODER", {
                { "hold", "Hold keeps each band for about 2.5 s after the voice stops (the original's pedal). Freeze keeps the loudest level each band heard until you switch it off." },
                { "voc_response", "How fast the ten band detectors follow the voice. Short: crisp, robotic. Long: smeared." },
                { "formant_shift", "Moves the ten carrier bands up or down: a bigger or smaller voice on the same notes." },
                { "carrier", "What the vocoder speaks with: the keys, an external synth on the right input channel (the voice then goes on the left), or both." },
                { "ext_level", "Level of the external carrier." } } },
            { "ENSEMBLE AND SPACE", {
                { "str_ens", "Strings through the ensemble (always on in the original)." },
                { "ens_depth", "Depth of the four-BBD ensemble; 100 % is the original's." },
                { "space_mix", "A plate reverb after the ensemble." },
                { "space_size", "Size of the reverb." } } },
        };
        for (auto& gr : groups)
            for (auto& it : gr.items)
            {
                Ctl c; c.id = it.id; c.help = it.help;
                auto* prm = proc.apvts.getParameter (c.id);
                c.title = prm->getName (40);
                if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (prm))
                {
                    c.combo = std::make_unique<juce::ComboBox>(); c.combo->addItemList (ch->choices, 1);
                    c.ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, c.id, *c.combo);
                    addAndMakeVisible (*c.combo);
                }
                else if (dynamic_cast<juce::AudioParameterBool*> (prm) != nullptr)
                {
                    c.toggle = std::make_unique<juce::ToggleButton> ("On");
                    c.ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, c.id, *c.toggle);
                    addAndMakeVisible (*c.toggle);
                }
                else
                {
                    c.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                    c.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 120, 22);
                    c.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
                    c.slider->setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe9b45a));
                    c.sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, c.id, *c.slider);
                    addAndMakeVisible (*c.slider);
                }
                ctls.push_back (std::move (c));
            }
    }
    void resized() override
    {
        // Tres columnas; cada control: título, el control y la ayuda debajo.
        const int colW = (getWidth() - 152) / 3;
        int k = 0;
        for (size_t gi = 0; gi < groups.size(); ++gi)
        {
            auto r = juce::Rectangle<int> (66 + (int) gi * (colW + 10), 112, colW, getHeight() - 170);
            groupR[gi] = r;
            r.removeFromTop (34);
            for (size_t j = 0; j < groups[gi].items.size(); ++j, ++k)
            {
                auto& c = ctls[(size_t) k];
                auto cell = r.removeFromTop (128);
                c.titleR = cell.removeFromTop (24).reduced (16, 0);
                auto ctl = cell.removeFromLeft (150).reduced (14, 4);
                c.helpR = cell.reduced (8, 0);
                if (c.slider) c.slider->setBounds (ctl.withSizeKeepingCentre (100, 100));
                if (c.combo) c.combo->setBounds (ctl.withSizeKeepingCentre (136, 32));
                if (c.toggle) c.toggle->setBounds (ctl.withSizeKeepingCentre (90, 30));
            }
        }
    }
    void paint (juce::Graphics& g) override
    {
        // El mismo instrumento de fondo (nogal y placas), un poco más oscuro, y los grupos como placas atornilladas.
        g.drawImage (back, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRect (getLocalBounds());
        auto plate = [&] (juce::Rectangle<float> r) {
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0, 3), 5.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2925), r.getX(), r.getY(), juce::Colour (0xff201c19), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (juce::Colour (0x22ffffff)); g.drawLine (r.getX() + 4, r.getY() + 1, r.getRight() - 4, r.getY() + 1, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawRoundedRectangle (r, 5.0f, 1.0f);
            for (auto p : { r.getTopLeft() + juce::Point<float> (9, 9), r.getTopRight() + juce::Point<float> (-9, 9),
                            r.getBottomLeft() + juce::Point<float> (9, -9), r.getBottomRight() + juce::Point<float> (-9, -9) })
            {
                g.setColour (juce::Colour (0xff8d877e)); g.fillEllipse (p.x - 3.5f, p.y - 3.5f, 7, 7);
                g.setColour (juce::Colour (0xff3a3632)); g.drawLine (p.x - 2.5f, p.y, p.x + 2.5f, p.y, 1.0f);
            }
        };
        auto say = [&] (const juce::String& t, juce::Rectangle<float> r, juce::Font f, juce::Colour c, juce::Justification j) {
            g.setFont (f); g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawText (t, r.translated (0, 1.2f), j);
            g.setColour (c); g.drawText (t, r, j);
        };
        const auto silk = juce::Colour (0xfff3e8d2), silkDim = juce::Colour (0xffb9aa90);
        plate ({ 66.0f, 62.0f, (float) getWidth() - 132.0f, 40.0f });
        say ("EXTENDED", { 90, 62, 300, 40 }, font (24.0f, true), silk, juce::Justification::centredLeft);
        say ("What the original did not have. Everything here is automatable and saved with the preset.", { 230, 62, 1000, 40 }, font (17.0f), silkDim, juce::Justification::centredLeft);
        int k = 0;
        for (size_t gi = 0; gi < groups.size(); ++gi)
        {
            plate (groupR[gi].toFloat());
            say (groups[gi].title, groupR[gi].toFloat().reduced (22, 10).removeFromTop (24), font (20.0f, true), juce::Colour (0xffe9b45a), juce::Justification::centredLeft);
            for (size_t j = 0; j < groups[gi].items.size(); ++j, ++k)
            {
                auto& c = ctls[(size_t) k];
                say (upperLabel (c.title), c.titleR.toFloat(), font (18.0f, true), silk, juce::Justification::centredLeft);
                g.setColour (silkDim); g.setFont (font (16.0f));
                g.drawFittedText (c.help, c.helpR.withTrimmedTop (6), juce::Justification::topLeft, 6, 1.0f);
            }
        }
    }

private:
    At330Processor& proc;
    struct Item { const char* id; const char* help; };
    struct Group { const char* title; std::vector<Item> items; };
    struct Ctl
    {
        juce::String id, title, help; juce::Rectangle<int> titleR, helpR;
        std::unique_ptr<juce::Slider> slider; std::unique_ptr<juce::ComboBox> combo; std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    };
    std::vector<Group> groups; std::vector<Ctl> ctls; std::array<juce::Rectangle<int>, 3> groupR {};
    juce::Image back;
};

// ── El contenido: 1600 x 960, escalado por el editor ──
class At330Editor::Content : public juce::Component, public juce::SettableTooltipClient
{
public:
    Content (At330Processor& p, At330Editor& e) : proc (p), ed (e), keys (p), extended (p, img (BinaryData::bg_jpg, BinaryData::bg_jpgSize))
    {
        const auto& L = layout();
        bg = img (BinaryData::bg_jpg, BinaryData::bg_jpgSize); on = img (BinaryData::on_jpg, BinaryData::on_jpgSize);
        down = img (BinaryData::keysdown_jpg, BinaryData::keysdown_jpgSize);
        knobImg = img (BinaryData::knob_png, BinaryData::knob_pngSize);
        leverImg = img (BinaryData::lever_png, BinaryData::lever_pngSize);
        buildStatic();
        // Controles invisibles sobre cada pieza.
        for (auto& c : L.ctls)
        {
            if (c.kind == "knob")
            {
                auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
                s->getProperties().set ("hidden", true);
                s->setBounds (juce::Rectangle<int> ((int) (c.c.x - c.r - 4), (int) (c.c.y - c.r - 4), (int) (2 * c.r + 8), (int) (2 * c.r + 8)));
                sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, c.id, *s));
                tip (*s, c.id); addAndMakeVisible (*s); sliders.push_back (std::move (s));
            }
            else if (c.kind == "tablet")
            {
                auto b = std::make_unique<juce::TextButton>();
                b->getProperties().set ("hidden", true); b->setClickingTogglesState (false);
                b->setBounds (c.rect.toNearestInt());
                const auto id = c.id;
                if (id == "hold")       // Hold va y viene entre Off y Hold (Freeze se elige en Extended y también se apaga acá)
                    b->onClick = [this] { auto* prm = proc.apvts.getParameter ("hold"); const int v = (int) proc.value ("hold");
                                          prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v == 0 ? 1.0f : 0.0f)); prm->endChangeGesture(); };
                else
                    b->onClick = [this, id] { auto* prm = proc.apvts.getParameter (id);
                                              prm->beginChangeGesture(); prm->setValueNotifyingHost (proc.value (id) > 0.5f ? 0.0f : 1.0f); prm->endChangeGesture(); };
                tip (*b, id); addAndMakeVisible (*b); buttons.push_back (std::move (b));
            }
            else if (c.kind == "switch3")
            {
                auto b = std::make_unique<juce::TextButton>();
                b->getProperties().set ("hidden", true);
                b->setBounds (juce::Rectangle<int> ((int) c.c.x - 14, (int) (c.c.y - c.step * 1.6f), 70, (int) (c.step * 3.2f)));
                b->onClick = [this] { auto* prm = proc.apvts.getParameter ("pitch_mode"); const int v = (int) proc.value ("pitch_mode");
                                      prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ((v + 1) % 3))); prm->endChangeGesture(); };
                tip (*b, "pitch_mode"); addAndMakeVisible (*b); buttons.push_back (std::move (b));
            }
        }
        const auto ki = L.keys.front().bounds.getUnion (L.keys.back().bounds);
        keys.setBounds (juce::Rectangle<float> (0, ki.getY(), (float) baseW, ki.getHeight()).toNearestInt());
        keys.onChange = [this] { repaint (keyArea()); };
        addAndMakeVisible (keys);
        // Barra de arriba.
        for (auto* b : { &prevBtn, &presetBtn, &nextBtn, &panelBtn, &extBtn, &manualBtn }) addAndMakeVisible (b);
        prevBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); };
        nextBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); };
        presetBtn.onClick = [this] { ed.showPresetMenu(); };
        panelBtn.onClick = [this] { ed.showPage (0); }; extBtn.onClick = [this] { ed.showPage (1); }; manualBtn.onClick = [this] { ed.showPage (2); };
        addChildComponent (extended);
        extended.setBounds (0, kTop, baseW, baseH - kTop - kBottom);
        presetBtn.setTooltip ("Factory presets by category. The ones marked (Audio In) need a voice at the input (sidechain).");
    }

    void setPage (int p)
    {
        extended.setVisible (p == 1);
        panelBtn.setToggleState (p == 0, juce::dontSendNotification); extBtn.setToggleState (p == 1, juce::dontSendNotification);
        manualBtn.setToggleState (p == 2, juce::dontSendNotification);
        repaint();
    }
    juce::TextButton presetBtn;

    void resized() override
    {
        prevBtn.setBounds (150, 9, 30, 26); presetBtn.setBounds (184, 7, 330, 30); nextBtn.setBounds (518, 9, 30, 26);
        panelBtn.setBounds (580, 8, 92, 28); extBtn.setBounds (678, 8, 104, 28); manualBtn.setBounds (788, 8, 92, 28);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& L = layout();
        g.fillAll (col::bg);
        // El instrumento: capa estática (render + serigrafía) y encima lo que cambia.
        const float s = (float) L.scale;
        auto src = [&] (juce::Rectangle<float> r) { return juce::Rectangle<float> (r.getX() * s, (r.getY() - L.image.getY()) * s, r.getWidth() * s, r.getHeight() * s).toNearestInt(); };
        // Sólo el pedazo de la capa estática que hay que repintar (un LED que parpadea no redibuja los 3200 px).
        auto blit = [&] (const juce::Image& im, juce::Rectangle<int> dst) {
            dst = dst.getIntersection (L.image.toNearestInt());
            if (dst.isEmpty()) return;
            const auto r = src (dst.toFloat());
            g.drawImage (im, dst.getX(), dst.getY(), dst.getWidth(), dst.getHeight(), r.getX(), r.getY(), r.getWidth(), r.getHeight());
        };
        blit (staticLayer, g.getClipBounds().expanded (1));
        for (auto& c : L.ctls)
        {
            if (c.kind == "tablet" && tabletOn (c.id))
            {
                const auto r = src (c.rect);
                g.drawImage (on, (int) c.rect.getX(), (int) c.rect.getY(), (int) c.rect.getWidth(), (int) c.rect.getHeight(), r.getX(), r.getY(), r.getWidth(), r.getHeight());
                if (c.id == "hold" && (int) proc.value ("hold") == 2)       // Freeze: un punto ámbar sobre la llave
                { g.setColour (col::accent); g.fillEllipse (c.rect.getCentreX() - 4, c.rect.getY() + 10, 8, 8); }
            }
            else if (c.kind == "knob") drawKnob (g, c);
            else if (c.kind == "switch3")
            {
                const float y = c.c.y + ((int) proc.value ("pitch_mode") - 1) * c.step;
                g.drawImage (leverImg, juce::Rectangle<float> (c.c.x - leverImg.getWidth() / s / 2, y - leverImg.getHeight() / s / 2, leverImg.getWidth() / s, leverImg.getHeight() / s));
                for (int i = 0; i < 3; ++i)
                {
                    g.setColour (i == (int) proc.value ("pitch_mode") ? col::accent : juce::Colour (0xffb9aa90)); g.setFont (font (14.5f, true));
                    g.drawText (i == 0 ? "OFF" : i == 1 ? "AUTO" : "MAN", (int) c.c.x + 17, (int) (c.c.y + (i - 1) * c.step) - 9, 60, 18, juce::Justification::centredLeft);
                }
            }
            else if (c.kind == "led") drawLed (g, c);
        }
        drawBands (g);
        // Teclas que suenan: el render de las teclas apretadas, recortado a cada tecla.
        for (auto& k : L.keys)
            if (proc.keyboardState.isNoteOn (1, k.note))
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (k.path);
                blit (down, k.bounds.toNearestInt().expanded (1));
                g.setColour (col::accent.withAlpha (0.18f)); g.fillPath (k.path);
            }
        drawSplit (g);
        drawTopBar (g); drawBottomBar (g);
        if (! dragging.isEmpty()) drawValue (g, dragging);
    }

    juce::Rectangle<int> keyArea() const { return keys.getBounds().expanded (2); }
    // Pinta de nuevo lo que cambió desde el último tic (parámetros, teclas, visor, LEDs, nivel de entrada).
    void tick()
    {
        const auto& L = layout();
        for (auto& c : L.ctls)
        {
            const float v = c.kind == "led" ? ledLevel (c.id) : proc.value (c.id);
            auto& old = last[c.id];
            if (std::abs (old - v) > 1e-4f) { old = v; repaint (ctlArea (c)); }
        }
        std::array<float, dh::vp::kBands> b {};
        bool bandsChanged = false;
        for (int i = 0; i < dh::vp::kBands; ++i) { b[(size_t) i] = segLevel (proc.view.bands[(size_t) i].load()); bandsChanged |= b[(size_t) i] != bandShown[(size_t) i]; }
        if (bandsChanged) { bandShown = b; repaint (L.bands.expanded (8).toNearestInt()); }
        const float mic = proc.view.mic.exchange (0.0f);
        micShown = std::max (mic, micShown * 0.85f);
        if (micShown < 1e-4f) micShown = 0.0f;
        const int micPx = (int) (140.0f * juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (micShown, -60.0f) + 60.0f) / 60.0f));
        if (micPx != lastMicPx) { lastMicPx = micPx; repaint (1136, 12, 150, 20); }
        uint64_t mask[2] = { 0, 0 };
        for (int n = 0; n < 128; ++n) if (proc.keyboardState.isNoteOn (1, n)) mask[n / 64] |= 1ull << (n % 64);
        if (mask[0] != keyMask[0] || mask[1] != keyMask[1]) { keyMask[0] = mask[0]; keyMask[1] = mask[1]; repaint (keyArea()); }
        // El valor del control que se mueve.
        juce::String d;
        for (size_t i = 0; i < sliders.size(); ++i) if (sliders[i]->isMouseButtonDown() || sliders[i]->isMouseOver()) d = idOfSlider ((int) i);
        if (d != dragging) { if (dragging.isNotEmpty()) repaint (valueArea (dragging)); dragging = d; if (d.isNotEmpty()) repaint (valueArea (d)); }
        else if (d.isNotEmpty()) repaint (valueArea (d));
        const auto st = statusText();
        if (st != lastStatus) { lastStatus = st; repaint (0, baseH - kBottom, baseW, kBottom); }
        if (proc.presetDisplayName() != presetBtn.getButtonText()) presetBtn.setButtonText (proc.presetDisplayName());
    }

private:
    At330Processor& proc; At330Editor& ed;
    Keys keys; Extended extended;
    juce::Image bg, on, down, knobImg, leverImg, staticLayer;
    std::vector<std::unique_ptr<juce::Slider>> sliders; std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, panelBtn { "PANEL" }, extBtn { "EXTENDED" }, manualBtn { "MANUAL" };
    std::map<juce::String, float> last;
    std::array<float, dh::vp::kBands> bandShown {};
    float micShown = 0.0f; int lastMicPx = -1; uint64_t keyMask[2] = { 0, 0 };
    juce::String dragging, lastStatus;

    void tip (juce::SettableTooltipClient& c, const juce::String& id) { if (auto* p = proc.apvts.getParameter (id)) c.setTooltip (p->getName (60)); }
    juce::String idOfSlider (int i) const
    {
        int k = 0;
        for (auto& c : layout().ctls) if (c.kind == "knob") { if (k == i) return c.id; ++k; }
        return {};
    }
    bool tabletOn (const juce::String& id) const { return id == "hold" ? (int) proc.value ("hold") > 0 : proc.value (id) > 0.5f; }
    float norm (const juce::String& id) const { if (auto* p = proc.apvts.getParameter (id)) return p->getValue(); return 0.0f; }
    static float segLevel (float env) { return (float) juce::jlimit (0, 12, (int) std::floor ((juce::Decibels::gainToDecibels (env * 6.0f, -60.0f) + 48.0f) / 4.0f)); }
    float ledLevel (const juce::String& id) const
    {
        if (id == "mic_led") return juce::Decibels::gainToDecibels (micShown, -60.0f) > -3.0f ? 1.0f : 0.0f;
        // Vibrato: el LED parpadea a la velocidad del LFO mientras hay vibrato.
        const float depth = proc.value ("vib_depth");
        if (depth < 0.5f) return 0.0f;
        const double t = juce::Time::getMillisecondCounterHiRes() * 0.001 * proc.value ("vib_rate");
        return (float) (0.5 + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * t)) > 0.5f ? 1.0f : 0.3f;
    }
    juce::Rectangle<int> ctlArea (const Layout::Ctl& c) const
    {
        if (c.kind == "tablet") return c.rect.expanded (4).toNearestInt();
        if (c.kind == "knob") return juce::Rectangle<float> (c.c.x - c.r - 6, c.c.y - c.r - 6, 2 * c.r + 12, 2 * c.r + 12).toNearestInt();
        if (c.kind == "switch3") return juce::Rectangle<float> (c.c.x - 20, c.c.y - c.step * 2, 90, c.step * 4).toNearestInt();
        return juce::Rectangle<float> (c.c.x - 16, c.c.y - 16, 32, 32).toNearestInt();
    }
    juce::Rectangle<int> valueArea (const juce::String& id) const
    {
        if (auto* c = layout().find (id))
        {
            return juce::Rectangle<float> (c->c.x - 60, c->c.y - c->r - 52, 120, 30).toNearestInt();
        }
        return {};
    }

    void drawKnob (juce::Graphics& g, const Layout::Ctl& c)
    {
        const float s = (float) layout().scale, w = knobImg.getWidth() / s, h = knobImg.getHeight() / s;
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (c.c.x - c.r + 2, c.c.y - c.r + 4, 2 * c.r, 2 * c.r);
        g.drawImage (knobImg, juce::Rectangle<float> (c.c.x - w / 2, c.c.y - h / 2, w, h));
        const float a = juce::degreesToRadians (-135.0f + 270.0f * norm (c.id));
        g.setColour (col::text);
        g.drawLine (c.c.x + std::sin (a) * c.r * 0.2f, c.c.y - std::cos (a) * c.r * 0.2f, c.c.x + std::sin (a) * c.r * 0.78f, c.c.y - std::cos (a) * c.r * 0.78f, 2.6f);
    }
    void drawLed (juce::Graphics& g, const Layout::Ctl& c)
    {
        const float lv = ledLevel (c.id);
        if (lv <= 0.0f) return;
        juce::ColourGradient glow (col::red.withAlpha (0.55f * lv), c.c.x, c.c.y, col::red.withAlpha (0.0f), c.c.x + 14, c.c.y, true);
        g.setGradientFill (glow); g.fillEllipse (c.c.x - 14, c.c.y - 14, 28, 28);
        g.setColour (juce::Colour (0xffff6a4a).withAlpha (lv)); g.fillEllipse (c.c.x - 3.6f, c.c.y - 3.6f, 7.2f, 7.2f);
    }
    void drawBands (juce::Graphics& g)
    {
        const auto r = layout().bands;
        const float w = r.getWidth() / dh::vp::kBands, sh = r.getHeight() / 12.0f;
        for (int b = 0; b < dh::vp::kBands; ++b)
            for (int sgm = 0; sgm < 12; ++sgm)
            {
                const bool lit = sgm < (int) bandShown[(size_t) b];
                auto seg = juce::Rectangle<float> (r.getX() + w * (float) b + w * 0.19f, r.getBottom() - sh * (float) (sgm + 1) + sh * 0.18f, w * 0.62f, sh * 0.64f);
                g.setColour (lit ? juce::Colour (0xffffb347) : juce::Colour (0xff2a1d10)); g.fillRoundedRectangle (seg, 1.0f);
                if (lit) { g.setColour (juce::Colour (0x33ff9a2a)); g.fillRoundedRectangle (seg.expanded (1.5f), 2.0f); }
            }
    }
    void drawSplit (juce::Graphics& g)
    {
        // La división: el borde izquierdo de la primera tecla de la mitad de arriba.
        const int split = 36 + (int) proc.value ("split");
        for (auto& k : layout().keys)
            if (k.note == split)
            {
                const float x = k.black ? k.bounds.getCentreX() : k.bounds.getX();
                g.setColour (col::accent.withAlpha (0.9f)); g.fillRect (x - 1.0f, k.bounds.getY() - 20.0f, 2.0f, 18.0f);
                g.setFont (font (13.0f, true));
                g.drawText ("LOWER", (int) x - 64, (int) k.bounds.getY() - 20, 58, 14, juce::Justification::centredRight);
                g.drawText ("UPPER", (int) x + 6, (int) k.bounds.getY() - 20, 58, 14, juce::Justification::centredLeft);
            }
    }
    void drawValue (juce::Graphics& g, const juce::String& id)
    {
        auto* p = proc.apvts.getParameter (id);
        if (p == nullptr) return;
        const auto r = valueArea (id).toFloat().reduced (8, 3);
        g.setColour (juce::Colour (0xee0e0c0a)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::accent); g.setFont (font (15.0f, true));
        g.drawText (p->getCurrentValueAsText(), r, juce::Justification::centred);
    }
    juce::String statusText() const
    {
        auto txt = [this] (const char* id) { return proc.apvts.getParameter (id)->getCurrentValueAsText(); };
        return "Envelopes: " + txt ("keying") + "     Hold: " + txt ("hold") + "     Carrier: " + txt ("carrier") + "     Formant shift: " + txt ("formant_shift")
               + "     Ensemble depth: " + txt ("ens_depth") + "     Space: " + txt ("space_mix") + "     Velocity: " + txt ("velocity");
    }
    void drawTopBar (juce::Graphics& g)
    {
        g.setColour (col::bar); g.fillRect (0, 0, baseW, kTop);
        g.setColour (col::text); g.setFont (font (28.0f, true)); g.drawText ("AT330", 20, 0, 120, kTop, juce::Justification::centredLeft);
        g.setColour (col::dim); g.setFont (font (15.0f));
        g.drawText ("VOICE IN", 1060, 0, 70, kTop, juce::Justification::centredRight);
        const auto m = juce::Rectangle<float> (1140, 16, 140, 12);
        g.setColour (col::well); g.fillRoundedRectangle (m, 3.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (micShown, -60.0f) + 60.0f) / 60.0f);
        g.setColour (lv > 0.95f ? col::red : col::green); g.fillRoundedRectangle (m.withWidth (m.getWidth() * lv), 3.0f);
        g.setColour (col::dim);
        g.drawText ("SPLIT " + dh::vp::params::noteName (36 + (int) proc.value ("split")), 1300, 0, 110, kTop, juce::Justification::centredLeft);
        g.drawText ((int) proc.value ("keying") == 0 ? "CLASSIC" : "POLY", 1410, 0, 80, kTop, juce::Justification::centredLeft);
        g.drawText (AT330_VERSION, baseW - 120, 0, 100, kTop, juce::Justification::centredRight);
    }
    void drawBottomBar (juce::Graphics& g)
    {
        g.setColour (col::bar); g.fillRect (0, baseH - kBottom, baseW, kBottom);
        g.setColour (col::dim); g.setFont (font (15.0f));
        g.drawText (lastStatus, 20, baseH - kBottom, baseW - 40, kBottom, juce::Justification::centredLeft);
        g.setColour (col::accent); g.drawText ("audiotools.com.ar", baseW - 220, baseH - kBottom, 200, kBottom, juce::Justification::centredRight);
    }

    // Capa estática: el render con la serigrafía encima, a 2x. La serigrafía es crema con una sombra leve (se lee sobre
    // las placas y sobre el nogal), los títulos llevan una marca del color de su sección y las llaves dos renglones: qué
    // son y en qué pie y mitad del teclado suenan.
    void buildStatic()
    {
        const auto& L = layout();
        const float s = (float) L.scale;
        staticLayer = bg.createCopy();
        juce::Graphics g (staticLayer);
        g.addTransform (juce::AffineTransform::translation (0, -L.image.getY()).scaled (s));
        const auto silk = juce::Colour (0xfff3e8d2), silkDim = juce::Colour (0xffb9aa90);
        auto say = [&] (const juce::String& t, juce::Rectangle<float> r, juce::Font f, juce::Colour c, juce::Justification j = juce::Justification::centred) {
            g.setFont (f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawText (t, r.translated (0.0f, 1.2f), j);
            g.setColour (c); g.drawText (t, r, j);
        };
        auto secColour = [] (const juce::String& id) {
            if (id == "str") return juce::Colour (0xffc78520);
            if (id == "choir") return juce::Colour (0xffb0452a);
            if (id == "voc") return juce::Colour (0xff2b8e94);
            return juce::Colour (0xffd9cdb6);
        };
        for (auto& sec : L.secs)
        {
            const auto f = font (22.0f, true);
            const float cx = (sec.x0 + sec.x1) / 2, tw = juce::GlyphArrangement::getStringWidth (f, sec.title) / 2;
            g.setColour (secColour (sec.id)); g.fillRoundedRectangle (cx - tw - 18, sec.y - 5, 10, 10, 2.0f);
            say (sec.title, juce::Rectangle<float> (cx - tw - 2, sec.y - 13, 2 * tw + 4, 26), f, silk);
            g.setColour (silkDim.withAlpha (0.55f));
            g.fillRect (sec.x0 + 10, sec.y - 0.6f, std::max (0.0f, cx - tw - 26 - sec.x0 - 10), 1.2f);
            g.fillRect (cx + tw + 8, sec.y - 0.6f, std::max (0.0f, sec.x1 - 10 - cx - tw - 8), 1.2f);
        }
        for (auto& c : L.ctls)
        {
            if (c.kind == "tablet")
            {
                say (c.label, juce::Rectangle<float> (c.rect.getCentreX() - 44, c.rect.getBottom() + 3, 88, 18), font (15.5f, true), silk);
                if (c.sub.isNotEmpty()) say (c.sub, juce::Rectangle<float> (c.rect.getCentreX() - 44, c.rect.getBottom() + 19, 88, 15), font (13.0f), silkDim);
            }
            else if (c.kind == "knob")
            {
                // La escala: 11 marcas en 270 grados y los números 0, 5 y 10 afuera.
                for (int i = 0; i <= 10; ++i)
                {
                    const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) i), r0 = c.r + 6.0f, r1 = c.r + (i % 5 == 0 ? 11.5f : 9.0f);
                    g.setColour (silkDim);
                    g.drawLine (c.c.x + std::sin (a) * r0, c.c.y - std::cos (a) * r0, c.c.x + std::sin (a) * r1, c.c.y - std::cos (a) * r1, i % 5 == 0 ? 1.6f : 1.1f);
                    if (i % 5 == 0)
                    {
                        const float rn = c.r + 15.5f;
                        say (juce::String (i), juce::Rectangle<float> (c.c.x + std::sin (a) * rn - 10, c.c.y - std::cos (a) * rn - 7, 20, 14), font (11.0f, true), silkDim);
                    }
                }
                float fh = 16.0f;
                while (fh > 11.0f && juce::GlyphArrangement::getStringWidth (font (fh, true), c.label) > 58.0f) fh -= 0.5f;
                say (c.label, juce::Rectangle<float> (c.c.x - 50, c.c.y + c.r + 13, 100, 18), font (fh, true), silk);
            }
        }
        say ("BANDS", juce::Rectangle<float> (L.bands.getX(), L.bands.getY() - 27, L.bands.getWidth(), 16), font (14.0f, true), silkDim);
        const char* fq[] = { "200", "", "400", "", "900", "", "2k", "", "4k", "" };
        for (int b = 0; b < 10; ++b)
            if (*fq[b]) say (fq[b], juce::Rectangle<float> (L.bands.getX() + L.bands.getWidth() * ((float) b + 0.5f) / 10.0f - 16, L.bands.getBottom() + 9, 32, 14), font (12.5f), silkDim);
        // La placa del nombre, sobre la franja de nogal.
        const auto nb = L.nameboard;
        say ("AT330", juce::Rectangle<float> (nb.getX() + 30, nb.getY(), 200, nb.getHeight()), font (40.0f, true), silk, juce::Justification::centredLeft);
        say (juce::String::fromUTF8 ("STRINGS  \u00b7  CHOIR  \u00b7  VOCODER"), juce::Rectangle<float> (nb.getX() + 156, nb.getY(), 420, nb.getHeight()), font (19.0f, true),
             juce::Colour (0xffe9b45a), juce::Justification::centredLeft);
        say ("Audio Tools", juce::Rectangle<float> (nb.getRight() - 260, nb.getY(), 230, nb.getHeight()), font (22.0f), silk, juce::Justification::centredRight);
    }
};

// ── Editor ──
At330Editor::At330Editor (At330Processor& p) : AudioProcessorEditor (p), proc (p)
{
    lnf = std::make_unique<Lnf>();
    setLookAndFeel (lnf.get());
    content = std::make_unique<Content> (p, *this);
    content->setBounds (0, 0, baseW, baseH);
    addAndMakeVisible (*content);
    content->setPage (0);
    proc.onPresetChanged = [this] { content->presetBtn.setButtonText (proc.presetDisplayName()); content->repaint(); };
    content->presetBtn.setButtonText (proc.presetDisplayName());
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.5), (int) (baseH * 1.5));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.8), (int) (baseH * 0.8));
    startTimerHz (30);

    // Capturas sin pantalla (scripts/manual-shots.py): AT330_SNAPSHOT=/ruta.png, AT330_PRESET=n, AT330_WAIT=s,
    // AT330_PAGE=panel|extended|manual, AT330_KEYS=45,52,57 (teclas apretadas).
    const auto snap = juce::SystemStats::getEnvironmentVariable ("AT330_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
        const int preset = env ("AT330_PRESET", "-1").getIntValue(), wait = env ("AT330_WAIT", "2").getIntValue();
        const auto pg = env ("AT330_PAGE", "panel"); const auto keysTxt = env ("AT330_KEYS", "");
        juce::Component::SafePointer<At330Editor> safe (this);
        juce::Timer::callAfterDelay (300, [safe, preset, pg, keysTxt] {
            if (safe == nullptr) return;
            if (preset >= 0) safe->proc.setCurrentProgram (preset);
            safe->showPage (pg == "extended" ? 1 : pg == "manual" ? 2 : 0);
            for (auto& k : juce::StringArray::fromTokens (keysTxt, ",", "")) if (k.isNotEmpty()) safe->proc.keyboardState.noteOn (1, k.getIntValue(), 0.8f);
        });
        juce::Timer::callAfterDelay (300 + wait * 1000, [safe, snap] {
            if (safe == nullptr) return;
            safe->setSize (baseW, baseH);
            juce::Component& shot = safe->manual != nullptr ? (juce::Component&) *safe : (juce::Component&) *safe->content;
            const auto im = shot.createComponentSnapshot (shot.getLocalBounds(), true, 2.0f);
            juce::File f (snap); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (im, os);
        });
    }
}

At330Editor::~At330Editor() { stopTimer(); proc.onPresetChanged = nullptr; manual.reset(); content.reset(); setLookAndFeel (nullptr); }

void At330Editor::resized()
{
    const float k = (float) getWidth() / (float) baseW;
    content->setTransform (juce::AffineTransform::scale (k));
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void At330Editor::showPage (int p)
{
    page = p;
   #if DH_HAS_MANUAL
    if (p == 2)
    {
        dh::manual::Theme t; t.bg = col::bg; t.panel = col::panel; t.text = col::text; t.muted = col::dim; t.accent = col::accent; t.line = col::line.withAlpha (0.5f);
        auto m = dh::manual::makeEmbedded (t);
        m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<At330Editor> (this)] { if (safe != nullptr) safe->showPage (0); }); };
        addAndMakeVisible (*m); m->setBounds (getLocalBounds()); m->grabKeyboardFocus();
        manual = std::move (m);
    }
    else manual.reset();
   #else
    if (p == 2) p = 0;
   #endif
    content->setPage (page);
}

void At330Editor::showPresetMenu()
{
    juce::PopupMenu menu; std::map<juce::String, juce::PopupMenu> sub; juce::StringArray order;
    const auto& ps = proc.getPresets();
    for (int i = 0; i < (int) ps.size(); ++i)
    {
        if (! order.contains (ps[(size_t) i].category)) order.add (ps[(size_t) i].category);
        sub[ps[(size_t) i].category].addItem (i + 1, ps[(size_t) i].name, true, i == proc.getCurrentProgram());
    }
    for (auto& c : order) menu.addSubMenu (c, sub[c]);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (content->presetBtn), [this] (int r) { if (r > 0) proc.setCurrentProgram (r - 1); });
}

void At330Editor::timerCallback()
{
    // Live esconde la ventana del editor sin cerrarlo: escondido no se repinta nada (QA en Live 2026-10-22, H3).
    if (! dh::reallyShowing (*this)) return;
    if (page != 2) content->tick();
}

