#include "PanelView.h"

namespace dh {

namespace {
juce::Colour cableColour (const juce::String& c)
{
    if (c == "red") return juce::Colour (0xffb3231c);
    if (c == "blue") return juce::Colour (0xff2452a8);
    if (c == "yellow") return juce::Colour (0xffd8a21e);
    if (c == "green") return juce::Colour (0xff2f8a3c);
    if (c == "white") return juce::Colour (0xffd9d8d2);
    return juce::Colour (0xff1c1c1e);
}
const char* kColors[] = { "red", "blue", "yellow", "green", "white", "black" };

// Descripción de cada jack para el globo: nombre y, en las entradas, la normalización.
juce::String jackDescription (int j, juce::String& detail)
{
    switch ((JackId) j)
    {
        case JackId::total_in:     detail = juce::String::fromUTF8 ("In · normalled: MG triangle → MG/T.EXT knobs"); return "TOTAL";
        case JackId::freq_in:      detail = juce::String::fromUTF8 ("In · normalled: EG1 → EG1/EXT knob (1 V/oct)"); return "FREQ";
        case JackId::ext_sig_in:   detail = juce::String::fromUTF8 ("In · audio into the HPF"); return "EXT SIGNAL IN";
        case JackId::hpf_cut_in:   detail = juce::String::fromUTF8 ("In · normalled: EG2 → HPF EG2/EXT (2 oct/V)"); return "HPF CUTOFF FREQ";
        case JackId::lpf_cut_in:   detail = juce::String::fromUTF8 ("In · normalled: EG2 → LPF EG2/EXT (2 oct/V)"); return "LPF CUTOFF FREQ";
        case JackId::gain_in:      detail = juce::String::fromUTF8 ("In · added to EG2 at the VCA"); return "VCA INITIAL GAIN";
        case JackId::signal_out:   detail = juce::String::fromUTF8 ("Out · after VOLUME"); return "SIGNAL OUT";
        case JackId::phones_out:   detail = juce::String::fromUTF8 ("Out · after VOLUME"); return "PHONES";
        case JackId::mg_tri_out:   detail = juce::String::fromUTF8 ("Out · ±2.5 V"); return "MG TRIANGLE";
        case JackId::mg_sq_out:    detail = juce::String::fromUTF8 ("Out · 0 / +5 V"); return "MG RECTANGLE";
        case JackId::eg1_out:      detail = juce::String::fromUTF8 ("Out · 0 → +5 V"); return "EG1 OUT";
        case JackId::eg1_rev_out:  detail = juce::String::fromUTF8 ("Out · +5 → 0 V"); return "EG1 REV OUT";
        case JackId::eg2_rev_out:  detail = juce::String::fromUTF8 ("Out · 0 → −5 V"); return "EG2 REV OUT";
        case JackId::vco2_cv_in:   detail = juce::String::fromUTF8 ("In · Hz/V · normalled: VCO 1+2 CV"); return "VCO 2 CV IN";
        case JackId::vco12_cv_in:  detail = juce::String::fromUTF8 ("In · Hz/V · normalled: keyboard CV"); return "VCO 1+2 CV IN";
        case JackId::kbd_cv_out:   detail = juce::String::fromUTF8 ("Out · Hz/V, after portamento"); return "KBD CV OUT";
        case JackId::sh_clock_in:  detail = juce::String::fromUTF8 ("In · S-trig (samples when pulled low)"); return "S/H CLOCK";
        case JackId::vca_ctrl_in:  detail = juce::String::fromUTF8 ("In · 0 to +5 V · normalled: EG1"); return "MVCA CONTROL";
        case JackId::eg1_trig_in:  detail = juce::String::fromUTF8 ("In · S-trig · normalled: trigger bus"); return "EG1 TRIG IN";
        case JackId::kbd_trig_in:  detail = juce::String::fromUTF8 ("In · S-trig · replaces the keyboard trigger"); return "TRIG IN";
        case JackId::kbd_trig_out: detail = juce::String::fromUTF8 ("Out · S-trig (low while a key is held)"); return "KBD TRIG OUT";
        case JackId::sh_in:        detail = juce::String::fromUTF8 ("In · signal to sample"); return "S/H IN";
        case JackId::sh_out:       detail = "Out"; return "S/H OUT";
        case JackId::vca_in:       detail = juce::String::fromUTF8 ("In · signal"); return "MVCA IN";
        case JackId::vca_out:      detail = "Out"; return "MVCA OUT";
        case JackId::pink_out:     detail = juce::String::fromUTF8 ("Out · 5 Vpp"); return "PINK NOISE";
        case JackId::white_out:    detail = juce::String::fromUTF8 ("Out · 5 Vpp"); return "WHITE NOISE";
        case JackId::wheel_out:    detail = juce::String::fromUTF8 ("Out · −5 to +5 V (pitch bend moves it)"); return "CONTROL WHEEL";
        case JackId::msw_out:      detail = juce::String::fromUTF8 ("Out · S-trig (sustain pedal presses it)"); return "MOMENTARY SWITCH";
        case JackId::esp_in:       detail = juce::String::fromUTF8 ("In · normalled: plugin sidechain input"); return "ESP SIGNAL IN";
        case JackId::esp_amp_out:  detail = juce::String::fromUTF8 ("Out · preamp"); return "ESP AMP OUT";
        case JackId::esp_bpf_out:  detail = juce::String::fromUTF8 ("Out · band pass"); return "ESP BPF OUT";
        case JackId::esp_fv_out:   detail = juce::String::fromUTF8 ("Out · pitch as Hz/V CV"); return juce::String::fromUTF8 ("ESP F∝V CV OUT");
        case JackId::esp_env_out:  detail = juce::String::fromUTF8 ("Out · 0 to +5 V"); return "ESP ENV OUT";
        case JackId::esp_trig_out: detail = juce::String::fromUTF8 ("Out · S-trig"); return "ESP TRIG OUT";
        default: detail = {}; return layout::jacks[j].id;
    }
}
}

PanelView::PanelView (AT20VProcessor& p) : proc (p)
{
    setSize ((int) A.viewWidth(), (int) A.viewHeight());
    for (int i = 0; i < kNumKnobs; ++i)
    {
        const auto& k = layout::knobs[i];
        KnobView kv { i, A.toView (k.x, k.y), k.radius * A.pxPerUnit * 0.5f, k.big, k.steps, k.startDeg, k.endDeg };
        kv.param = proc.apvts.getParameter (layout::knobs[i].id);
        knobs.push_back (kv);
    }
    for (int j = 0; j < kNumJacks; ++j)
    {
        const auto& jd = layout::jacks[j];
        jacks.push_back ({ j, A.toView (jd.x, jd.y), A.toView (jd.x, jd.y, A.plugTopZ), jd.isOutput });
    }
    rebuildCables();
    setMouseCursor (juce::MouseCursor::NormalCursor);
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

PanelView::~PanelView() { stopTimer(); }

void PanelView::rebuildCables()
{
    std::vector<CableView> fresh;
    for (auto& c : proc.getCables())
    {
        CableView cv { c.src, c.dst, c.color, {} };
        // Conserva la física de los cables que siguen enchufados.
        bool kept = false;
        for (auto& old : cables)
            if (old.src == c.src && old.dst == c.dst) { cv.rope = old.rope; kept = true; break; }
        if (! kept) cv.rope.init (jacks[(size_t) c.src].plugTop, jacks[(size_t) c.dst].plugTop);
        fresh.push_back (cv);
    }
    cables = std::move (fresh);
    repaint();
}

// ───────────── valores de perillas ─────────────
float PanelView::knobValue (const KnobView& k) const
{
    return k.param != nullptr ? k.param->convertFrom0to1 (k.param->getValue()) : 0.0f;
}

float PanelView::knobAngle (const KnobView& k, float v) const
{
    if (k.steps > 0) return k.startDeg + (k.endDeg - k.startDeg) * v / (float) (k.steps - 1);
    return k.startDeg + (k.endDeg - k.startDeg) * v;
}

void PanelView::setKnobValue (KnobView& k, float v)
{
    if (k.param == nullptr) return;
    v = k.steps > 0 ? juce::jlimit (0.0f, (float) (k.steps - 1), std::round (v)) : juce::jlimit (0.0f, 1.0f, v);
    k.param->setValueNotifyingHost (k.param->convertTo0to1 (v));
}

int PanelView::knobAt (juce::Point<float> pt) const
{
    for (const auto& k : knobs)
    {
        const juce::Point<float> c (k.base.x, k.base.y - k.radius * 0.35f);
        if (pt.getDistanceFrom (c) < k.radius * 1.08f) return k.index;
    }
    return -1;
}

int PanelView::jackAt (juce::Point<float> pt) const
{
    const float r = layout::jackRadius * A.pxPerUnit * 0.5f * 1.15f;
    for (const auto& j : jacks)
        if (pt.getDistanceFrom (j.base) < r || pt.getDistanceFrom (j.plugTop) < r * 0.8f) return j.index;
    return -1;
}

// ───────────── dibujo ─────────────
void PanelView::drawPlug (juce::Graphics& g, juce::Point<float> base, const juce::String& color, float lift) const
{
    auto it = A.plugs.find (color);
    if (it == A.plugs.end() || ! it->second.isValid()) return;
    const float s = A.plugSize * 0.5f * (1.0f + lift * 0.12f);
    const float ax = A.plugAnchorX * 0.5f * (1.0f + lift * 0.12f), ay = A.plugAnchorY * 0.5f * (1.0f + lift * 0.12f);
    // drawImage usa la opacidad del último setColour (las sombras son semitransparentes).
    g.setOpacity (1.0f);
    if (lift > 0.0f)
    {
        // Ficha en la mano: sombra desplazada sobre el panel.
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (base.x - 9.0f + 10.0f, base.y - 5.0f + 14.0f, 18.0f, 10.0f);
        base.y -= 12.0f * lift;
        g.setOpacity (1.0f);
    }
    g.drawImage (it->second, base.x - ax, base.y - ay, s, s, 0, 0, it->second.getWidth(), it->second.getHeight());
}

void PanelView::drawCable (juce::Graphics& g, const juce::Path& path, const juce::String& color, bool shadowOnly) const
{
    if (shadowOnly)
    {
        // Sombra suave sobre el panel (la luz viene de arriba a la izquierda).
        const auto t = juce::AffineTransform::translation (6.0f, 11.0f);
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.strokePath (path, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.strokePath (path, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
        return;
    }
    const auto base = cableColour (color);
    const juce::PathStrokeType stroke (6.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (base.darker (0.75f));
    g.strokePath (path, juce::PathStrokeType (8.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (base);
    g.strokePath (path, juce::PathStrokeType (6.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // Brillo especular: una línea fina corrida hacia la luz.
    g.setColour (juce::Colours::white.withAlpha (color == "white" ? 0.55f : 0.32f));
    g.strokePath (path, juce::PathStrokeType (1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (-1.3f, -1.8f));
    juce::ignoreUnused (stroke);
}

void PanelView::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    if (A.background.isValid())
        g.drawImage (A.background, getLocalBounds().toFloat());
    else
        g.fillAll (juce::Colour (0xff1d1d1f));

    // LEDs
    const float ledVals[4] = { proc.ledMg.load() ? 1.0f : 0.0f, juce::jlimit (0.0f, 1.0f, proc.ledEg.load()),
                               proc.ledEspPeak.load() ? 1.0f : 0.0f, proc.ledEspTrig.load() ? 1.0f : 0.0f };
    for (int i = 0; i < 4; ++i)
    {
        if (ledVals[i] <= 0.01f) continue;
        const auto& l = layout::leds[i];
        const auto c = A.toView (l.x, l.y, 2.0f);
        juce::ColourGradient glow (juce::Colour (0xffff3018).withAlpha (0.55f * ledVals[i]), c.x, c.y,
                                   juce::Colour (0x00ff3018), c.x + 16.0f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - 16.0f, c.y - 16.0f, 32.0f, 32.0f);
        if (A.ledOn.isValid())
        {
            g.setOpacity (ledVals[i]);
            const float s = (float) A.ledOn.getWidth() * 0.5f;
            g.drawImage (A.ledOn, c.x - s * 0.5f, c.y - s * 0.5f + 1.0f, s, s, 0, 0, A.ledOn.getWidth(), A.ledOn.getHeight());
            g.setOpacity (1.0f);
        }
    }

    // Perillas (de atrás hacia adelante: las de abajo tapan a las de arriba).
    g.setOpacity (1.0f);
    for (const auto& k : knobs)
    {
        const auto& sheet = k.big ? A.knobBig : A.knobSmall;
        if (! sheet.image.isValid()) continue;
        const float ang = knobAngle (k, knobValue (k));
        const float degPerFrame = 360.0f / (float) sheet.frames;
        const int f = (int) std::lround ((ang < 0 ? ang + 360.0f : ang) / degPerFrame) % sheet.frames;
        const auto src = sheet.frame (f);
        const float s = (float) sheet.size * 0.5f;
        g.drawImage (sheet.image, k.base.x - sheet.anchorX * 0.5f, k.base.y - sheet.anchorY * 0.5f, s, s,
                     src.getX(), src.getY(), src.getWidth(), src.getHeight());
    }

    // Jack resaltado mientras se arrastra un cable o al pasar por encima.
    auto ringAt = [&] (int j, juce::Colour col) {
        const auto& jv = jacks[(size_t) j];
        const float r = layout::jackRadius * 1.08f;
        g.setColour (col);
        g.drawEllipse (jv.base.x - r, jv.base.y - r * A.cosT, r * 2.0f, r * 2.0f * A.cosT, 2.0f);
    };
    if (drag.active)
        for (const auto& j : jacks)
            if (canConnect (drag.fixedJack, j.index)) ringAt (j.index, juce::Colour (0x66ffd27a));
    if (hoverJack >= 0) ringAt (hoverJack, juce::Colour (0xb0ffe3a0));

    // Cables: sombras, fichas, cuerpos.
    for (const auto& c : cables) drawCable (g, c.rope.path(), c.color, true);
    if (drag.active) drawCable (g, drag.rope.path(), drag.color, true);
    for (const auto& c : cables)
    {
        drawPlug (g, jacks[(size_t) c.src].base, c.color);
        drawPlug (g, jacks[(size_t) c.dst].base, c.color);
    }
    if (drag.active) drawPlug (g, jacks[(size_t) drag.fixedJack].base, drag.color);
    for (const auto& c : cables) drawCable (g, c.rope.path(), c.color, false);
    if (drag.active)
    {
        drawCable (g, drag.rope.path(), drag.color, false);
        drawPlug (g, drag.free + juce::Point<float> (0.0f, A.plugTopZ * A.sinT), drag.color, 1.0f);
    }

    // Globo con el valor de la perilla activa o la descripción del jack.
    const int shownKnob = dragKnob >= 0 ? dragKnob : hoverKnob;
    const int shownJack = shownKnob < 0 ? hoverJack : -1;
    if (shownKnob >= 0 || shownJack >= 0)
    {
        juce::String title, value;
        juce::Point<float> anchor;
        float rad;
        if (shownKnob >= 0)
        {
            const auto& k = knobs[(size_t) shownKnob];
            title = paramName (k.index).toUpperCase();
            value = formatKnob (k.index, knobValue (k));
            anchor = k.base; rad = k.radius;
        }
        else
        {
            title = jackDescription (shownJack, value);
            anchor = jacks[(size_t) shownJack].base; rad = layout::jackRadius;
        }
        struct { juce::Point<float> base; float radius; } k { anchor, rad };
        const juce::Font f1 (juce::FontOptions ("Helvetica Neue", 11.0f, juce::Font::bold));
        const juce::Font f2 (juce::FontOptions ("Helvetica Neue", 13.5f, juce::Font::plain));
        const float w = std::max (juce::GlyphArrangement::getStringWidth (f1, title), juce::GlyphArrangement::getStringWidth (f2, value)) + 22.0f;
        juce::Rectangle<float> r (k.base.x - w * 0.5f, k.base.y - k.radius * 2.0f - 46.0f, w, 40.0f);
        if (r.getY() < 4.0f) r.setY (k.base.y + k.radius + 10.0f);
        r = r.withX (juce::jlimit (4.0f, (float) getWidth() - w - 4.0f, r.getX()));
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0, 3), 6.0f);
        g.setColour (juce::Colour (0xf0121214));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (0x40ffffff));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (juce::Colour (0xffa9a9a4));
        g.setFont (f1);
        g.drawText (title, r.withHeight (20.0f).translated (0, 3), juce::Justification::centred);
        g.setColour (juce::Colour (0xfff2efe6));
        g.setFont (f2);
        g.drawText (value, r.withTrimmedTop (18.0f), juce::Justification::centred);
    }
}

// ───────────── animación ─────────────
void PanelView::timerCallback()
{
    // Live esconde la ventana del editor sin cerrarlo: escondido no se anima ni se repinta (QA en Live 2026-10-22, H3:
    // ~23 % de un núcleo). Al volver, lo que cambió difiere de lo mostrado y se redibuja solo.
    if (! dh::reallyShowing (*this)) return;
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = juce::jlimit (0.001f, 0.033f, (float) ((now - lastTime) * 0.001));
    lastTime = now;
    // Sólo se redibuja lo que cambió: redibujar el panel entero (una imagen de 3384 px escalada) en
    // cada parpadeo de un LED costaba ~20 % de un núcleo con la ventana abierta.
    juce::RectangleList<int> dirty;

    const int bubbleKnob = dragKnob >= 0 ? dragKnob : hoverKnob;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        const float v = knobValue (k);
        if (v != k.shown)
        {
            k.shown = v;
            const auto& sheet = k.big ? A.knobBig : A.knobSmall;
            const float s = (float) sheet.size * 0.5f;
            dirty.add (juce::Rectangle<float> (k.base.x - sheet.anchorX * 0.5f, k.base.y - sheet.anchorY * 0.5f, s, s).expanded (4.0f).toNearestInt());
            // El globo con el valor está encima de la perilla (o debajo, cerca del borde): también se redibuja, si no
            // el número quedaba congelado y cortado mientras se arrastra o automatiza.
            if ((int) i == bubbleKnob) dirty.add (juce::Rectangle<float> (k.base.x - 140.0f, k.base.y - k.radius * 2.0f - 56.0f, 280.0f, k.radius * 4.0f + 120.0f).toNearestInt());
        }
    }
    const float ledVals[4] = { proc.ledMg.load() ? 1.0f : 0.0f, juce::jlimit (0.0f, 1.0f, proc.ledEg.load()),
                               proc.ledEspPeak.load() ? 1.0f : 0.0f, proc.ledEspTrig.load() ? 1.0f : 0.0f };
    for (int i = 0; i < 4; ++i)
        if (std::abs (ledVals[i] - ledShown[(size_t) i]) > 0.02f)
        {
            ledShown[(size_t) i] = ledVals[i];
            const auto c = A.toView (layout::leds[i].x, layout::leds[i].y, 2.0f);
            dirty.add (juce::Rectangle<int> ((int) c.x - 18, (int) c.y - 18, 36, 36));
        }

    // Física: dos subpasos por cuadro para que la cuerda no tiemble. Se redibuja la zona de la
    // cuerda antes y después de moverse (más la sombra corrida y las fichas).
    auto ropeArea = [] (const Rope& r) { return r.path().getBounds().expanded (24.0f).withTrimmedTop (-60.0f).toNearestInt(); };
    for (auto& c : cables)
    {
        if (c.rope.settled() && ! drag.active) continue;
        dirty.add (ropeArea (c.rope));
        for (int s = 0; s < 2; ++s) c.rope.step (jacks[(size_t) c.src].plugTop, jacks[(size_t) c.dst].plugTop, dt * 0.5f);
        dirty.add (ropeArea (c.rope));
    }
    if (drag.active)
    {
        dirty.add (ropeArea (drag.rope));
        for (int s = 0; s < 2; ++s) drag.rope.step (jacks[(size_t) drag.fixedJack].plugTop, drag.free, dt * 0.5f);
        dirty.add (ropeArea (drag.rope));
        dirty.add (juce::Rectangle<float> (60.0f, 90.0f).withCentre (drag.free).toNearestInt());
    }
    dirty.consolidate();
    for (const auto& r : dirty) repaint (r);
}

// ───────────── mouse ─────────────
void PanelView::mouseMove (const juce::MouseEvent& e)
{
    const int k = knobAt (e.position);
    const int j = k < 0 ? jackAt (e.position) : -1;
    if (k != hoverKnob || j != hoverJack)
    {
        hoverKnob = k; hoverJack = j;
        setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor
                               : (j >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
        repaint();
    }
}

void PanelView::mouseExit (const juce::MouseEvent&)
{
    if (hoverKnob >= 0 || hoverJack >= 0) { hoverKnob = hoverJack = -1; repaint(); }
}

juce::String PanelView::nextColor()
{
    return kColors[(colorCycle++) % 5];   // el negro queda para elegirlo a mano
}

bool PanelView::canConnect (int fixedJack, int target) const
{
    if (fixedJack < 0 || target < 0 || target == fixedJack) return false;
    const bool fixedOut = layout::jacks[fixedJack].isOutput, targetOut = layout::jacks[target].isOutput;
    if (fixedOut == targetOut) return false;
    // Una entrada admite una sola ficha; las salidas admiten varias (fichas apilables).
    return true;
}

void PanelView::commitCables (std::vector<Cable> cs)
{
    proc.setCables (std::move (cs));
}

void PanelView::mouseDown (const juce::MouseEvent& e)
{
    lastMouse = e.position;
    const int k = knobAt (e.position);
    if (k >= 0 && ! e.mods.isPopupMenu())
    {
        dragKnob = k;
        dragStartValue = knobValue (knobs[(size_t) k]);
        dragAccum = 0.0f;
        if (auto* prm = knobs[(size_t) k].param) prm->beginChangeGesture();
        e.source.enableUnboundedMouseMovement (true);
        repaint();
        return;
    }
    const int j = jackAt (e.position);
    if (j < 0) return;
    if (e.mods.isPopupMenu()) { showJackMenu (j); return; }

    auto cs = proc.getCables();
    // Alt-clic: desenchufa todo lo que hay en ese jack.
    if (e.mods.isAltDown())
    {
        cs.erase (std::remove_if (cs.begin(), cs.end(), [j] (auto& c) { return c.src == j || c.dst == j; }), cs.end());
        commitCables (cs);
        return;
    }
    // Si hay un cable enchufado, se levanta su extremo (el último que se enchufó). Con Shift, en cambio, sale un
    // cable más (una salida puede alimentar varias entradas, como en el AT42).
    for (int i = (int) cs.size() - 1; i >= 0 && ! e.mods.isShiftDown(); --i)
    {
        auto c = cs[(size_t) i];
        if (c.src != j && c.dst != j) continue;
        drag.active = true;
        drag.fixedJack = c.src == j ? c.dst : c.src;
        drag.color = c.color;
        drag.free = e.position;
        // Conserva la forma del cable que se levanta.
        for (auto& cv : cables)
            if (cv.src == c.src && cv.dst == c.dst)
            {
                // La cuerda del arrastre va del extremo fijo (índice 0) a la mano.
                drag.rope = cv.rope;
                if (c.src == j) { std::reverse (drag.rope.p.begin(), drag.rope.p.end()); std::reverse (drag.rope.prev.begin(), drag.rope.prev.end()); }
            }
        cs.erase (cs.begin() + i);
        commitCables (cs);
        return;
    }
    // Jack libre: sale un cable nuevo (los anillos de los jacks donde puede ir aparecen todos de una vez).
    repaint();
    drag.active = true;
    drag.fixedJack = j;
    drag.color = nextColor();
    drag.free = e.position;
    drag.rope.init (jacks[(size_t) j].plugTop, e.position, 1.0f, 60.0f);
}

void PanelView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragKnob >= 0)
    {
        auto& k = knobs[(size_t) dragKnob];
        // Arriba o a la derecha sube, abajo o a la izquierda baja: el arrastre funciona en los dos ejes.
        const float dy = (lastMouse.y - e.position.y) + (e.position.x - lastMouse.x);
        lastMouse = e.position;
        const float fine = e.mods.isShiftDown() || e.mods.isCommandDown() ? 0.2f : 1.0f;
        if (k.steps > 0)
        {
            dragAccum += dy;
            const float stepPx = 28.0f;
            if (std::abs (dragAccum) >= stepPx)
            {
                const float v = knobValue (k) + (dragAccum > 0 ? 1.0f : -1.0f);
                dragAccum = 0.0f;
                setKnobValue (k, v);
            }
        }
        else
            setKnobValue (k, knobValue (k) + dy * fine / 220.0f);
        return;
    }
    if (drag.active)
    {
        drag.free = e.position;
        const int j = jackAt (e.position);
        hoverJack = canConnect (drag.fixedJack, j) ? j : -1;
    }
}

void PanelView::mouseUp (const juce::MouseEvent& e)
{
    if (dragKnob >= 0)
    {
        if (auto* prm = knobs[(size_t) dragKnob].param) prm->endChangeGesture();
        e.source.enableUnboundedMouseMovement (false);
        dragKnob = -1;
        repaint();
        return;
    }
    if (! drag.active) return;
    const int target = jackAt (e.position);
    auto cs = proc.getCables();
    if (canConnect (drag.fixedJack, target))
    {
        Cable c;
        c.src = layout::jacks[drag.fixedJack].isOutput ? drag.fixedJack : target;
        c.dst = layout::jacks[drag.fixedJack].isOutput ? target : drag.fixedJack;
        c.color = drag.color;
        // La entrada sólo admite una ficha: la anterior sale.
        cs.erase (std::remove_if (cs.begin(), cs.end(), [&] (auto& x) { return x.dst == c.dst; }), cs.end());
        cs.push_back (c);
        // La cuerda nueva hereda la forma de la arrastrada.
        Rope r = drag.rope;
        r.length = jacks[(size_t) c.src].plugTop.getDistanceFrom (jacks[(size_t) c.dst].plugTop) * 1.18f + 34.0f;
        if (! layout::jacks[drag.fixedJack].isOutput) { std::reverse (r.p.begin(), r.p.end()); std::reverse (r.prev.begin(), r.prev.end()); }
        drag.active = false;
        hoverJack = -1;
        commitCables (cs);
        for (auto& cv : cables) if (cv.src == c.src && cv.dst == c.dst) { cv.rope = r; cv.rope.energy = 1.0f; }
    }
    else
    {
        drag.active = false;
        hoverJack = -1;
    }
    repaint();
}

void PanelView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int k = knobAt (e.position);
    if (k < 0) return;
    const auto normal = Engine::normalSetting();
    auto& kv = knobs[(size_t) k];
    if (auto* prm = kv.param)
    {
        prm->beginChangeGesture();
        setKnobValue (kv, normal[(size_t) k]);
        prm->endChangeGesture();
    }
}

void PanelView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int k = knobAt (e.position);
    if (k < 0) return;
    auto& kv = knobs[(size_t) k];
    const float d = (w.isReversed ? -w.deltaY : w.deltaY);
    if (kv.steps > 0)
    {
        if (std::abs (d) > 0.05f) setKnobValue (kv, knobValue (kv) + (d > 0 ? 1.0f : -1.0f));
    }
    else
        setKnobValue (kv, knobValue (kv) + d * (e.mods.isShiftDown() ? 0.02f : 0.1f));
}

void PanelView::showJackMenu (int j)
{
    auto cs = proc.getCables();
    juce::PopupMenu m, colors;
    for (int i = 0; i < 6; ++i) colors.addItem (100 + i, juce::String (kColors[i]).substring (0, 1).toUpperCase() + juce::String (kColors[i]).substring (1));
    bool any = false;
    for (auto& c : cs) any = any || c.src == j || c.dst == j;
    m.addSectionHeader (juce::String (layout::jacks[j].id).replace ("_", " ").toUpperCase());
    m.addSubMenu ("Cable color", colors, any);
    m.addItem (1, "Unplug", any);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, j] (int r) {
        auto cs2 = proc.getCables();
        if (r == 1)
            cs2.erase (std::remove_if (cs2.begin(), cs2.end(), [j] (auto& c) { return c.src == j || c.dst == j; }), cs2.end());
        else if (r >= 100)
            for (auto& c : cs2) if (c.src == j || c.dst == j) c.color = kColors[r - 100];
        else return;
        commitCables (cs2);
    });
}

} // namespace dh
