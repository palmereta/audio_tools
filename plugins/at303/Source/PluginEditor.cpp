#include "PluginEditor.h"

using namespace dh::tb;

namespace {
namespace col {
    const juce::Colour bg (0xff141516), panel (0xff1c1e20), panel2 (0xff26292c), line (0xff35393d), ink (0xffe4e6e8), inkDim (0xff9aa0a6), inkFaint (0xff62676c);
    const juce::Colour amber (0xffffa53a), amberDim (0xff5a3510), red (0xffff5a3c), blue (0xff58b4ff), green (0xff7ee0a8);
}
inline juce::Image loadImg (const char* data, int size) { return juce::ImageCache::getFromMemory (data, size); }
const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'" };
}

// ── Estilo: botones planos grafito, perillas que dibujan la tapa renderizada rotada ──
struct At303Editor::Look : public juce::LookAndFeel_V4
{
    juce::Image capBig, capSmall;
    Look()
    {
        capBig = loadImg (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
        capSmall = loadImg (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
        setColour (juce::PopupMenu::backgroundColourId, col::panel2); setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::amber); setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
        setColour (juce::ComboBox::backgroundColourId, col::panel2); setColour (juce::ComboBox::textColourId, col::ink); setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::TooltipWindow::backgroundColourId, col::ink); setColour (juce::TooltipWindow::textColourId, col::bg);
        setColour (juce::TextButton::buttonColourId, col::panel2); setColour (juce::TextButton::buttonOnColourId, col::amber);
        setColour (juce::TextButton::textColourOffId, col::ink); setColour (juce::TextButton::textColourOnId, col::bg);
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const float ang = a0 + pos * (a1 - a0);
        if (s.getProperties().contains ("cap"))
        {
            const auto& img = (int) s.getProperties()["cap"] == 1 ? capBig : capSmall;
            const float sz = (float) std::min (w, h);
            const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
            // La tapa no gira (su luz es la del render); la sombra es fija y el índice ámbar es lo único que rota.
            const float r = (sz - 24.0f) * 0.5f;             // radio de la pollera: la imagen trae 4 mm (12 px) de aire por lado
            juce::ColourGradient sh (juce::Colours::black.withAlpha (0.55f), cx + r * 0.12f, cy + r * 0.16f, juce::Colours::transparentBlack, cx + r * 0.12f + r * 1.25f, cy + r * 0.16f, true);
            g.setGradientFill (sh); g.fillEllipse (cx - r * 1.15f + r * 0.12f, cy - r * 1.15f + r * 0.16f, r * 2.3f, r * 2.3f);
            g.drawImage (img, juce::Rectangle<float> (sz, sz).withCentre ({ cx, cy }));
            juce::Path ind; ind.addRoundedRectangle (-r * 0.05f, -r * 0.62f, r * 0.1f, r * 0.38f, r * 0.03f);
            g.setColour (juce::Colour (0xffe8a24a)); g.fillPath (ind, juce::AffineTransform::rotation (ang).translated (cx, cy));
            return;
        }
        // Perillas del panel de software: arco ámbar y un punto.
        const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4);
        const float rad = std::min (r.getWidth(), r.getHeight()) * 0.5f, cx = r.getCentreX(), cy = r.getCentreY();
        juce::Path tr; tr.addCentredArc (cx, cy, rad, rad, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (tr, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path v; v.addCentredArc (cx, cy, rad, rad, 0, a0, ang, true);
        g.setColour (col::amber); g.strokePath (v, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::panel2.brighter (0.15f)); g.fillEllipse (cx - rad * 0.68f, cy - rad * 0.68f, rad * 1.36f, rad * 1.36f);
        g.setColour (col::ink); g.drawLine (cx + std::sin (ang) * rad * 0.2f, cy - std::cos (ang) * rad * 0.2f, cx + std::sin (ang) * rad * 0.62f, cy - std::cos (ang) * rad * 0.62f, 2.0f);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.5f)); }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& l) override { l.setBounds (4, 1, b.getWidth() - 20, b.getHeight() - 2); l.setFont (getComboBoxFont (b)); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (14.0f, (float) h * 0.5f), juce::Font::bold)); }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        juce::Colour c = b.getToggleState() ? col::amber : col::panel2;
        if (over && ! b.getToggleState()) c = c.brighter (0.08f);
        if (down) c = c.darker (0.15f);
        g.setColour (c); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
};

// ── El instrumento: el render y todo lo que se mueve encima ──
class At303Editor::Instrument : public juce::Component, public juce::SettableTooltipClient
{
public:
    Instrument (At303Processor& p, At303Editor& e) : proc (p), ed (e)
    {
        bg = loadImg (BinaryData::bg_jpg, BinaryData::bg_jpgSize);
        down = loadImg (BinaryData::keysdown_jpg, BinaryData::keysdown_jpgSize);
        auto v = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::layout_json, BinaryData::layout_jsonSize));
        auto rect = [] (const juce::var& a) { return juce::Rectangle<float> ((float) a[0], (float) a[1], (float) a[2], (float) a[3]) * 0.5f; };
        for (auto& k : *v["knobs"].getArray())
            knobs.push_back ({ k["id"].toString(), juce::Point<float> ((float) k["c"][0], (float) k["c"][1]) * 0.5f, (float) k["r"] * 0.5f, k["size"].toString() == "big" });
        for (auto& pd : *v["pads"].getArray()) pads.push_back (rect (pd));
        for (auto& k : *v["keys"].getArray()) keys.push_back ({ (int) k["semi"], rect (k["rect"]), (bool) k["sharp"] });
        for (auto& b : *v["buttons"].getArray()) buttons.push_back ({ b["id"].toString(), rect (b["rect"]) });
        display = rect (v["display"]); sweep = rect (v["sweep"]); wave = rect (v["wave"]);
        static const std::map<juce::String, juce::String> tipsMap {
            { "cutoff", "Cutoff: where the filter sits. At half, with Resonance up, it rings near 500 Hz, as the factory set it." },
            { "resonance", "Resonance: the squelch. The filter loses bass as it rises, and accents sweep further with it up." },
            { "envmod", "Env Mod: how far the envelope sweeps the filter. Turning it up also lowers where the sweep starts, as on the original." },
            { "decay", "Decay: how long the filter sweep lasts on normal notes (200 ms to 2.5 s). Accented notes always use the shortest." },
            { "accent", "Accent: how much louder and brighter accented notes are. Repeated accents climb higher: the capacitor charges." },
            { "tuning", "Tuning: ±7 semitones." }, { "tempo", "Tempo of the internal clock (when not following the host)." }, { "volume", "Volume." } };
        for (auto& k : knobs)
        {
            auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            s->getProperties().set ("cap", k.big ? 1 : 2);
            s->setTooltip (tipsMap.count (k.id) ? tipsMap.at (k.id) : juce::String());
            atts.push_back (std::make_unique<SA> (proc.apvts, k.id, *s));
            const float sz = k.r * 2.0f + 24.0f;   // la tapa se renderiza con 4 mm de aire alrededor (3 px por mm)
            s->setBounds (juce::Rectangle<float> (sz, sz).withCentre (k.c).toNearestInt());
            addAndMakeVisible (*s); sliders.push_back (std::move (s));
        }
        setTooltip ("Pads: click to cycle note, tie, rest. Keys: play, or write pitches in Pitch mode. Pitch / Time: write the pattern the original way.");
    }
    bool inspect = false;
    void setInspect (bool on) { inspect = on; for (auto& sl : sliders) sl->setVisible (! on); repaint(); }
    int mode = 0;                 // 0 normal, 1 escribir alturas, 2 escribir tiempos
    int cursor = 0;
    bool nextUp = false, nextDown = false;

    void paint (juce::Graphics& g) override
    {
        g.drawImage (bg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        drawSilkscreen (g);
        auto& v = proc.view;
        const auto pat = proc.getPattern (proc.patternIndex());
        const int stepNow = v.step.load();
        // Pads: nota (blanco cálido), ligadura (ámbar tenue), silencio (apagado); acento rojo, slide con punto azul.
        const auto notes = compile (pat);
        std::vector<int> slotOf (16, -1); { int ptr = 0; for (int i = 0; i < pat.length; ++i) if (pat.time[(size_t) i] == kNote || (i == 0 && pat.time[0] == kTie)) slotOf[(size_t) i] = ptr++ % 16; }
        for (int i = 0; i < 16; ++i)
        {
            auto r = pads[(size_t) i].reduced (2.0f);
            if (i >= pat.length) { g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r, 3.0f); continue; }
            const auto t = pat.time[(size_t) i];
            juce::Colour c = t == kRest ? juce::Colours::transparentBlack : (t == kTie ? col::amber.withAlpha (0.28f) : col::amber.withAlpha (0.55f));
            if (t == kNote && slotOf[(size_t) i] >= 0 && pat.pitch[(size_t) slotOf[(size_t) i]].accent) c = col::red.withAlpha (0.7f);
            if (i == stepNow) c = c.isTransparent() ? juce::Colours::white.withAlpha (0.25f) : c.brighter (0.6f).withAlpha (0.95f);
            if (mode == 2 && i == cursor) { g.setColour (juce::Colours::white.withAlpha (0.9f)); g.drawRoundedRectangle (r.expanded (2), 3.0f, 1.5f); }
            if (! c.isTransparent())
            {
                juce::ColourGradient glow (c, r.getCentreX(), r.getCentreY(), c.withAlpha (0.0f), r.getCentreX(), r.getY() - 6, true);
                g.setGradientFill (glow); g.fillRoundedRectangle (r.expanded (5), 6.0f);
                g.setColour (c); g.fillRoundedRectangle (r, 3.0f);
            }
            if (t == kNote && slotOf[(size_t) i] >= 0 && pat.pitch[(size_t) slotOf[(size_t) i]].slide) { g.setColour (col::blue); g.fillEllipse (r.getRight() - 6, r.getY() + 2, 4, 4); }
        }
        // Teclas y botones apretados: el recorte del render con todo abajo.
        auto drawDown = [&] (juce::Rectangle<float> r) {
            const float k = (float) down.getWidth() / (float) getWidth();
            g.drawImage (down, juce::roundToInt (r.getX()), juce::roundToInt (r.getY()), juce::roundToInt (r.getWidth()), juce::roundToInt (r.getHeight()), juce::roundToInt (r.getX() * k), juce::roundToInt (r.getY() * k), juce::roundToInt (r.getWidth() * k), juce::roundToInt (r.getHeight() * k));
        };
        for (auto& k : keys) if (proc.keyboardState.isNoteOn (1, 36 + k.semi + 12) || pressedKey == k.semi) drawDown (k.r.expanded (2));
        for (auto& b : buttons)
        {
            if (b.id == pressedBtn) drawDown (b.r.expanded (2));
            const bool lit = (b.id == "pitch" && mode == 1) || (b.id == "time" && mode == 2) || (b.id == "run" && proc.value ("run") > 0.5f)
                             || (b.id == "up" && nextUp) || (b.id == "down" && nextDown);
            if (lit) { g.setColour (col::amber); g.fillEllipse (b.r.getRight() - 7, b.r.getY() + 3, 4, 4); }
        }
        // Llave de forma de onda.
        {
            const bool sq = (int) proc.value ("waveform") == 1;
            auto r = wave.reduced (wave.getWidth() * 0.32f, wave.getHeight() * 0.14f);
            auto lever = sq ? r.withTop (r.getCentreY()) : r.withBottom (r.getCentreY());
            juce::ColourGradient m (juce::Colour (0xffd9dadc), lever.getX(), lever.getY(), juce::Colour (0xff6f7275), lever.getRight(), lever.getBottom(), false);
            g.setGradientFill (m); g.fillRoundedRectangle (lever, 2.0f);
        }
        // Display ámbar (siete segmentos): tempo; en modo de escritura, el lugar.
        {
            juce::String txt;
            if (mode == 1) txt = "P" + juce::String (cursor + 1).paddedLeft (' ', 3);
            else if (mode == 2) txt = "t" + juce::String (cursor + 1).paddedLeft (' ', 3);
            else if (juce::Time::getMillisecondCounter() < patShownUntil) txt = juce::String (proc.patternIndex() / 16 + 1) + (((proc.patternIndex() / 8) & 1) ? "b" : "A") + " " + juce::String (proc.patternIndex() % 8 + 1);
            else txt = juce::String (juce::roundToInt (proc.value ("tempo"))).paddedLeft (' ', 4);
            drawSevenSeg (g, display.reduced (display.getWidth() * 0.08f, display.getHeight() * 0.18f), txt);
        }
        // Barra de carga del acento: la tensión de C13, en vivo.
        {
            const float c = juce::jlimit (0.0f, 1.0f, v.c13.load() / 2.2f);
            auto r = sweep.reduced (1.0f);
            g.setColour (col::amber.withAlpha (0.18f)); g.fillRoundedRectangle (r.withWidth (r.getWidth() * c).expanded (0, 3), 3.0f);
            g.setColour (col::amber); g.fillRoundedRectangle (r.withWidth (r.getWidth() * c), 1.5f);
        }
        if (inspect) paintInspect (g);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        for (int i = 0; i < 16; ++i)
            if (pads[(size_t) i].contains (p))
            {
                ed.editPattern ([i] (Pattern& pt) { if (i < pt.length) pt.time[(size_t) i] = (StepType) ((pt.time[(size_t) i] + 1) % 3); });
                return;
            }
        for (auto& k : keys)
            if (k.r.contains (p))
            {
                pressedKey = k.semi;
                if (mode == 1)
                {
                    const int c = cursor; const bool up = nextUp, dn = nextDown; const int semi = k.semi;
                    ed.editPattern ([c, up, dn, semi] (Pattern& pt) { auto& s = pt.pitch[(size_t) c]; s.note = (int8_t) semi; s.up = up; s.down = dn; });
                    cursor = (cursor + 1) % 16; nextUp = nextDown = false;
                }
                proc.keyboardState.noteOn (1, 36 + 12 + k.semi, 0.8f);
                repaint(); return;
            }
        for (auto& b : buttons)
            if (b.r.contains (p))
            {
                pressedBtn = b.id;
                button (b.id);
                repaint(); return;
            }
        if (wave.contains (p)) { proc.setValue ("waveform", (int) proc.value ("waveform") == 1 ? 0.0f : 1.0f); repaint(); }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (pressedKey >= 0) proc.keyboardState.noteOff (1, 36 + 12 + pressedKey, 0.0f);
        pressedKey = -1; pressedBtn.clear(); repaint();
    }
    void showPatternBriefly() { patShownUntil = juce::Time::getMillisecondCounter() + 1200; }

    // Repintado del temporizador: con Inspect, todo; si no, sólo lo que se mueve (pads, display, barra, botones y,
    // si cambió algo, las teclas). Repintar el render entero 30 veces por segundo costaba ~9 % de un núcleo.
    void tick()
    {
        if (inspect) { repaint(); return; }
        juce::Rectangle<float> row = pads.front();
        for (auto& r : pads) row = row.getUnion (r);
        for (auto r : { row, display, sweep, wave }) repaint (r.expanded (8).getSmallestIntegerContainer());
        juce::Rectangle<float> btn; for (auto& b : buttons) btn = btn.isEmpty() ? b.r : btn.getUnion (b.r);
        repaint (btn.expanded (4).getSmallestIntegerContainer());
        uint32_t mask = 0; for (auto& k : keys) if (proc.keyboardState.isNoteOn (1, 36 + k.semi + 12)) mask |= 1u << k.semi;
        if (mask != lastKeys) { lastKeys = mask; juce::Rectangle<float> kr; for (auto& k : keys) kr = kr.isEmpty() ? k.r : kr.getUnion (k.r); repaint (kr.expanded (4).getSmallestIntegerContainer()); }
    }

private:
    struct KnobPos { juce::String id; juce::Point<float> c; float r; bool big; };
    struct Key { int semi; juce::Rectangle<float> r; bool sharp; };
    struct Btn { juce::String id; juce::Rectangle<float> r; };
    At303Processor& proc; At303Editor& ed;
    juce::Image bg, down;
    std::vector<KnobPos> knobs; std::vector<juce::Rectangle<float>> pads; std::vector<Key> keys; std::vector<Btn> buttons;
    juce::Rectangle<float> display, sweep, wave;
    std::vector<std::unique_ptr<juce::Slider>> sliders; std::vector<std::unique_ptr<SA>> atts;
    int pressedKey = -1; juce::String pressedBtn; juce::uint32 patShownUntil = 0; uint32_t lastKeys = 0;
    std::array<float, 72> megHist {}, vegHist {}; int histPos = 0;

    // Serigrafía: todo el texto del panel, como texto vectorial (nítido a cualquier escala). Cada botón dice qué
    // hace, las teclas llevan su nota y las secciones están rotuladas.
    void drawSilkscreen (juce::Graphics& g)
    {
        const juce::Colour ink (0xffd9dcdf), inkDim (0xff9ca2a8), rule (0x30d9dcdf);
        auto font = [] (float h, bool bold = false) { return juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain); };
        // Título.
        g.setColour (ink); g.setFont (font (34.0f, true));
        g.drawText ("AT303", juce::Rectangle<float> (display.getX(), display.getY() - 64, 220, 36), juce::Justification::centredLeft);
        g.setColour (inkDim); g.setFont (font (13.0f));
        g.drawText ("Bass line synthesizer  /  Audio Tools", juce::Rectangle<float> (display.getX(), display.getY() - 30, 300, 18), juce::Justification::centredLeft);
        g.drawText ("TEMPO  /  PATTERN  /  STEP", display.withY (display.getBottom() + 4).withHeight (16).expanded (20, 0), juce::Justification::centred);
        // Secciones con su regla.
        auto section = [&] (juce::Rectangle<float> r, const juce::String& t) {
            g.setColour (rule); g.drawHorizontalLine ((int) r.getY() + 8, r.getX(), r.getRight());
            g.setFont (font (12.5f, true));
            const float w = juce::GlyphArrangement::getStringWidth (juce::Font (font (12.5f, true)), t) + 16;
            g.setColour (juce::Colour (0xff1b1d20)); g.fillRect (juce::Rectangle<float> (r.getX() + 10, r.getY(), w, 16));
            g.setColour (inkDim); g.drawText (t, juce::Rectangle<float> (r.getX() + 18, r.getY(), w, 16), juce::Justification::centredLeft); };
        juce::Rectangle<float> bigR; for (auto& k : knobs) if (k.big) bigR = bigR.isEmpty() ? juce::Rectangle<float> (k.r * 2, k.r * 2).withCentre (k.c) : bigR.getUnion (juce::Rectangle<float> (k.r * 2, k.r * 2).withCentre (k.c));
        section ({ bigR.getX() - 20, bigR.getY() - 46, bigR.getWidth() + 40, 16 }, "FILTER  /  ENVELOPE  /  ACCENT");
        section ({ wave.getX() - 26, bigR.getY() - 46, wave.getWidth() + 52, 16 }, "OSCILLATOR");
        juce::Rectangle<float> padR; for (auto& p : pads) padR = padR.isEmpty() ? p : padR.getUnion (p);
        section ({ padR.getX(), padR.getY() - 40, padR.getWidth(), 16 }, "STEPS  /  click: note, tie, rest");
        juce::Rectangle<float> btnR; for (auto& b : buttons) btnR = btnR.isEmpty() ? b.r : btnR.getUnion (b.r);
        section ({ btnR.getX() - 6, btnR.getY() - 40, btnR.getWidth() + 12, 16 }, "WRITE THE PATTERN");
        juce::Rectangle<float> keyR; for (auto& k : keys) keyR = keyR.isEmpty() ? k.r : keyR.getUnion (k.r);
        section ({ keyR.getX(), keyR.getY() - 34, keyR.getWidth(), 16 }, "KEYBOARD  /  play, or write pitches in Pitch mode");
        // Perillas.
        static const std::map<juce::String, std::pair<const char*, const char*>> kn {
            { "cutoff", { "CUTOFF", "brightness" } }, { "resonance", { "RESONANCE", "squelch" } }, { "envmod", { "ENV MOD", "sweep depth" } },
            { "decay", { "DECAY", "sweep length" } }, { "accent", { "ACCENT", "" } }, { "tuning", { "TUNING", "" } },
            { "tempo", { "TEMPO", "" } }, { "volume", { "VOLUME", "" } } };
        for (auto& k : knobs)
        {
            const auto it = kn.find (k.id); if (it == kn.end()) continue;
            const float y = k.c.y + k.r + (k.big ? 14.0f : 16.0f);
            g.setColour (ink); g.setFont (font (k.big ? 15.0f : 13.0f, true));
            g.drawText (it->second.first, juce::Rectangle<float> (k.c.x - 70, y, 140, 18), juce::Justification::centred);
            if (*it->second.second) { g.setColour (inkDim); g.setFont (font (12.0f)); g.drawText (it->second.second, juce::Rectangle<float> (k.c.x - 70, y + 17, 140, 15), juce::Justification::centred); }
        }
        g.setColour (ink); g.setFont (font (15.0f, true));
        g.drawText ("WAVE", wave.withY (wave.getBottom() + 8).withHeight (18).expanded (30, 0), juce::Justification::centred);
        g.setColour (inkDim); g.setFont (font (12.0f));
        g.drawText ("SAW", wave.withX (wave.getRight() + 4).withHeight (wave.getHeight() * 0.4f).withWidth (40), juce::Justification::centredLeft);
        g.drawText ("SQUARE", wave.withX (wave.getRight() + 4).withY (wave.getY() + wave.getHeight() * 0.6f).withHeight (wave.getHeight() * 0.4f).withWidth (60), juce::Justification::centredLeft);
        g.drawText ("ACCENT CHARGE", sweep.withY (sweep.getBottom() + 6).withHeight (15).expanded (40, 0), juce::Justification::centred);
        // Pasos: número.
        g.setFont (font (11.5f));
        for (size_t i = 0; i < pads.size(); ++i) g.drawText (juce::String ((int) i + 1), pads[i].withY (pads[i].getY() - 18).withHeight (14), juce::Justification::centred);
        // Botones: nombre y para qué sirve.
        static const std::map<juce::String, std::pair<const char*, const char*>> bt {
            { "pitch", { "PITCH", "write notes" } }, { "time", { "TIME", "write rhythm" } }, { "run", { "RUN", "start / stop" } },
            { "clear", { "CLEAR", "erase pattern" } }, { "down", { "DOWN", "oct. down / note" } }, { "up", { "UP", "oct. up / tie" } },
            { "acc", { "ACCENT", "accent / rest" } }, { "slide", { "SLIDE", "glide to next" } } };
        for (auto& b : buttons)
        {
            const auto it = bt.find (b.id); if (it == bt.end()) continue;
            // El nombre arriba; para qué sirve, sobre la tecla misma.
            g.setColour (ink); g.setFont (font (12.5f, true));
            g.drawText (it->second.first, juce::Rectangle<float> (b.r.getX() - 10, b.r.getY() - 19, b.r.getWidth() + 20, 16), juce::Justification::centred);
            g.setColour (juce::Colour (0xff2b2e32)); g.setFont (font (11.0f, true));
            g.drawText (it->second.second, b.r.reduced (2, 0), juce::Justification::centred);
        }
        // Teclas: la nota.
        static const char* nn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };
        for (auto& k : keys)
        {
            g.setColour (k.sharp ? inkDim : juce::Colour (0xff3a3d41)); g.setFont (font (14.0f, true));
            g.drawText (nn[juce::jlimit (0, 12, k.semi)], k.r.withTrimmedTop (k.r.getHeight() - 24).withTrimmedBottom (6), juce::Justification::centred);
        }
    }

    void button (const juce::String& id)
    {
        if (id == "pitch") { mode = mode == 1 ? 0 : 1; cursor = 0; }
        else if (id == "time") { mode = mode == 2 ? 0 : 2; cursor = 0; }
        else if (id == "run") proc.setValue ("run", proc.value ("run") > 0.5f ? 0.0f : 1.0f);
        else if (id == "clear") proc.clearPattern();
        else if (mode == 2 && (id == "down" || id == "up" || id == "acc"))
        {
            // Escritura de tiempos (como la caja moderna): Down = nota, Up = ligadura, Accent = silencio.
            const StepType t = id == "down" ? kNote : (id == "up" ? kTie : kRest);
            const int c = cursor;
            ed.editPattern ([c, t] (Pattern& pt) { pt.time[(size_t) c] = t; });
            cursor = (cursor + 1) % std::max (1, proc.getPattern (proc.patternIndex()).length);
        }
        else if (id == "up") { nextUp = ! nextUp; nextDown = false; }
        else if (id == "down") { nextDown = ! nextDown; nextUp = false; }
        else if (mode == 1 && (id == "acc" || id == "slide"))          // acento y slide sólo al escribir alturas
        {
            const int c = (cursor + 15) % 16; const bool acc = id == "acc";
            ed.editPattern ([c, acc] (Pattern& pt) { auto& s = pt.pitch[(size_t) c]; if (acc) s.accent = ! s.accent; else s.slide = ! s.slide; });
        }
    }

    void drawSevenSeg (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& txt)
    {
        // Segmentos a, b, c, d, e, f, g por carácter.
        auto segs = [] (juce::juce_wchar ch) -> int {
            switch (ch) { case '0': return 0x3F; case '1': return 0x06; case '2': return 0x5B; case '3': return 0x4F; case '4': return 0x66; case '5': return 0x6D;
                          case '6': return 0x7D; case '7': return 0x07; case '8': return 0x7F; case '9': return 0x6F; case 'A': return 0x77; case 'b': return 0x7C;
                          case 'P': return 0x73; case 't': return 0x78; case '-': return 0x40; default: return 0; }
        };
        const int n = 4; const float cw = r.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            const auto ch = i < txt.length() ? txt[i] : ' ';
            const int m = segs (ch);
            auto c = juce::Rectangle<float> (r.getX() + cw * i, r.getY(), cw, r.getHeight()).reduced (cw * 0.16f, 0);
            const float w = c.getWidth(), h = c.getHeight(), t = std::max (2.0f, w * 0.14f), sk = w * 0.12f;
            auto seg = [&] (int bit, float x0, float y0, float x1, float y1) {
                const bool on = (m >> bit) & 1;
                g.setColour (on ? col::amber : col::amberDim.withAlpha (0.35f));
                juce::Line<float> L (c.getX() + x0 - sk * (y0 / h - 0.5f), c.getY() + y0, c.getX() + x1 - sk * (y1 / h - 0.5f), c.getY() + y1);
                g.drawLine (L, t);
                if (on) { g.setColour (col::amber.withAlpha (0.15f)); g.drawLine (L, t * 3.0f); }
            };
            seg (0, t, 0, w - t, 0); seg (1, w, t, w, h * 0.5f - t * 0.5f); seg (2, w, h * 0.5f + t * 0.5f, w, h - t); seg (3, t, h, w - t, h);
            seg (4, 0, h * 0.5f + t * 0.5f, 0, h - t); seg (5, 0, t, 0, h * 0.5f - t * 0.5f); seg (6, t, h * 0.5f, w - t, h * 0.5f);
        }
    }

    // El circuito en vivo sobre el render oscurecido.
    void paintInspect (juce::Graphics& g)
    {
        auto& v = proc.view;
        g.setColour (juce::Colour (0xe0101214)); g.fillRect (getLocalBounds());
        auto b = getLocalBounds().toFloat().reduced (40, 50);
        g.setColour (col::inkDim); g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Inspect: the circuit, live", b.removeFromTop (24), juce::Justification::centredLeft);
        // Cadena: VCO → escalera (4 capacitores) → VCA → salida.
        const float y = b.getY() + 120;
        auto box = [&] (float x, float w, const juce::String& t) { juce::Rectangle<float> r (x, y - 44, w, 88); g.setColour (col::line); g.drawRoundedRectangle (r, 6, 1.5f); g.setColour (col::ink); g.drawText (t, r.withY (r.getBottom() + 6).withHeight (18).expanded (80, 0), juce::Justification::centred); return r; };
        auto vco = box (b.getX(), 120, "Ramp core  CV " + juce::String (v.cv.load(), 2) + " V");
        auto lad = box (b.getX() + 200, 380, "Diode ladder  k = " + juce::String (v.k.load(), 1) + "  ring " + juce::String (juce::roundToInt (v.cutoffHz.load())) + " Hz");
        auto vca = box (b.getX() + 660, 120, "VCA");
        g.setColour (col::inkFaint); g.drawArrow ({ vco.getRight(), y, lad.getX(), y }, 1.5f, 8, 8); g.drawArrow ({ lad.getRight(), y, vca.getX(), y }, 1.5f, 8, 8);
        // Capacitores de la escalera: la altura del relleno es el estado.
        for (int i = 0; i < 4; ++i)
        {
            const float s = std::tanh (v.ladder[(size_t) i].load() * 2.5f);
            auto c = juce::Rectangle<float> (lad.getX() + 30 + 90.0f * (float) i, y - 34, 40, 50);
            g.setColour (col::line); g.drawRect (c, 1.0f);
            auto f = s >= 0 ? c.withTop (c.getCentreY() - s * c.getHeight() * 0.5f).withBottom (c.getCentreY()) : c.withTop (c.getCentreY()).withBottom (c.getCentreY() - s * c.getHeight() * 0.5f);
            g.setColour (col::amber.withAlpha (0.85f)); g.fillRect (f);
            g.setColour (col::inkFaint); g.setFont (juce::FontOptions (12.0f)); g.drawText (i == 0 ? juce::String ("0.018 uF") : juce::String ("0.033 uF"), c.withY (c.getBottom() + 4).withHeight (14).expanded (20, 0), juce::Justification::centred);
        }
        // VCA: la corriente de control.
        g.setColour (col::green.withAlpha (0.8f)); g.fillRect (vca.reduced (12).withTop (vca.getBottom() - 12 - (vca.getHeight() - 24) * juce::jlimit (0.0f, 1.0f, v.vca.load() / 1.5f)));
        // Envolventes: historia de la MEG y la VEG; C13.
        megHist[(size_t) histPos] = v.meg.load(); vegHist[(size_t) histPos] = v.veg.load(); histPos = (histPos + 1) % (int) megHist.size();
        g.setFont (juce::FontOptions (15.0f));
        auto scope = juce::Rectangle<float> (b.getX(), y + 120, 420, 120);
        g.setColour (col::line); g.drawRect (scope, 1.0f);
        auto trace = [&] (const std::array<float, 72>& h, juce::Colour c) {
            juce::Path p; for (int i = 0; i < 72; ++i) { const float xv = scope.getX() + scope.getWidth() * (float) i / 71.0f, yv = scope.getBottom() - 4 - (scope.getHeight() - 8) * juce::jlimit (0.0f, 1.0f, h[(size_t) ((histPos + i) % 72)]); if (i == 0) p.startNewSubPath (xv, yv); else p.lineTo (xv, yv); }
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.6f)); };
        trace (megHist, col::amber); trace (vegHist, col::green);
        g.setColour (col::inkDim); g.drawText ("MEG (filter)  VEG (VCA)", scope.withY (scope.getBottom() + 4).withHeight (16), juce::Justification::centredLeft);
        auto c13 = juce::Rectangle<float> (scope.getRight() + 60, scope.getY(), 60, scope.getHeight());
        g.setColour (col::line); g.drawRect (c13, 1.0f);
        g.setColour (col::amber); g.fillRect (c13.reduced (6).withTop (c13.getBottom() - 6 - (c13.getHeight() - 12) * juce::jlimit (0.0f, 1.0f, v.c13.load() / 2.2f)));
        g.setColour (col::inkDim); g.drawText ("C13 " + juce::String (v.c13.load(), 2) + " V", c13.withY (c13.getBottom() + 4).withHeight (16).expanded (40, 0), juce::Justification::centred);
        g.setColour (col::inkFaint); g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText ("Each accent charges C13 through a diode and 47 k. It only drains through the Resonance pot, so a run of accents climbs: the fourth one screams.",
                          juce::Rectangle<int> ((int) c13.getRight() + 50, (int) scope.getY(), 300, 120), juce::Justification::topLeft, 6);
    }
};

// ── Las dos listas del original: tiempos por paso y alturas por orden, con la línea de qué altura usa cada paso ──
class At303Editor::LaneGrid : public juce::Component
{
public:
    explicit LaneGrid (At303Editor& e) : ed (e) {}
    std::function<Pattern()> getPattern;
    int step = -1;
    void paint (juce::Graphics& g) override
    {
        const auto p = getPattern();
        auto b = getLocalBounds().toFloat();
        const float lab = 60, cw = (b.getWidth() - lab) / 16.0f;
        g.setColour (col::inkDim); g.setFont (juce::FontOptions (12.5f));
        g.drawText ("Time", 0, 0, (int) lab, 26, juce::Justification::centredLeft);
        g.drawText ("Pitch", 0, 70, (int) lab, 40, juce::Justification::centredLeft);
        g.drawText ("Up / Down", 0, 116, (int) lab, 16, juce::Justification::centredLeft);
        g.drawText ("Accent", 0, 136, (int) lab, 16, juce::Justification::centredLeft);
        g.drawText ("Slide", 0, 156, (int) lab, 16, juce::Justification::centredLeft);
        int ptr = 0;
        for (int i = 0; i < 16; ++i)
        {
            const float x = lab + cw * (float) i;
            auto r = juce::Rectangle<float> (x + 2, 2, cw - 4, 24);
            const bool in = i < p.length;
            const auto t = p.time[(size_t) i];
            g.setColour (! in ? col::panel : t == kNote ? col::amber.withAlpha (0.8f) : t == kTie ? col::amber.withAlpha (0.35f) : col::panel2);
            g.fillRoundedRectangle (r, 3.0f);
            if (i == step) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (r, 3.0f, 1.5f); }
            g.setColour (in ? col::bg : col::inkFaint); g.drawText (! in ? "" : t == kNote ? "N" : t == kTie ? "T" : "-", r, juce::Justification::centred);
            // Línea del paso a la altura que consume.
            if (in && (t == kNote || (i == 0 && t == kTie)))
            {
                const int slot = ptr++ % 16;
                g.setColour (col::inkFaint.withAlpha (0.8f));
                g.drawLine (x + cw * 0.5f, 28, lab + cw * ((float) slot + 0.5f), 70, 1.0f);
            }
            // Altura (arrastrar para cambiar) y banderas.
            const auto& s = p.pitch[(size_t) i];
            auto pr = juce::Rectangle<float> (x + 2, 70, cw - 4, 40);
            g.setColour (col::panel2); g.fillRoundedRectangle (pr, 3.0f);
            g.setColour (col::ink); g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            g.drawText (juce::String (noteNames[juce::jlimit (0, 12, (int) s.note)]) + (s.up ? "+" : s.down ? "-" : ""), pr, juce::Justification::centred);
            g.setFont (juce::FontOptions (12.5f));
            auto flag = [&] (float yy, bool on, juce::Colour c, const juce::String& t2) { auto fr = juce::Rectangle<float> (x + 4, yy, cw - 8, 16); g.setColour (on ? c : col::panel2); g.fillRoundedRectangle (fr, 3.0f); g.setColour (on ? col::bg : col::inkFaint); g.drawText (t2, fr, juce::Justification::centred); };
            flag (116, s.up || s.down, col::inkDim, s.up ? "up" : s.down ? "down" : "");
            flag (136, s.accent, col::red, "A");
            flag (156, s.slide, col::blue, "S");
        }
        g.setColour (col::inkFaint);
        g.drawText ("Each N plays the next pitch: change the rhythm and the notes move along.", juce::Rectangle<float> (lab, 178, b.getWidth() - lab, 18), juce::Justification::centredLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const float lab = 60, cw = ((float) getWidth() - lab) / 16.0f;
        if (e.position.x < lab) return;                                  // las etiquetas de los renglones no editan
        const int i = (int) ((e.position.x - lab) / cw);
        if (i < 0 || i > 15) return;
        const float y = e.position.y;
        dragSlot = -1;
        if (y < 28) ed.editPattern ([i] (Pattern& p) { if (i < p.length) p.time[(size_t) i] = (StepType) ((p.time[(size_t) i] + 1) % 3); });
        else if (y >= 70 && y < 110) { dragSlot = i; dragY = y; dragNote = getPattern().pitch[(size_t) i].note; }
        else if (y >= 116 && y < 132) ed.editPattern ([i] (Pattern& p) { auto& s = p.pitch[(size_t) i]; if (! s.up && ! s.down) s.up = true; else if (s.up) { s.up = false; s.down = true; } else s.down = false; });
        else if (y >= 136 && y < 152) ed.editPattern ([i] (Pattern& p) { p.pitch[(size_t) i].accent = ! p.pitch[(size_t) i].accent; });
        else if (y >= 156 && y < 172) ed.editPattern ([i] (Pattern& p) { p.pitch[(size_t) i].slide = ! p.pitch[(size_t) i].slide; });
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragSlot < 0) return;
        const int n = juce::jlimit (0, 12, dragNote + (int) std::round ((dragY - e.position.y) / 8.0f));
        const int s = dragSlot;
        ed.editPattern ([s, n] (Pattern& p) { p.pitch[(size_t) s].note = (int8_t) n; });
    }
private:
    At303Editor& ed; int dragSlot = -1, dragNote = 0; float dragY = 0;
};

At303Editor::At303Editor (At303Processor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf = std::make_unique<Look>();
    setLookAndFeel (lnf.get());
    content.painter = [this] (juce::Graphics& g) { paintPanel (g); };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    inst = std::make_unique<Instrument> (proc, *this);
    content.addAndMakeVisible (*inst);
    grid = std::make_unique<LaneGrid> (*this);
    grid->getPattern = [this] { return proc.getPattern (proc.patternIndex()); };
    content.addAndMakeVisible (*grid);

    static const char* groups[] = { "I", "II", "III", "IV" };
    for (int i = 0; i < 4; ++i) { auto* b = groupBtns.add (new juce::TextButton (groups[i])); b->onClick = [this, i] { const int pi = proc.patternIndex(); proc.setValue ("pattern", (float) (i * 16 + (pi % 16))); inst->showPatternBriefly(); }; content.addAndMakeVisible (b); }
    for (int i = 0; i < 2; ++i) { auto* b = abBtns.add (new juce::TextButton (i == 0 ? "A" : "B")); b->onClick = [this, i] { const int pi = proc.patternIndex(); proc.setValue ("pattern", (float) ((pi / 16) * 16 + i * 8 + pi % 8)); inst->showPatternBriefly(); }; content.addAndMakeVisible (b); }
    for (int i = 0; i < 8; ++i) { auto* b = numBtns.add (new juce::TextButton (juce::String (i + 1))); b->onClick = [this, i] { const int pi = proc.patternIndex(); proc.setValue ("pattern", (float) ((pi / 8) * 8 + i)); inst->showPatternBriefly(); }; content.addAndMakeVisible (b); }
    static const char* tabs[] = { "Generator", "Extended", "Output" };
    static const char* tabIds[] = { "generate", "extended", "output" };
    for (int i = 0; i < 3; ++i) { auto* b = tabBtns.add (new juce::TextButton (tabs[i])); juce::String id = tabIds[i]; b->onClick = [this, id] { tab = id; buildDetail(); }; content.addAndMakeVisible (b); }

    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &manualBtn, &inspectBtn, &modeBtn, &syncBtn, &tripBtn, &lenDown, &lenUp, &genBtn, &mutBtn, &stockBtn }) content.addAndMakeVisible (b);
    prevBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.loadPreset ((proc.getCurrentProgram() - 1 + n) % n); };
    nextBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.loadPreset ((proc.getCurrentProgram() + 1) % n); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    manualBtn.onClick = [this] { showManual (manual == nullptr); };
   #if ! DH_HAS_MANUAL
    manualBtn.setVisible (false);
   #endif
    inspectBtn.setClickingTogglesState (true);
    inspectBtn.onClick = [this] { inst->setInspect (inspectBtn.getToggleState()); };
    inspectBtn.setTooltip ("Inspect: fade the instrument into its circuit, live: the diode ladder's capacitors, the accent capacitor C13 and the two envelopes.");
    modeBtn.setClickingTogglesState (true);
    modeBtn.onClick = [this] { proc.setValue ("mode", modeBtn.getToggleState() ? 1.0f : 0.0f); };
    modeBtn.setTooltip ("MIDI mode: play it from a keyboard or the piano roll. Velocity 100 and up is an accent; overlapping notes slide.");
    syncBtn.setClickingTogglesState (true);
    syncAtt = std::make_unique<BA> (proc.apvts, "sync", syncBtn);
    syncBtn.setTooltip ("Follow host: the sequencer runs with the host's transport and tempo. Off: Run and Tempo on the instrument.");
    tripBtn.setClickingTogglesState (true);
    tripBtn.onClick = [this] { const bool on = tripBtn.getToggleState(); editPattern ([on] (Pattern& pt) { pt.triplet = on; if (on) pt.length = std::min (pt.length, 15); }); };
    tripBtn.setTooltip ("Triplets: three steps per beat (eight clocks per step); up to 15 steps.");
    lenDown.onClick = [this] { editPattern ([] (Pattern& pt) { pt.length = std::max (1, pt.length - 1); }); };
    lenUp.onClick = [this] { editPattern ([] (Pattern& pt) { pt.length = std::min (pt.triplet ? 15 : 16, pt.length + 1); }); };
    genBtn.onClick = [this] { proc.generate (false); };
    genBtn.setTooltip ("Generate: a new acid line in the chosen root and scale, with these densities. Each press is a new seed.");
    mutBtn.onClick = [this] { proc.generate (true); };
    mutBtn.setTooltip ("Mutate: keep the rhythm, redraw the notes, accents and slides.");
    stockBtn.onClick = [this] {
        for (auto* id : { "overdrive", "normal_decay", "accent_decay", "soft_attack", "slide_time", "tracking", "filter_fm", "env_range", "cutoff_top", "vca_click", "sweep_speed", "sweep_on", "muffler", "high_res", "env_zero", "bass_fix", "note_delay" })
            if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->getDefaultValue());
    };
    stockBtn.setTooltip ("Stock 303: every extension back to the original circuit.");

    proc.onPatternsChanged = [this] { grid->repaint(); inst->repaint(); content.repaint (panelR.withWidth (330)); };
    proc.onPresetChanged = [this] { presetBtn.setButtonText (proc.presetDisplayName()); grid->repaint(); content.repaint(); };
    presetBtn.setButtonText (proc.presetDisplayName());
    buildDetail();
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.4), (int) (baseH * 1.4));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.74), (int) (baseH * 0.74));
    startTimerHz (30);

    // Capturas sin pantalla: AT303_SNAPSHOT=/ruta.png, AT303_PRESET=n, AT303_WAIT=s, AT303_TAB=..., AT303_INSPECT=1, AT303_RUN=1.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("AT303_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
        const int preset = env ("AT303_PRESET", "-1").getIntValue(), wait = env ("AT303_WAIT", "4").getIntValue();
        const auto tb = env ("AT303_TAB", ""); const bool insp = env ("AT303_INSPECT", "0").getIntValue() != 0, run = env ("AT303_RUN", "0").getIntValue() != 0;
        const bool man = env ("AT303_MANUAL", "0").getIntValue() != 0;
        juce::Component::SafePointer<At303Editor> safe (this);
        juce::Timer::callAfterDelay (300, [safe, preset, tb, insp, run, man] {
            if (safe == nullptr) return;
            if (preset >= 0) safe->proc.loadPreset (preset);
            if (tb.isNotEmpty()) { safe->tab = tb; safe->buildDetail(); }
            if (insp) { safe->inspectBtn.setToggleState (true, juce::sendNotification); }
            if (run) { safe->proc.setValue ("sync", 0.0f); safe->proc.setValue ("run", 1.0f); }
            if (man) safe->showManual (true);
        });
        juce::Timer::callAfterDelay (300 + wait * 1000, [safe, snap, man] {
            if (safe == nullptr) return;
            juce::Component& shot = man && safe->manual != nullptr ? (juce::Component&) *safe : (juce::Component&) safe->content;
            const auto img = shot.createComponentSnapshot (shot.getLocalBounds(), true, man ? 2.0f * (float) baseW / (float) safe->getWidth() : 2.0f);
            juce::File f (snap); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

At303Editor::~At303Editor() { proc.onPatternsChanged = nullptr; proc.onPresetChanged = nullptr; manual.reset(); setLookAndFeel (nullptr); }

void At303Editor::editPattern (std::function<void (Pattern&)> f)
{
    auto p = proc.getPattern (proc.patternIndex());
    f (p);
    proc.setPattern (proc.patternIndex(), p);
    grid->repaint(); inst->repaint(); content.repaint (panelR.withWidth (330));
}

juce::String At303Editor::valueText (const juce::String& id) const
{
    const float v = proc.value (id);
    if (id == "normal_decay") return v < 1 ? juce::String ("Knob") : juce::String (juce::roundToInt (v)) + " ms";
    if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (proc.apvts.getParameter (id)))
        return (std::abs (v) < 10 && pf->getNormalisableRange().end <= 30 ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + (pf->getLabel().isNotEmpty() ? " " + pf->getLabel() : juce::String());
    if (auto* pr = proc.apvts.getParameter (id)) return pr->getCurrentValueAsText();
    return {};
}

void At303Editor::buildDetail()
{
    struct D { const char* id; const char* title; int kind; };
    static const std::vector<D> gen { { "gen_root", "Root", 1 }, { "gen_scale", "Scale", 1 }, { "gen_octaves", "Octaves", 1 }, { "gen_density", "Density", 0 }, { "gen_accent", "Accents", 0 }, { "gen_slide", "Slides", 0 }, { "gen_ties", "Ties", 0 } };
    static const std::vector<D> ext { { "overdrive", "Overdrive", 0 }, { "normal_decay", "Normal decay", 0 }, { "accent_decay", "Accent decay", 0 }, { "soft_attack", "Soft attack", 0 }, { "slide_time", "Slide time", 0 },
                                      { "tracking", "Tracking", 0 }, { "filter_fm", "Filter FM", 0 }, { "env_range", "Env range", 0 }, { "cutoff_top", "Cutoff top", 0 }, { "vca_click", "VCA click", 0 },
                                      { "sweep_speed", "Sweep", 1 }, { "muffler", "Muffler", 1 }, { "high_res", "High reso", 2 }, { "env_zero", "Env to 0", 2 }, { "bass_fix", "Full bass", 2 }, { "note_delay", "4 ms delay", 2 } };
    static const std::vector<D> out { { "drive_type", "Drive", 1 }, { "drive", "Amount", 0 }, { "drive_tone", "Tone", 0 }, { "delay_mix", "Delay", 0 }, { "delay_time", "Time", 1 }, { "delay_fb", "Feedback", 0 }, { "delay_tone", "Delay tone", 0 },
                                      { "transpose", "Transpose", 0 }, { "accent_vel", "Accent vel.", 0 }, { "quality", "Quality", 1 } };
    detail.clear();
    for (int i = 0; i < tabBtns.size(); ++i) tabBtns[i]->setToggleState ((i == 0 && tab == "generate") || (i == 1 && tab == "extended") || (i == 2 && tab == "output"), juce::dontSendNotification);
    genBtn.setVisible (tab == "generate"); mutBtn.setVisible (tab == "generate"); stockBtn.setVisible (tab == "extended");
    const auto& defs = tab == "generate" ? gen : tab == "extended" ? ext : out;
    for (auto& d : defs)
    {
        auto c = std::make_unique<Control>(); c->id = d.id; c->title = d.title; c->kind = d.kind;
        if (d.kind == 0)
        {
            c->s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            c->s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            c->sa = std::make_unique<SA> (proc.apvts, d.id, *c->s);
            c->s->onValueChange = [this] { content.repaint (detailR); };
            content.addAndMakeVisible (*c->s);
        }
        else if (d.kind == 1)
        {
            c->c = std::make_unique<juce::ComboBox>();
            if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (d.id))) c->c->addItemList (pc->choices, 1);
            c->ca = std::make_unique<CA> (proc.apvts, d.id, *c->c);
            content.addAndMakeVisible (*c->c);
        }
        else
        {
            c->b = std::make_unique<juce::TextButton> (d.title); c->b->setClickingTogglesState (true);
            c->ba = std::make_unique<BA> (proc.apvts, d.id, *c->b);
            content.addAndMakeVisible (*c->b);
        }
        detail.push_back (std::move (c));
    }
    layoutDetail();
    content.repaint();
}

void At303Editor::layoutDetail()
{
    const int n = (int) detail.size(); if (n == 0) return;
    const int cols = 4, rows = (n + cols - 1) / cols;
    const int cw = detailR.getWidth() / cols, rh = (detailR.getHeight() - 8) / rows;
    for (int i = 0; i < n; ++i)
    {
        auto& c = *detail[(size_t) i];
        const auto cell = juce::Rectangle<int> (detailR.getX() + (i % cols) * cw, detailR.getY() + 8 + (i / cols) * rh, cw, rh);
        const int ks = std::min (40, rh - 30);
        if (c.s) c.s->setBounds (cell.withSizeKeepingCentre (ks, ks).withY (cell.getY() + 2));
        if (c.c) c.c->setBounds (cell.withSizeKeepingCentre (cw - 8, 24).withY (cell.getY() + 8));
        if (c.b) c.b->setBounds (cell.withSizeKeepingCentre (cw - 8, 24).withY (cell.getY() + 8));
    }
}

void At303Editor::showPresetMenu()
{
    juce::PopupMenu menu; std::map<juce::String, juce::PopupMenu> sub; juce::StringArray order;
    const auto& ps = proc.getPresets();
    for (int i = 0; i < (int) ps.size(); ++i)
    {
        if (! order.contains (ps[(size_t) i].category)) order.add (ps[(size_t) i].category);
        sub[ps[(size_t) i].category].addItem (i + 1, ps[(size_t) i].name, true, i == proc.getCurrentProgram());
    }
    for (auto& c : order) menu.addSubMenu (c, sub[c]);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [this] (int r) { if (r > 0) proc.loadPreset (r - 1); });
}

void At303Editor::showManual (bool on)
{
   #if DH_HAS_MANUAL
    if (! on) { manual.reset(); manualBtn.setToggleState (false, juce::dontSendNotification); return; }
    dh::manual::Theme t; t.bg = col::bg; t.panel = col::panel; t.text = col::ink; t.muted = col::inkDim; t.accent = col::amber; t.line = col::line;
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<At303Editor> (this)] { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*m); m->setBounds (getLocalBounds()); m->grabKeyboardFocus();
    manual = std::move (m); manualBtn.setToggleState (true, juce::dontSendNotification);
   #else
    juce::ignoreUnused (on);
   #endif
}

void At303Editor::timerCallback()
{
    // Live esconde la ventana del editor sin cerrarlo: escondido no se repinta nada (QA en Live 2026-10-22, H3: 16 %
    // de un núcleo). Todo lo de abajo se recalcula en cada tic, así que al volver a la vista aparece lo que cambió.
    if (! dh::reallyShowing (*this)) return;
    if (const auto name = proc.presetDisplayName(); name != presetBtn.getButtonText()) presetBtn.setButtonText (name);
    const int pi = proc.patternIndex();
    for (int i = 0; i < 4; ++i) groupBtns[i]->setToggleState (pi / 16 == i, juce::dontSendNotification);
    for (int i = 0; i < 2; ++i) abBtns[i]->setToggleState ((pi / 8) % 2 == i, juce::dontSendNotification);
    for (int i = 0; i < 8; ++i) numBtns[i]->setToggleState (pi % 8 == i, juce::dontSendNotification);
    modeBtn.setToggleState ((int) proc.value ("mode") == 1, juce::dontSendNotification);
    tripBtn.setToggleState (proc.getPattern (pi).triplet, juce::dontSendNotification);
    const int st = proc.view.step.load();
    if (st != grid->step) { grid->step = st; grid->repaint(); }
    inst->tick();
    if (pi != lastPat) { lastPat = pi; content.repaint (patR); content.repaint (panelR.withTrimmedTop (180).withWidth (330)); }
}

void At303Editor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void At303Editor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    inst->setBounds (0, 0, baseW, instH);
    panelR = { 0, instH, baseW, baseH - instH };
    const int y0 = instH + 12;
    prevBtn.setBounds (20, y0, 30, 28); presetBtn.setBounds (54, y0, 230, 28); nextBtn.setBounds (288, y0, 30, 28);
    manualBtn.setBounds (20, y0 + 36, 96, 28); inspectBtn.setBounds (122, y0 + 36, 96, 28);
    patR = { 20, y0 + 76, 300, 160 };
    for (int i = 0; i < 4; ++i) groupBtns[i]->setBounds (20 + i * 50, y0 + 96, 46, 26);
    for (int i = 0; i < 2; ++i) abBtns[i]->setBounds (230 + i * 46, y0 + 96, 42, 26);
    for (int i = 0; i < 8; ++i) numBtns[i]->setBounds (20 + i * 37, y0 + 128, 34, 26);
    modeBtn.setBounds (20, y0 + 168, 96, 26); syncBtn.setBounds (122, y0 + 168, 104, 26); tripBtn.setBounds (232, y0 + 168, 86, 26);
    lenDown.setBounds (20, y0 + 202, 30, 26); lenUp.setBounds (130, y0 + 202, 30, 26);
    gridR = { 340, y0, 560, 200 };
    grid->setBounds (gridR);
    const int dx = 920;
    for (int i = 0; i < 3; ++i) tabBtns[i]->setBounds (dx + i * 108, y0, 102, 28);
    detailR = { dx, y0 + 30, baseW - dx - 14, baseH - y0 - 78 };
    genBtn.setBounds (dx, baseH - 40, 100, 28); mutBtn.setBounds (dx + 108, baseH - 40, 100, 28); stockBtn.setBounds (dx, baseH - 40, 120, 28);
    layoutDetail();
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void At303Editor::paintPanel (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::panel); g.fillRect (panelR);
    g.setColour (col::line); g.fillRect (panelR.getX(), panelR.getY(), panelR.getWidth(), 1);
    g.setColour (col::inkDim); g.setFont (juce::FontOptions (12.5f));
    const int y0 = instH + 12;
    g.drawText ("Pattern " + params::patternName (proc.patternIndex()), 20, y0 + 76, 200, 18, juce::Justification::centredLeft);
    const auto p = proc.getPattern (proc.patternIndex());
    g.drawText ("Length " + juce::String (p.length), 54, y0 + 202, 72, 26, juce::Justification::centred);
    for (auto& c : detail)
    {
        juce::Rectangle<int> r;
        if (c->s) r = c->s->getBounds(); else if (c->c) r = c->c->getBounds(); else continue;
        g.setColour (col::inkDim); g.setFont (juce::FontOptions (12.0f));
        g.drawText (c->title, r.getCentreX() - 50, r.getBottom() + 1, 100, 14, juce::Justification::centred);
        if (c->kind == 0) { g.setColour (col::ink); g.drawText (valueText (c->id), r.getCentreX() - 50, r.getBottom() + 14, 100, 14, juce::Justification::centred); }
    }
}
