#include "PluginEditor.h"
#include "../../../shared/ui/Showing.h"

using namespace dh::pl;

namespace {
namespace col {
    const juce::Colour chalk (0xffe9e4d8), chalkDim (0xff9a9c9c), chalkFaint (0xff5c6064), brass (0xffd9a85b), brassDim (0xff8c6a38),
                       slate (0xff1d2023), slateDeep (0xff15171a), red (0xffe0745a), line (0xff3a3f44);
}
inline juce::Image loadImg (const char* data, int size) { return juce::ImageCache::getFromMemory (data, size); }
const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
juce::String noteName (float midi) { const int m = (int) std::lround (midi); return juce::String (noteNames[(m % 12 + 12) % 12]) + juce::String (m / 12 - 1); }
// Escala de la imagen: el render es la ventana a 2x; la pizarra mide 0,612 m de ancho → 1960,8 px por metro a 1x.
constexpr float kPxPerM = 1200.0f / 0.612f;
const juce::Point<float> kPivot (158.8f, 84.3f);
}

// ── Estilo: perillas de bronce (tapa quieta, índice que gira), botones grabados en la piedra ──
struct PlumbEditor::Look : public juce::LookAndFeel_V4
{
    juce::Image cap;
    Look()
    {
        cap = loadImg (BinaryData::knob_png, BinaryData::knob_pngSize);
        setColour (juce::PopupMenu::backgroundColourId, col::slateDeep); setColour (juce::PopupMenu::textColourId, col::chalk);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::brass); setColour (juce::PopupMenu::highlightedTextColourId, col::slateDeep);
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack); setColour (juce::ComboBox::textColourId, col::chalk);
        setColour (juce::ComboBox::outlineColourId, col::line); setColour (juce::ComboBox::arrowColourId, col::chalkDim);
        setColour (juce::TooltipWindow::backgroundColourId, col::chalk); setColour (juce::TooltipWindow::textColourId, col::slateDeep);
        setColour (juce::TextButton::textColourOffId, col::chalk); setColour (juce::TextButton::textColourOnId, col::slateDeep);
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const float ang = a0 + pos * (a1 - a0);
        const float sz = (float) std::min (w, h), cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
        const float r = sz * 0.5f * 0.82f;
        // Arco grabado del recorrido y la parte recorrida en tiza.
        juce::Path tr; tr.addCentredArc (cx, cy, r * 1.12f, r * 1.12f, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (tr, juce::PathStrokeType (2.0f));
        juce::Path v; v.addCentredArc (cx, cy, r * 1.12f, r * 1.12f, 0, a0, ang, true);
        g.setColour (col::brass.withAlpha (0.85f)); g.strokePath (v, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.5f), cx + r * 0.1f, cy + r * 0.15f, juce::Colours::transparentBlack, cx + r * 1.3f, cy + r * 0.15f, true);
        g.setGradientFill (sh); g.fillEllipse (cx - r * 1.05f, cy - r * 1.0f, r * 2.3f, r * 2.3f);
        g.drawImage (cap, juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ cx, cy }));
        juce::Path ind; ind.addRoundedRectangle (-r * 0.045f, -r * 0.72f, r * 0.09f, r * 0.36f, r * 0.03f);
        g.setColour (col::slateDeep); g.fillPath (ind, juce::AffineTransform::rotation (ang).translated (cx, cy));
        juce::ignoreUnused (s);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) h * 0.52f))); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (15.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (15.0f)); }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        if (b.getToggleState()) { g.setColour (col::brass); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); return; }
        g.setColour (juce::Colours::black.withAlpha (down ? 0.35f : (over ? 0.22f : 0.16f))); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (over ? col::chalkDim : col::line); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    }
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.16f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (col::line); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        juce::Path a; const float ax = (float) w - 18, ay = (float) h * 0.5f;
        a.startNewSubPath (ax - 4, ay - 2); a.lineTo (ax, ay + 2); a.lineTo (ax + 4, ay - 2);
        g.setColour (col::chalkDim); g.strokePath (a, juce::PathStrokeType (1.5f));
    }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& l) override { l.setBounds (10, 1, b.getWidth() - 30, b.getHeight() - 2); l.setFont (getComboBoxFont (b)); }
};

// ── La plomada: oscila con el error de afinación de lo cantado; se queda quieta cuando la voz está justa ──
class PlumbEditor::Bob : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Bob (PlumbProcessor& p) : proc (p)
    {
        img = loadImg (BinaryData::bob_png, BinaryData::bob_pngSize);
        setInterceptsMouseClicks (false, false);
    }
    void tick (float dt)
    {
        // La última estimación, del mismo anillo que lee la vista de notas (last() es una copia sin sincronizar de
        // una estructura que el audio escribe mientras tanto).
        const auto& eng = proc.getEngine();
        const int64_t vw = eng.viewWrite();
        const auto o = vw > 0 ? eng.viewAt (vw - 1) : CorrectOut {};
        float target = 0;
        if (o.voiced && o.target > 0) { errIn = (o.pitch - o.target) * 100.0f; errOut = (o.output - o.target) * 100.0f; target = juce::jlimit (-25.0f, 25.0f, errIn * 0.5f); voiced = true; }
        else voiced = false;
        // Péndulo amortiguado empujado hacia el ángulo del error, medio grado por cent y a la derecha si
        // la nota está alta; la escala grabada (±25°, una marca cada 2,5°) es entonces de ±50 cents, una marca cada 5.
        // dt acotado (un cuadro tardío no debe disparar la integración) y el ángulo dentro de la escala grabada.
        const float w = 7.0f, z = 0.28f, h = juce::jlimit (0.0f, 1.0f / 30.0f, dt);
        vel += (-w * w * (theta - target) - 2 * z * w * vel) * h;
        theta += vel * h;
        if (std::abs (theta) > 25.0f) { theta = juce::jlimit (-25.0f, 25.0f, theta); vel = 0; }
        sung = o.voiced ? o.pitch : 0; out = o.voiced ? o.output : 0;
        // Sólo se repinta lo que cambió: la plomada si se movió, si no las dos lecturas.
        const auto text = voiced ? noteName (sung) + signedStr (100.0f * (sung - std::round (sung))) + noteName (out) + signedStr (100.0f * (out - std::round (out))) : juce::String();
        if (std::abs (theta - drawnTheta) > 0.02f) { drawnTheta = theta; drawnText = text; repaint(); }
        else if (text != drawnText) { drawnText = text; repaint (0, 455, getWidth(), 46); }
    }
    void paint (juce::Graphics& g) override
    {
        const float h = 0.24f * kPxPerM, w = h / 3.0f;
        const auto pv = kPivot - getPosition().toFloat();
        g.drawImageTransformed (img, juce::AffineTransform::scale (w / img.getWidth(), h / img.getHeight())
                                        .translated (pv.x - w * 0.5f, pv.y).rotated (juce::degreesToRadians (-theta), pv.x, pv.y), false);
        // Lecturas bajo la escala.
        auto b = juce::Rectangle<float> (20, 455, (float) getWidth() - 20, 22);
        g.setFont (juce::FontOptions (15.0f));
        g.setColour (col::chalkDim); g.drawText ("Sung", b.removeFromLeft (70), juce::Justification::centredLeft);
        g.setColour (col::chalk); g.drawText (voiced ? noteName (sung) + "  " + signedStr (100.0f * (sung - std::round (sung))) + " c" : juce::String ("-"), b, juce::Justification::centredLeft);
        auto b2 = juce::Rectangle<float> (20, 478, (float) getWidth() - 20, 22);
        g.setColour (col::chalkDim); g.drawText ("Out", b2.removeFromLeft (70), juce::Justification::centredLeft);
        g.setColour (col::brass); g.drawText (voiced ? noteName (out) + "  " + signedStr (100.0f * (out - std::round (out))) + " c" : juce::String ("-"), b2, juce::Justification::centredLeft);
    }
private:
    PlumbProcessor& proc; juce::Image img;
    float theta = 0, vel = 0, errIn = 0, errOut = 0, sung = 0, out = 0, drawnTheta = 1000; bool voiced = false;
    juce::String drawnText;
    static juce::String signedStr (float c) { const int v = juce::roundToInt (c); return (v > 0 ? "+" : "") + juce::String (v); }
};

// ── La pizarra: la vista de notas en vivo ──
class PlumbEditor::NoteView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit NoteView (PlumbProcessor& p) : proc (p)
    {
        setTooltip ("Live notes: what is sung in chalk, the corrected pitch in brass, the notes of the scale as lines. The blocks are the target notes.");
    }
    void tick()
    {
        const auto& e = proc.getEngine();
        const int64_t w = e.viewWrite();
        if (w - readPos > 3000) readPos = w - 3000;
        for (; readPos < w; ++readPos)
        {
            const auto o = e.viewAt (readPos);
            // El tiempo volvió atrás (prepareToPlay, un render): lo viejo ya no se descartaría nunca.
            if (! pts.empty() && o.time < pts.back().time) { pts.clear(); med.clear(); medDone = 0; }
            pts.push_back (o); med.push_back ({});
        }
        const double sr = proc.getSampleRateNow();
        if (! pts.empty())
        {
            now = pts.back().time;
            while (! pts.empty() && pts.front().time < now - (kSecs + 0.5) * sr) { pts.pop_front(); med.pop_front(); if (medDone > 0) --medDone; }
        }
        // Mediana de ±15 vecinos con voz (~10 ms) de cada punto, una sola vez, cuando ya llegaron sus vecinos de
        // adelante: un grupo corto de valores fuera de lugar (el detector en una transición) no la mueve.
        for (; medDone + 15 < pts.size(); ++medDone)
        {
            float wp[31], wo[31]; int k = 0;
            for (size_t j = medDone >= 15 ? medDone - 15 : 0; j <= medDone + 15; ++j)
                if (pts[j].voiced && pts[j].pitch > 0) { wp[k] = pts[j].pitch; wo[k] = pts[j].output; ++k; }
            auto& m = med[medDone];
            if (k >= 3) { std::nth_element (wp, wp + k / 2, wp + k); std::nth_element (wo, wo + k / 2, wo + k); m = { wp[k / 2], wo[k / 2], true }; }
        }
        // El centro sigue a la voz despacio.
        float sum = 0; int cnt = 0;
        for (auto it = pts.rbegin(); it != pts.rend() && cnt < 600; ++it) if (it->voiced) { sum += it->pitch; ++cnt; }
        if (cnt > 20) centre += (sum / (float) cnt - centre) * 0.05f;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const double sr = proc.getSampleRateNow();
        const float semis = 16.0f, rowH = b.getHeight() / semis, left = 44.0f;
        auto yOf = [&] (float m) { return b.getCentreY() - (m - centre) * rowH; };
        auto xOf = [&] (double t) { return b.getRight() - 8.0f - (float) ((now - t) / sr) * (b.getWidth() - left - 8.0f) / kSecs; };
        // Renglones: las notas de la escala en bronce tenue, las demás apenas.
        const uint16_t allowed = proc.allowedMask(), bypass = proc.bypassMask();
        g.setFont (juce::FontOptions (13.0f));
        for (int m = (int) std::floor (centre - semis / 2) - 1; m <= (int) std::ceil (centre + semis / 2) + 1; ++m)
        {
            const float y = yOf ((float) m);
            if (y < b.getY() - 2 || y > b.getBottom() + 2) continue;
            const int pc = (m % 12 + 12) % 12;
            const bool on = (allowed >> pc) & 1, by = (bypass >> pc) & 1;
            g.setColour (on ? col::brassDim.withAlpha (0.55f) : col::chalkFaint.withAlpha (by ? 0.35f : 0.18f));
            g.drawHorizontalLine ((int) y, b.getX() + left, b.getRight());
            g.setColour (on ? col::chalkDim : col::chalkFaint);
            g.drawText (noteName ((float) m), juce::Rectangle<float> (b.getX(), y - 8, left - 6, 16), juce::Justification::centredRight);
        }
        if (pts.empty()) { g.setColour (col::chalkFaint); g.setFont (juce::FontOptions (16.0f)); g.drawText ("Sing, or play something with a pitch.", b, juce::Justification::centred); return; }
        // Segmentos con voz de menos de 25 ms (transiciones entre vocales) no se dibujan.
        std::vector<bool> keep (pts.size(), false);
        for (size_t i = 0; i < pts.size();)
        {
            if (! pts[i].voiced || pts[i].pitch <= 0) { ++i; continue; }
            size_t j = i; while (j + 1 < pts.size() && pts[j + 1].voiced && pts[j + 1].pitch > 0) ++j;
            const bool longEnough = pts[j].time - pts[i].time >= 0.025 * sr;
            for (size_t k = i; k <= j; ++k) keep[k] = longEnough;
            i = j + 1;
        }
        // Bloques de las notas objetivo.
        auto drawBlock = [&] (double t0, double t1, float tgt) {
            const float x0 = std::max (b.getX() + left, xOf (t0)), x1 = xOf (t1);
            if (x1 - x0 < 2 || t1 - t0 < 0.03 * sr) return;                    // objetivos de un instante (un arranque) no
            auto r = juce::Rectangle<float> (x0, yOf (tgt) - rowH * 0.32f, x1 - x0, rowH * 0.64f);
            g.setColour (col::brass.withAlpha (0.13f)); g.fillRoundedRectangle (r, rowH * 0.3f);
            g.setColour (col::brass.withAlpha (0.45f)); g.drawRoundedRectangle (r, rowH * 0.3f, 1.0f);
        };
        double bs = -1, bl = -1; float bt = -1;
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const auto& p = pts[i];
            const bool v = p.voiced && p.target > 0 && keep[i];
            const float t = v ? std::round (p.target) : -1.0f;
            if (! juce::exactlyEqual (t, bt)) { if (bt > 0) drawBlock (bs, bl, bt); bs = p.time; bt = t; }
            bl = p.time;
        }
        if (bt > 0) drawBlock (bs, bl, bt);
        // Lo cantado (tiza) y lo corregido (bronce).
        auto trace = [&] (bool corrected, juce::Colour c, float wdt) {
            juce::Path path; bool open = false; double lastT = 0; float lastV = 0, lastX = -1e9f;
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const auto& p = pts[i];
                if (! p.voiced || p.pitch <= 0 || ! keep[i]) { open = false; continue; }
                const float v = corrected ? p.output : p.pitch;
                // Un punto lejos de la mediana de sus vecinos (el detector en una transición) no se dibuja; un salto
                // grande corta el trazo en vez de dibujar una línea vertical.
                if (med[i].ok && std::abs (v - (corrected ? med[i].out : med[i].pitch)) > 3) continue;
                const float x = xOf (p.time), y = yOf (v);
                if (x < b.getX() + left) continue;
                const bool cut = ! open || p.time - lastT > 0.03 * sr || std::abs (v - lastV) > 3;
                if (! cut && x - lastX < 0.75f) continue;                       // como mucho un punto cada ¾ de píxel
                if (cut) { path.startNewSubPath (x, y); open = true; } else path.lineTo (x, y);
                lastT = p.time; lastV = v; lastX = x;
            }
            g.setColour (c); g.strokePath (path, juce::PathStrokeType (wdt, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        trace (false, col::chalk.withAlpha (0.8f), 1.6f);
        trace (true, col::brass, 2.2f);
        g.setFont (juce::FontOptions (13.0f));
        g.setColour (col::chalk.withAlpha (0.8f)); g.drawText ("Sung", juce::Rectangle<float> (b.getRight() - 170, b.getY() + 4, 60, 16), juce::Justification::centredRight);
        g.setColour (col::brass); g.drawText ("Corrected", juce::Rectangle<float> (b.getRight() - 100, b.getY() + 4, 92, 16), juce::Justification::centredRight);
    }
private:
    static constexpr float kSecs = 5.0f;
    PlumbProcessor& proc;
    struct Med { float pitch = 0, out = 0; bool ok = false; };
    std::deque<CorrectOut> pts; std::deque<Med> med; size_t medDone = 0; int64_t readPos = 0; double now = 0; float centre = 60.0f;
};

// ── Página avanzada: se abre sobre la pizarra ──
class PlumbEditor::Advanced : public juce::Component
{
public:
    explicit Advanced (PlumbProcessor& p) : proc (p)
    {
        struct D { const char* id; const char* title; int kind; const char* tip; };   // 0 perilla, 1 combo, 2 interruptor
        const std::vector<std::pair<const char*, std::vector<D>>> groups {
            { "Character", { { "custom_character", "Custom", 2, "Custom character: set these by hand instead of from Style." },
                             { "transition", "Transition", 0, "How the correction glides from one target note to the next." },
                             { "humanize", "Humanize", 0, "Long notes are corrected more slowly than note starts, so they keep their life." },
                             { "flex", "Flex", 0, "Only notes within this distance of the target are corrected; bends and swoops beyond it pass." },
                             { "hysteresis", "Hysteresis", 0, "How far past the midpoint a note must go before the target changes." },
                             { "keep_vibrato", "Vibrato", 0, "How much of the singer's vibrato is kept." },
                             { "keep_drift", "Drift", 0, "How much of the slow wander inside a note is kept." } } },
            { "Pitch", { { "transpose", "Transpose", 0, "Semitones, applied to the target." }, { "fine", "Fine", 0, "Cents." },
                         { "vib_depth", "Vibrato add", 0, "Synthetic vibrato added to the target." }, { "vib_rate", "Rate", 0, "Rate of the added vibrato." }, { "vib_delay", "Delay", 0, "The added vibrato starts after this delay." } } },
            { "Formants", { { "formants", "Preserve", 2, "Keep the voice's formants where they are when the pitch moves (no chipmunk)." },
                            { "formant_amount", "Amount", 0, "100 %: formants stay; lower: they follow the pitch partly." },
                            { "throat", "Throat", 0, "Moves the formants: a longer (down) or shorter (up) vocal tract." } } },
            { "MIDI and input", { { "midi_mode", "MIDI", 1, "Scale from held notes, note lock (the held note is the target) or auto." }, { "bend_range", "Bend", 0, "Pitch bend range in note lock." },
                                  { "ref_a", "A =", 0, "Reference tuning." }, { "gate", "Gate", 0, "Below this level the detector ignores the input." } } },
            { "Output", { { "mix", "Mix", 0, "Corrected against the dry voice, aligned." }, { "output", "Output", 0, "Output level." } } },
        };
        for (auto& gp : groups)
        {
            Group grp; grp.title = gp.first;
            for (auto& d : gp.second)
            {
                auto c = std::make_unique<Ctl>(); c->id = d.id; c->title = d.title; c->kind = d.kind;
                if (d.kind == 0)
                {
                    c->s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
                    c->s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
                    c->s->setTooltip (d.tip); c->sa = std::make_unique<SA> (proc.apvts, d.id, *c->s);
                    c->s->onValueChange = [this] { repaint(); };
                    addAndMakeVisible (*c->s);
                }
                else if (d.kind == 1)
                {
                    c->c = std::make_unique<juce::ComboBox>();
                    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (d.id))) c->c->addItemList (pc->choices, 1);
                    c->c->setTooltip (d.tip); c->ca = std::make_unique<CA> (proc.apvts, d.id, *c->c);
                    addAndMakeVisible (*c->c);
                }
                else
                {
                    c->b = std::make_unique<juce::TextButton> (d.title); c->b->setClickingTogglesState (true);
                    c->b->setTooltip (d.tip); c->ba = std::make_unique<BA> (proc.apvts, d.id, *c->b);
                    c->b->onClick = [this] { repaint(); };
                    addAndMakeVisible (*c->b);
                }
                grp.ctls.push_back (std::move (c));
            }
            groupsV.push_back (std::move (grp));
        }
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (col::slateDeep); g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (col::line); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
        const bool custom = proc.value ("custom_character") > 0.5f;
        for (auto& gp : groupsV)
        {
            g.setColour (col::brass); g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            g.drawText (gp.title, gp.titleR, juce::Justification::centredLeft);
            for (auto& c : gp.ctls)
            {
                if (c->kind == 2) continue;
                const auto r = c->s ? c->s->getBounds() : c->c->getBounds();
                const bool dim = gp.title == "Character" && ! custom;
                g.setColour (dim ? col::chalkFaint : col::chalkDim); g.setFont (juce::FontOptions (13.0f));
                g.drawText (c->title, r.getCentreX() - 50, r.getBottom() + 1, 100, 15, juce::Justification::centred);
                if (c->s) { g.setColour (dim ? col::chalkFaint : col::chalk); g.drawText (valueText (c->id), r.getCentreX() - 50, r.getBottom() + 15, 100, 15, juce::Justification::centred); }
            }
        }
        if (! custom) { g.setColour (col::chalkFaint); g.setFont (juce::FontOptions (13.0f)); g.drawText ("Set by Style. Turn on Custom to change them.", groupsV[0].titleR.withX (groupsV[0].titleR.getX() + 220).withWidth (360), juce::Justification::centredLeft); }
    }
    void resized() override
    {
        auto b = getLocalBounds().reduced (18, 12);
        const int rowH = 132;
        auto row1 = b.removeFromTop (rowH); auto row2 = b.removeFromTop (rowH); auto row3 = b;
        layoutGroup (groupsV[0], row1);
        auto r2 = row2; layoutGroup (groupsV[1], r2.removeFromLeft (r2.getWidth() * 5 / 8)); layoutGroup (groupsV[2], r2);
        auto r3 = row3; layoutGroup (groupsV[3], r3.removeFromLeft (r3.getWidth() * 5 / 8)); layoutGroup (groupsV[4], r3);
    }
private:
    struct Ctl { juce::String id, title; int kind = 0; std::unique_ptr<juce::Slider> s; std::unique_ptr<juce::ComboBox> c; std::unique_ptr<juce::TextButton> b; std::unique_ptr<SA> sa; std::unique_ptr<CA> ca; std::unique_ptr<BA> ba; };
    struct Group { juce::String title; std::vector<std::unique_ptr<Ctl>> ctls; juce::Rectangle<int> titleR; };
    PlumbProcessor& proc; std::vector<Group> groupsV;
    void layoutGroup (Group& gp, juce::Rectangle<int> r)
    {
        gp.titleR = r.removeFromTop (24);
        int n = 0; for (auto& c : gp.ctls) n += c->kind == 2 ? 0 : 1;
        // Los interruptores van al lado del título.
        int tx = gp.titleR.getX() + 110;
        for (auto& c : gp.ctls) if (c->kind == 2) { c->b->setBounds (tx, gp.titleR.getY(), 100, 24); tx += 108; }
        const int cw = std::min (96, r.getWidth() / std::max (1, n));
        int x = r.getX();
        for (auto& c : gp.ctls)
        {
            if (c->kind == 2) continue;
            const auto cell = juce::Rectangle<int> (x, r.getY() + 6, cw, r.getHeight() - 6);
            if (c->s) c->s->setBounds (cell.withSizeKeepingCentre (54, 54).withY (cell.getY()));
            if (c->c) c->c->setBounds (x, cell.getY() + 12, cw + 100, 28);
            x += c->c ? cw + 110 : cw;
        }
    }
    juce::String valueText (const juce::String& id) const
    {
        const float v = proc.value (id);
        if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (proc.apvts.getParameter (id)))
        {
            if (id == "flex" && v >= 199.5f) return "always";
            const bool small = pf->getNormalisableRange().end - pf->getNormalisableRange().start <= 30;
            return (small ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + (pf->getLabel().isNotEmpty() ? " " + pf->getLabel() : juce::String());
        }
        return {};
    }
};

// ── Editor ──
PlumbEditor::PlumbEditor (PlumbProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf = std::make_unique<Look>();
    setLookAndFeel (lnf.get());
    content.painter = [this] (juce::Graphics& g) { paintPanel (g); };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);

    bob = std::make_unique<Bob> (proc); content.addAndMakeVisible (*bob);
    bob->setTooltip ("The plumb bob swings with how far the sung note is from its target and hangs still when it is in tune.");
    notes = std::make_unique<NoteView> (proc); content.addAndMakeVisible (*notes);
    advanced = std::make_unique<Advanced> (proc); content.addChildComponent (*advanced);

    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &manualBtn, &advBtn, &listenBtn, &applyBtn, &compareBtn }) content.addAndMakeVisible (b);
    applyBtn.setVisible (false);
    prevBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.loadPreset ((proc.getCurrentProgram() - 1 + n) % n); };
    nextBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.loadPreset ((proc.getCurrentProgram() + 1) % n); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    manualBtn.onClick = [this] { showManual (manual == nullptr); };
   #if ! DH_HAS_MANUAL
    manualBtn.setVisible (false);
   #endif
    advBtn.setClickingTogglesState (true);
    advBtn.onClick = [this] { advanced->setVisible (advBtn.getToggleState()); };
    advBtn.setTooltip ("Advanced: the character of the correction, pitch, formants, MIDI and output.");
    compareBtn.setClickingTogglesState (true);
    compareBtn.onClick = [this] { proc.compare.store (compareBtn.getToggleState()); };
    compareBtn.setTooltip ("Compare: hear the voice without correction, at the same level and latency.");
    listenBtn.onClick = [this] { proc.startListening(); suggestion = "Listening..."; applyBtn.setVisible (false); };
    listenBtn.setTooltip ("Listen: sing or play the song for 15 to 30 seconds and Plumb proposes its key. It never changes the key by itself.");
    applyBtn.onClick = [this] {
        int t; bool mi; float cf; proc.keySuggestion (t, mi, cf);
        proc.setValue ("key", (float) t); proc.setValue ("scale", mi ? 2.0f : 1.0f);
        proc.stopListening(); applyBtn.setVisible (false); suggestion.clear();
    };

    static const char* modeNames[] = { "Live", "Tight", "Studio" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = modeBtns.add (new juce::TextButton (modeNames[i]));
        b->onClick = [this, i] { proc.setValue ("mode", (float) i); };
        content.addAndMakeVisible (b);
    }
    modeBtns[0]->setTooltip ("Live: 32 samples (0.7 ms at 48 kHz). For monitoring on stage and in the headphones.");
    modeBtns[1]->setTooltip ("Tight: 128 samples (2.7 ms). The default: plans each splice between pulses.");
    modeBtns[2]->setTooltip ("Studio: 1024 samples (21 ms). For mixing: catches every note start.");
    for (int i = 0; i < 12; ++i)
    {
        auto* b = keyBtns.add (new juce::TextButton (noteNames[i]));
        b->onClick = [this, i] {
            if ((int) proc.value ("scale") == params::customScale())
            {
                const auto id = "deg" + juce::String (i);
                proc.setValue (id, (float) (((int) proc.value (id) + 1) % 3));
            }
            else proc.setValue ("key", (float) i);
            refreshKeys();
        };
        content.addAndMakeVisible (b);
    }
    scaleBox.addItemList (params::scaleNames(), 1); scaleAtt = std::make_unique<CA> (proc.apvts, "scale", scaleBox);
    voiceBox.addItemList (params::voices(), 1); voiceAtt = std::make_unique<CA> (proc.apvts, "voice", voiceBox);
    scaleBox.onChange = [this] { refreshKeys(); };
    scaleBox.setTooltip ("Scale: the notes the voice is corrected to. Custom: click the note buttons to snap, skip or leave a note alone.");
    voiceBox.setTooltip ("Voice: the range the detector listens to. Instrument covers 40 Hz to 2 kHz.");
    content.addAndMakeVisible (scaleBox); content.addAndMakeVisible (voiceBox);
    auto knob = [this] (juce::Slider& s, const char* id, std::unique_ptr<SA>& att, const char* tip) {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        att = std::make_unique<SA> (proc.apvts, id, s); s.setTooltip (tip);
        s.onValueChange = [this] { content.repaint(); };
        content.addAndMakeVisible (s); };
    knob (amount, "amount", amountAtt, "Amount: how much of the distance to the target note is corrected.");
    knob (speed, "speed", speedAtt, "Speed: how fast the correction reaches the target. 0 is instant: the hard-tune effect.");
    knob (style, "style", styleAtt, "Style: from Natural (keeps vibrato, scoops and bends) to Hard (every note exactly on pitch).");

    proc.onPresetChanged = [this] { presetBtn.setButtonText (proc.presetDisplayName()); refreshKeys(); content.repaint(); };
    presetBtn.setButtonText (proc.presetDisplayName());
    compareBtn.setToggleState (proc.compare.load(), juce::dontSendNotification);
    refreshKeys();
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.5), (int) (baseH * 1.5));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.8), (int) (baseH * 0.8));
    startTimerHz (60);

    // Capturas sin pantalla: PLUMB_SNAPSHOT=/ruta.png, PLUMB_PRESET=n, PLUMB_WAIT=s, PLUMB_ADVANCED=1, PLUMB_MANUAL=1,
    // PLUMB_DEMO_INPUT=archivo.wav (la versión independiente lo reproduce por la cadena, para que la vista tenga algo).
    const auto snap = juce::SystemStats::getEnvironmentVariable ("PLUMB_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
        const int preset = env ("PLUMB_PRESET", "-1").getIntValue(), wait = env ("PLUMB_WAIT", "4").getIntValue();
        const bool adv = env ("PLUMB_ADVANCED", "0").getIntValue() != 0, man = env ("PLUMB_MANUAL", "0").getIntValue() != 0;
        const auto key = env ("PLUMB_KEY", ""), scale = env ("PLUMB_SCALE", "");
        juce::Component::SafePointer<PlumbEditor> safe (this);
        juce::Timer::callAfterDelay (300, [safe, preset, adv, man, key, scale] {
            if (safe == nullptr) return;
            if (preset >= 0) safe->proc.loadPreset (preset);
            if (key.isNotEmpty()) safe->proc.setValue ("key", key.getFloatValue());
            if (scale.isNotEmpty()) safe->proc.setValue ("scale", scale.getFloatValue());
            if (adv) safe->advBtn.setToggleState (true, juce::sendNotification);
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

PlumbEditor::~PlumbEditor() { proc.onPresetChanged = nullptr; manual.reset(); setLookAndFeel (nullptr); }

void PlumbEditor::refreshKeys()
{
    const bool custom = (int) proc.value ("scale") == params::customScale();
    const int key = (int) proc.value ("key");
    const uint16_t allowed = proc.allowedMask();
    for (int i = 0; i < 12; ++i)
    {
        auto* b = keyBtns[i];
        if (custom)
        {
            const int st = (int) proc.value ("deg" + juce::String (i));
            b->setToggleState (st == 0, juce::dontSendNotification);
            b->setButtonText (st == 2 ? juce::String (noteNames[i]) + " ~" : juce::String (noteNames[i]));
            b->setTooltip (juce::String (noteNames[i]) + (st == 0 ? ": a target" : st == 1 ? ": skipped (pulled to a neighbour)" : ": left alone (no correction near it)") + ". Click to change.");
        }
        else
        {
            b->setToggleState (i == key, juce::dontSendNotification);
            b->setButtonText (noteNames[i]);
            b->setTooltip ("Key of " + juce::String (noteNames[i]) + ((allowed >> i) & 1 ? " (in the scale)" : ""));
        }
        b->getProperties().set ("inScale", (bool) ((allowed >> i) & 1));
    }
    content.repaint();
}

void PlumbEditor::showPresetMenu()
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

void PlumbEditor::showManual (bool on)
{
   #if DH_HAS_MANUAL
    if (! on) { manual.reset(); manualBtn.setToggleState (false, juce::dontSendNotification); return; }
    dh::manual::Theme t; t.bg = col::slateDeep; t.panel = col::slate; t.text = col::chalk; t.muted = col::chalkDim; t.accent = col::brass; t.line = col::line;
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PlumbEditor> (this)] { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*m); m->setBounds (getLocalBounds()); m->grabKeyboardFocus();
    manual = std::move (m); manualBtn.setToggleState (true, juce::dontSendNotification);
   #else
    juce::ignoreUnused (on);
   #endif
}

void PlumbEditor::timerCallback()
{
    // Live crea el editor al insertar y después sólo esconde la ventana: escondido no se anima ni se repinta (QA en
    // Live 2026-10-22, H3).
    if (! dh::reallyShowing (*this)) return;
    bob->tick (1.0f / 60.0f);
    notes->tick();
    const int m = (int) proc.value ("mode");
    for (int i = 0; i < 3; ++i) modeBtns[i]->setToggleState (i == m, juce::dontSendNotification);
    // Lo que puede cambiar sin un clic acá (automatización del host, Apply, otra instancia del editor): Compare, la
    // latencia informada, la tonalidad y la escala, el nombre del preset con su marca de modificado.
    compareBtn.setToggleState (proc.compare.load(), juce::dontSendNotification);
    if (const int lat = proc.getLatencySamples(); lat != shownLatency) { shownLatency = lat; content.repaint (juce::Rectangle<int> (676, 52, 268, 18)); }
    const uint32_t sig = (uint32_t) proc.allowedMask() | ((uint32_t) proc.bypassMask() << 12) | ((uint32_t) (int) proc.value ("key") << 24) | ((uint32_t) (int) proc.value ("scale") << 28);
    if (sig != keySignature) { keySignature = sig; refreshKeys(); content.repaint(); }
    if (const auto name = proc.presetDisplayName(); name != presetBtn.getButtonText()) presetBtn.setButtonText (name);
    if (proc.isListening())
    {
        int t; bool mi; float cf; proc.keySuggestion (t, mi, cf);
        const double secs = proc.listenSeconds();
        if (secs > 8 && cf > 0.02f)
        {
            suggestion = "Sounds like " + juce::String (noteNames[t]) + (mi ? " minor" : " major");
            applyBtn.setVisible (true);
        }
        else suggestion = "Listening... " + juce::String ((int) secs) + " s";
        if (secs > 45 && ! applyBtn.isVisible()) { proc.stopListening(); suggestion = "Not enough notes yet. Try again."; }
        content.repaint (juce::Rectangle<int> (330, 690, 520, 30));
    }
}

void PlumbEditor::paint (juce::Graphics& g) { g.fillAll (col::slateDeep); }

void PlumbEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    // Arriba (dentro del marco).
    prevBtn.setBounds (330, 22, 32, 30); presetBtn.setBounds (366, 22, 240, 30); nextBtn.setBounds (610, 22, 32, 30);
    for (int i = 0; i < 3; ++i) modeBtns[i]->setBounds (676 + i * 92, 22, 88, 30);
    advBtn.setBounds (1028, 22, 96, 30); manualBtn.setBounds (952, 22, 70, 30);
    // Plomada (columna izquierda) y pizarra.
    bob->setBounds (10, 60, 315, 510);
    notes->setBounds (330, 68, 846, 480);
    advanced->setBounds (330, 64, 846, 488);
    // Abajo.
    for (int i = 0; i < 12; ++i) keyBtns[i]->setBounds (330 + i * 46, 572, 42, 32);
    scaleBox.setBounds (330, 618, 210, 32); voiceBox.setBounds (548, 618, 150, 32);
    listenBtn.setBounds (706, 618, 90, 32); applyBtn.setBounds (706, 656, 90, 30);
    compareBtn.setBounds (40, 680, 120, 32);
    amount.setBounds (870, 576, 92, 92); speed.setBounds (974, 576, 92, 92); style.setBounds (1078, 576, 92, 92);
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

juce::String PlumbEditor::valueText (const juce::String& id) const
{
    const float v = proc.value (id);
    if (id == "style") return v < 5 ? "Natural" : v > 99.4f ? "Hard" : juce::String (juce::roundToInt (v)) + " %";
    if (id == "speed") return v < 0.5f ? "instant" : juce::String (juce::roundToInt (v)) + " ms";
    return juce::String (juce::roundToInt (v)) + " %";
}

void PlumbEditor::paintPanel (juce::Graphics& g)
{
    static const juce::Image bg = loadImg (BinaryData::bg_jpg, BinaryData::bg_jpgSize);
    g.drawImage (bg, juce::Rectangle<float> (0, 0, (float) baseW, (float) baseH), juce::RectanglePlacement::stretchToFit);
    g.setColour (col::chalk); g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("Plumb", 40, 18, 200, 36, juce::Justification::centredLeft);
    // Latencia del modo elegido, en ms.
    // En las capturas sin un dispositivo de audio real la aplicación puede quedar a 16 kHz: ahí se muestran los ms
    // a 48 kHz, la tasa de referencia del manual. En uso normal, siempre la tasa real.
    static const bool snapshotMode = juce::SystemStats::getEnvironmentVariable ("PLUMB_SNAPSHOT", {}).isNotEmpty();
    const double sr = snapshotMode && proc.getSampleRateNow() < 32000 ? 48000.0 : proc.getSampleRateNow();
    const int lat = proc.getLatencySamples();
    g.setColour (col::chalkDim); g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Latency " + juce::String (1000.0 * lat / sr, 2) + " ms (" + juce::String (lat) + " samples)", 676, 54, 268, 14, juce::Justification::centred);
    // Perillas: nombre y valor.
    auto cap = [&] (juce::Slider& s, const char* name, const char* id) {
        const auto r = s.getBounds();
        g.setColour (col::chalkDim); g.setFont (juce::FontOptions (14.0f)); g.drawText (name, r.getX() - 10, r.getBottom() + 2, r.getWidth() + 20, 16, juce::Justification::centred);
        g.setColour (col::chalk); g.drawText (valueText (id), r.getX() - 10, r.getBottom() + 18, r.getWidth() + 20, 16, juce::Justification::centred); };
    cap (amount, "Amount", "amount"); cap (speed, "Speed", "speed"); cap (style, "Style", "style");
    // Debajo de las teclas: qué notas son de la escala.
    const uint16_t allowed = proc.allowedMask();
    for (int i = 0; i < 12; ++i)
        if ((allowed >> i) & 1) { const auto r = keyBtns[i]->getBounds().toFloat(); g.setColour (col::brass.withAlpha (0.8f)); g.fillEllipse (r.getCentreX() - 2.5f, r.getBottom() + 3, 5, 5); }
    g.setColour (col::chalkDim); g.setFont (juce::FontOptions (14.0f));
    g.drawText (suggestion, 804, 618, 300, 32, juce::Justification::centredLeft);
    if ((int) proc.value ("scale") == params::customScale())
        g.drawText ("Custom: click a note to make it a target, skip it, or leave it alone (~).", 330, 656, 470, 20, juce::Justification::centredLeft);
}
