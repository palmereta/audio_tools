#pragma once
// Páginas de Valence sin el átomo (la Simple, con el átomo, está en SimplePage.h): Layers (las cuatro capas en tiras y
// la capa elegida con todas sus filas de pasos), Harmony (la entrada, la armonía, el tiempo y el núcleo en un teclado)
// y Outputs (la de Orrery con cuatro capas).
// Repintado: nada se repinta por reloj si no cambió. El cursor de cada capa repinta sólo las dos celdas que deja y
// toma, y los parámetros que cambian el dibujo de una capa (largo, euclidiano, figura...) se comparan en una firma.

#include "Widgets.h"

namespace dh::val::ui {

class Page : public juce::Component
{
public:
    explicit Page (ValenceProcessor& p) : proc (p) {}
    virtual void refresh() { repaint(); }
protected:
    // ¿La página se ve? isVisible() es una bandera; dh::reallyShowing pregunta a la ventana (en Linux, una ida y vuelta al
    // servidor X), así que se consulta a lo sumo cuatro veces por segundo.
    bool onScreen()
    {
        if (! isVisible()) { shownAt = 0; return false; }
        const auto now = juce::Time::getMillisecondCounter();
        if (shownAt == 0 || now - shownAt > 250) { shown = dh::reallyShowing (*this); shownAt = now; }
        return shown;
    }
    ValenceProcessor& proc;
    std::vector<std::unique_ptr<juce::Component>> owned;
    template <typename T, typename... A> T* make (A&&... a) { auto c = std::make_unique<T> (std::forward<A> (a)...); auto* p = c.get(); owned.push_back (std::move (c)); addAndMakeVisible (p); return p; }
private:
    juce::uint32 shownAt = 0; bool shown = false;
};

// Lo que una capa necesita para dibujarse, leído de los parámetros.
struct LayerLook
{
    int len = 16, pulses = 0, rot = 0, rate = 6, pattern = 0, velMode = 0, cc = -1, mult = 1, key = 0, scale = 0;
    bool on = false;
    static LayerLook read (const ValenceProcessor& p, int l)
    {
        const juce::String pre = "l" + juce::String (l + 1) + "_";
        auto i = [&] (const char* id) { return (int) std::lround (p.value (pre + id)); };
        LayerLook k;
        k.len = juce::jlimit (1, kMaxSteps, i ("length")); k.pulses = i ("eu_pulses"); k.rot = i ("eu_rot"); k.rate = i ("rate");
        k.pattern = i ("pattern"); k.velMode = i ("vel_mode"); k.cc = i ("cc"); k.mult = i ("mult"); k.on = p.value (pre + "on") > 0.5f;
        k.key = (int) std::lround (p.value ("key")); k.scale = (int) std::lround (p.value ("scale"));      // los nombres de la fila Note
        return k;
    }
    bool hit (const Step& s, int k) const { return pulses > 0 ? Engine::euclidHit (k, len, pulses, rot) : s.on; }
    bool operator== (const LayerLook&) const = default;
};

// La tira compacta de una capa (página Layers, arriba): encendido (clic), acento (Mayúsculas), ratchet (Alt), ligadura
// (Cmd/Ctrl), el cursor y los pasos que suenan por el euclidiano. Las filas son estado, no parámetros: setStep.
class StepStrip : public juce::Component
{
public:
    StepStrip (ValenceProcessor& p, int l) : proc (p), layer (l) { setOpaque (true); }
    std::function<void (int)> onTouch;      // la capa tocada (la página la elige y repinta su editor)
    int cursor = -1;
    bool selected = false;
    LayerLook look;
    void paint (juce::Graphics& g) override
    {
        cache.draw (g, *this, [this] (juce::Graphics& ig) { paintCells (ig); });
        if (cursor >= 0)
        {
            const float w = cellW();
            g.setColour (col::text); g.drawRoundedRectangle (juce::Rectangle<float> ((float) cursor * w + 0.5f, 0.5f, w - 1.0f, (float) getHeight() - 1.0f), 3.0f, 1.6f);
        }
    }
    void invalidate() { cache.invalidate (*this); }
    void repaintStep (int k) { if (k >= 0) repaint (stepArea (k)); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int k = juce::jlimit (0, kMaxSteps - 1, (int) ((float) e.x / cellW()));
        auto s = proc.getRows()[(size_t) layer][(size_t) k];
        if (e.mods.isShiftDown()) s.accent = ! s.accent;
        else if (e.mods.isAltDown()) s.ratchet = (uint8_t) (s.ratchet % 4 + 1);
        else if (e.mods.isCommandDown()) s.tie = ! s.tie;
        else s.on = ! s.on;
        proc.setStep (layer, k, s);
        cache.update (*this, stepArea (k), [this] (juce::Graphics& ig) { paintCells (ig); });
        if (onTouch) onTouch (layer);
    }
    void mouseEnter (const juce::MouseEvent&) override { showHelp ("Steps of layer " + juce::String (layer + 1) + ": click turns a step on or off, Shift+click accents it, Alt+click cycles the ratchet (1-4), Cmd/Ctrl+click ties it to the next. Opacity is the probability; with Euclid pulses the generator decides which steps play. All rows below"); }
    void mouseExit (const juce::MouseEvent&) override { showHelp ({}); }
private:
    ValenceProcessor& proc; int layer;
    PaintCache cache;
    float cellW() const { return (float) getWidth() / (float) kMaxSteps; }
    juce::Rectangle<int> stepArea (int k) const { return juce::Rectangle<float> ((float) k * cellW() - 1.0f, 0.0f, cellW() + 2.0f, (float) getHeight()).getSmallestIntegerContainer(); }
    void paintCells (juce::Graphics& g)
    {
        g.fillAll (selected ? col::raised : col::surface);
        const auto rows = proc.getRows();
        const auto& r = rows[(size_t) layer];
        const auto c = col::layer (layer);
        const float w = cellW(), h = (float) getHeight();
        const auto clip = g.getClipBounds();
        for (int k = juce::jmax (0, (int) ((float) clip.getX() / w) - 1); k < kMaxSteps && (float) k * w < (float) clip.getRight(); ++k)
        {
            const auto cell = juce::Rectangle<float> (k * w + 1.0f, 1.0f, w - 2.0f, h - 2.0f);
            if (k >= look.len) { g.setColour (col::border.withAlpha (0.35f)); g.drawRoundedRectangle (cell, 3.0f, 1.0f); continue; }
            const auto& s = r[(size_t) k];
            const bool hit = look.hit (s, k);
            const float alpha = hit ? 0.25f + 0.75f * (float) s.prob / 100.0f : 0.0f;
            g.setColour (c.withAlpha (alpha * (look.on ? 1.0f : 0.4f))); g.fillRoundedRectangle (cell, 3.0f);
            g.setColour (hit ? c : col::border); g.drawRoundedRectangle (cell, 3.0f, 1.0f);
            if (s.accent) { g.setColour (col::text); g.fillEllipse (cell.getCentreX() - 2.0f, cell.getY() + 3.0f, 4.0f, 4.0f); }
            if (s.ratchet > 1) { g.setColour (col::text.withAlpha (0.8f)); g.setFont (font (10.0f)); g.drawText (juce::String ((int) s.ratchet), cell.toNearestInt(), juce::Justification::centredBottom); }
            if (s.tie) { g.setColour (col::text.withAlpha (0.7f)); g.drawHorizontalLine ((int) cell.getBottom() - 3, cell.getX() + 2.0f, cell.getRight() + 2.0f); }
        }
    }
};

// Todas las filas de pasos de una capa, "desenrolladas": cada fila una tira de 32 celdas.
//  - llaves (On, Accent, Tie): clic, o arrastrar para pintar varias;
//  - barras (Velocity, Probability, CC): el valor es la altura donde se hace clic; arrastrar dibuja;
//  - valores (Ratchet, Octave, Note, Finger): arrastrar hacia arriba o abajo, o la rueda.
// Doble clic: el valor por defecto. Clic derecho: menú de la fila (restablecer, llenar, rotar).
class RowEditor : public juce::Component
{
public:
    explicit RowEditor (ValenceProcessor& p) : proc (p) { setOpaque (true); }
    std::function<void()> onEdit;
    int layer = 0, cursor = -1;
    LayerLook look;

    enum Kind { Toggle, Bar, Value };
    enum RowId { ROn, RVel, RAccent, RTie, RRatchet, RProb, ROct, RNote, RCC, RFinger, RowCount };
    struct Row { const char* name; Kind kind; int lo, hi, def, h; const char* help; };
    static const Row& row (int i)
    {
        static const Row R[RowCount] = {
            { "On", Toggle, 0, 1, 1, 24, "click or drag across to switch steps on and off. With Euclid pulses the generator decides (hollow: off in the rows, filled: plays)" },
            { "Velocity", Bar, 1, 127, 100, 44, "drag up or down to draw. Used when the layer's Velocity is Steps" },
            { "Accent", Toggle, 0, 1, 0, 24, "adds the Accent amount (Harmony page) to the step's velocity" },
            { "Tie", Toggle, 0, 1, 0, 24, "holds the note into the next step: the same note sustains, another one plays legato" },
            { "Ratchet", Value, 1, 4, 1, 26, "notes inside the step, 1 to 4: drag up or down, or the wheel" },
            { "Probability", Bar, 0, 100, 100, 44, "the chance the step plays; the draw is reproducible (the same Variation plays the same)" },
            { "Octave", Value, -2, 2, 0, 26, "octave of this step, on top of the layer's Register" },
            { "Note", Value, -1, 21, -1, 26, "a fixed note of the scale instead of the pattern's, counted from the key (-: the pattern's note)" },
            { "CC", Bar, -1, 127, -1, 44, "value sent with the layer's CC row number; at the bottom nothing is sent" },
            { "Finger", Value, 0, 15, 0, 26, "with the Finger order pattern: which note of the chord the step plays (1 is the lowest)" } };
        return R[std::clamp (i, 0, RowCount - 1)];
    }
    static constexpr int labelW = 96, headH = 16, gap = 4;
    static int neededHeight() { int h = headH; for (int i = 0; i < RowCount; ++i) h += row (i).h + gap; return h; }

    static int get (const Step& s, int r)
    {
        switch (r)
        {
            case ROn: return s.on ? 1 : 0; case RVel: return s.vel; case RAccent: return s.accent ? 1 : 0; case RTie: return s.tie ? 1 : 0;
            case RRatchet: return s.ratchet; case RProb: return s.prob; case ROct: return s.oct; case RNote: return s.fixed; case RCC: return s.cc;
            default: return s.finger;
        }
    }
    static void set (Step& s, int r, int v)
    {
        v = juce::jlimit (row (r).lo, row (r).hi, v);
        switch (r)
        {
            case ROn: s.on = v != 0; break; case RVel: s.vel = (uint8_t) v; break; case RAccent: s.accent = v != 0; break; case RTie: s.tie = v != 0; break;
            case RRatchet: s.ratchet = (uint8_t) v; break; case RProb: s.prob = (uint8_t) v; break; case ROct: s.oct = (int8_t) v; break;
            case RNote: s.fixed = (int8_t) v; break; case RCC: s.cc = (int8_t) v; break; default: s.finger = (int8_t) v; break;
        }
    }
    // Una fila que hoy no hace nada en esta capa se dibuja apagada (y la ayuda dice por qué).
    bool rowActive (int r) const
    {
        if (r == RVel) return look.velMode == VelStep;
        if (r == RCC) return look.cc >= 0;
        if (r == RFinger) return look.pattern == PatFinger;
        return true;
    }

    void paint (juce::Graphics& g) override
    {
        cache.draw (g, *this, [this] (juce::Graphics& ig) { paintCells (ig); });
        // El cursor: una columna sobre todas las filas, encima del caché.
        if (cursor >= 0) { g.setColour (col::text.withAlpha (0.85f)); g.drawRoundedRectangle (columnRect (cursor).reduced (0.5f), 4.0f, 1.5f); }
    }
    void invalidate() { cache.invalidate (*this); }
    void repaintColumn (int k) { if (k >= 0) repaint (columnRect (k).expanded (2.0f).getSmallestIntegerContainer()); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragRow = rowAt (e.y); lastK = -1;
        const int k = stepAt (e.x);
        if (dragRow < 0 || k < 0) return;
        if (e.mods.isPopupMenu()) { rowMenu (dragRow, k); dragRow = -1; return; }
        const auto& D = row (dragRow);
        const auto s = proc.getRows()[(size_t) layer][(size_t) k];
        if (D.kind == Toggle) paintVal = get (s, dragRow) != 0 ? 0 : 1;
        if (D.kind == Value) { dragK = k; startVal = get (s, dragRow); return; }
        drawTo (k, e.y);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragRow < 0) return;
        const auto& D = row (dragRow);
        if (D.kind == Value)
        {
            const float px = dragRow == RNote ? 6.0f : 10.0f;
            setCell (dragK, dragRow, startVal - juce::roundToInt ((float) e.getDistanceFromDragStartY() / px));
            return;
        }
        const int k = juce::jlimit (0, kMaxSteps - 1, (int) ((float) (e.x - labelW) / cellW()));
        drawTo (k, e.y);
    }
    void mouseUp (const juce::MouseEvent&) override { dragRow = -1; }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int r = rowAt (e.y), k = stepAt (e.x);
        if (r >= 0 && k >= 0 && row (r).kind != Toggle) setCell (k, r, row (r).def);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int r = rowAt (e.y), k = stepAt (e.x);
        if (r < 0 || k < 0 || row (r).kind == Toggle) return;
        if (const int n = wheel.take (w); n != 0)
        {
            const auto& D = row (r);
            const int unit = D.kind == Bar ? std::max (1, (D.hi - D.lo) / 25) : 1;
            setCell (k, r, get (proc.getRows()[(size_t) layer][(size_t) k], r) + n * unit);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { hover (e.x, e.y); }
    void mouseEnter (const juce::MouseEvent& e) override { hover (e.x, e.y); }
    void mouseExit (const juce::MouseEvent&) override { hoverKey = {}; showHelp ({}); }

private:
    ValenceProcessor& proc;
    PaintCache cache;
    int dragRow = -1, dragK = -1, startVal = 0, paintVal = 1, lastK = -1, lastV = 0;
    void paintCells (juce::Graphics& g)
    {
        g.fillAll (col::surface);
        const auto rows = proc.getRows();
        const auto& R = rows[(size_t) layer];
        const auto c = col::layer (layer);
        const auto clip = g.getClipBounds();
        const float w = cellW();
        const int k0 = juce::jmax (0, (int) ((float) (clip.getX() - labelW) / w) - 1);
        const int k1 = juce::jmin (kMaxSteps - 1, (int) ((float) (clip.getRight() - labelW) / w) + 1);
        // Números de paso.
        if (clip.getY() < headH)
        {
            g.setFont (font (10.5f));
            for (int k = k0; k <= k1; ++k)
            {
                g.setColour (k >= look.len ? col::faint.withAlpha (0.5f) : (k % 4 == 0 ? col::dim : col::faint));
                g.drawText (juce::String (k + 1), cellRect (k, 0).withY (0).withHeight (headH).toNearestInt(), juce::Justification::centred);
            }
        }
        for (int r = 0; r < RowCount; ++r)
        {
            const auto& D = row (r);
            const int y = rowY (r);
            if (y > clip.getBottom() || y + D.h < clip.getY()) continue;
            const bool act = rowActive (r);
            if (clip.getX() < labelW)
            {
                g.setColour (act ? col::dim : col::faint); g.setFont (font (12.0f));
                g.drawText (D.name, 0, y, labelW - 8, D.h, juce::Justification::centredLeft);
            }
            const float a = act ? 1.0f : 0.35f;
            for (int k = k0; k <= k1; ++k)
            {
                const auto cell = cellRect (k, r);
                const bool in = k < look.len;
                const float ia = in ? a : a * 0.25f;
                const auto& s = R[(size_t) k];
                const int v = get (s, r);
                g.setColour (col::bg); g.fillRoundedRectangle (cell, 3.0f);
                if (D.kind == Toggle)
                {
                    const bool gen = r == ROn && look.pulses > 0;
                    const bool on = gen ? look.hit (s, k) : v != 0;
                    if (on) { g.setColour (c.withAlpha ((gen ? 0.45f : 0.8f) * ia)); g.fillRoundedRectangle (cell.reduced (1.0f), 3.0f); }
                    if (gen && s.on != on) { g.setColour (col::text.withAlpha (0.5f * ia)); g.fillEllipse (cell.getCentreX() - 1.5f, cell.getCentreY() - 1.5f, 3.0f, 3.0f); }
                }
                else if (D.kind == Bar)
                {
                    const float f = (float) (v - D.lo) / (float) (D.hi - D.lo);
                    if (f > 0.0f) { g.setColour (c.withAlpha (0.75f * ia)); g.fillRect (cell.reduced (2.0f, 2.0f).withTrimmedTop ((cell.getHeight() - 4.0f) * (1.0f - f))); }
                }
                else
                {
                    const bool neutral = v == D.def;
                    g.setColour ((neutral ? col::faint : col::text).withAlpha (ia)); g.setFont (font (12.0f, ! neutral));
                    g.drawText (valueText (r, v), cell.toNearestInt(), juce::Justification::centred);
                }
                g.setColour ((in ? col::border : col::border.withAlpha (0.4f))); g.drawRoundedRectangle (cell, 3.0f, 1.0f);
            }
        }
    }

    dh::WheelSteps wheel;
    juce::String hoverKey;

    float cellW() const { return (float) (getWidth() - labelW) / (float) kMaxSteps; }
    int rowY (int r) const { int y = headH; for (int i = 0; i < r; ++i) y += row (i).h + gap; return y; }
    juce::Rectangle<float> cellRect (int k, int r) const { return { (float) labelW + (float) k * cellW() + 1.0f, (float) rowY (r), cellW() - 2.0f, (float) row (r).h }; }
    juce::Rectangle<float> columnRect (int k) const { return { (float) labelW + (float) k * cellW() - 1.0f, (float) headH - 2.0f, cellW() + 2.0f, (float) (rowY (RowCount - 1) + row (RowCount - 1).h - headH + 4) }; }
    int rowAt (int y) const { for (int r = 0; r < RowCount; ++r) { const int y0 = rowY (r); if (y >= y0 - gap / 2 && y < y0 + row (r).h + gap / 2) return r; } return -1; }
    int stepAt (int x) const { if (x < labelW) return -1; return juce::jlimit (0, kMaxSteps - 1, (int) ((float) (x - labelW) / cellW())); }

    juce::String valueText (int r, int v) const
    {
        if (r == ROct) return v == 0 ? juce::String ("0") : (v > 0 ? "+" : "") + juce::String (v);
        if (r == RNote)
        {
            if (v < 0) return "-";
            const auto& sc = scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, (int) std::lround (proc.value ("scale")))];
            const int semis = degreeSemis (sc, v), key = (int) std::lround (proc.value ("key"));
            return juce::String (noteName (key + semis)) + juce::String::repeatedString ("'", (key % 12 + semis) / 12);
        }
        if (r == RFinger) return juce::String (v + 1);
        return juce::String (v);
    }
    int valueAtY (int r, int y) const
    {
        const auto& D = row (r);
        const float y0 = (float) rowY (r) + 2.0f, h = (float) D.h - 4.0f;
        const float f = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) y - y0) / h);
        return D.lo + juce::roundToInt (f * (float) (D.hi - D.lo));
    }
    // Llaves y barras: del último paso tocado al actual, sin huecos aunque el mouse vaya rápido.
    void drawTo (int k, int y)
    {
        const auto& D = row (dragRow);
        const int v = D.kind == Toggle ? paintVal : valueAtY (dragRow, y);
        if (lastK < 0) { setCell (k, dragRow, v); lastK = k; lastV = v; return; }
        const int n = std::abs (k - lastK);
        for (int i = 1; i <= n; ++i)
        {
            const int kk = lastK + (k > lastK ? i : -i);
            setCell (kk, dragRow, D.kind == Toggle ? v : lastV + (v - lastV) * i / n);
        }
        if (n == 0) setCell (k, dragRow, v);
        lastK = k; lastV = v;
    }
    void setCell (int k, int r, int v)
    {
        auto s = proc.getRows()[(size_t) layer][(size_t) k];
        const int before = get (s, r);
        set (s, r, v);
        if (get (s, r) == before) return;
        proc.setStep (layer, k, s);
        cache.update (*this, columnRect (k).expanded (2.0f).getSmallestIntegerContainer(), [this] (juce::Graphics& ig) { paintCells (ig); });
        if (onEdit) onEdit();
        showValue (r, k, get (s, r));
    }
    void showValue (int r, int k, int v)
    {
        juce::String t = juce::String (row (r).name) + ", step " + juce::String (k + 1) + ": ";
        if (row (r).kind == Toggle) t << (v != 0 ? "on" : "off");
        else if (r == RCC && v < 0) t << "nothing sent";
        else if (r == RProb) t << v << " %";
        else t << valueText (r, v);
        showHelp (t);
    }
    void hover (int x, int y)
    {
        const int r = rowAt (y), k = stepAt (x);
        const juce::String key = juce::String (r) + ":" + juce::String (k);
        if (key == hoverKey) return;
        hoverKey = key;
        if (r < 0) { showHelp ("Step rows of layer " + juce::String (layer + 1) + ": right-click a row to reset, fill or rotate it"); return; }
        juce::String t = juce::String (row (r).name) + ": " + row (r).help;
        if (! rowActive (r))
            t << (r == RVel ? " (now off: set Velocity to Steps)" : r == RCC ? " (now off: choose a CC row number)" : " (now off: the pattern is not Finger order)");
        if (k >= 0 && row (r).kind != Toggle)
        {
            const int v = get (proc.getRows()[(size_t) layer][(size_t) k], r);
            t = juce::String (row (r).name) + ", step " + juce::String (k + 1) + ": " + (r == RCC && v < 0 ? juce::String ("nothing sent") : valueText (r, v) + (r == RProb ? " %" : ""))
                + ". " + row (r).help;
        }
        showHelp (t);
    }
    void rowMenu (int r, int k)
    {
        juce::PopupMenu m;
        m.addSectionHeader (juce::String (row (r).name) + " row");
        m.addItem (1, "Reset the row");
        m.addItem (2, "Fill with step " + juce::String (k + 1) + "'s value");
        m.addItem (3, "Rotate left");
        m.addItem (4, "Rotate right");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, r, k] (int id) {
            if (id <= 0) return;
            auto rows = proc.getRows();
            auto& R = rows[(size_t) layer];
            const int len = look.len;
            std::vector<int> v ((size_t) len);
            for (int i = 0; i < len; ++i) v[(size_t) i] = get (R[(size_t) i], r);
            const int fill = get (R[(size_t) k], r);
            for (int i = 0; i < len; ++i)
            {
                int x = v[(size_t) i];
                if (id == 1) x = row (r).def;
                else if (id == 2) x = fill;
                else if (id == 3) x = v[(size_t) ((i + 1) % len)];
                else x = v[(size_t) ((i - 1 + len) % len)];
                set (R[(size_t) i], r, x);
            }
            proc.setRows (rows);
            invalidate();
            if (onEdit) onEdit();
        });
    }
};

class LayersPage : public Page, private juce::Timer
{
public:
    explicit LayersPage (ValenceProcessor& p) : Page (p)
    {
        editor = make<RowEditor> (p);
        editor->onEdit = [this] { lay[(size_t) sel].strip->invalidate(); };
        for (int l = 0; l < kLayers; ++l)
        {
            const juce::String pre = "l" + juce::String (l + 1) + "_";
            auto& L = lay[(size_t) l];
            L.on = make<ParamBox> (p, pre + "on", "On", "switch layer " + juce::String (l + 1) + " on or off", false);
            L.on->tint = col::layer (l);
            L.strip = make<StepStrip> (p, l);
            L.strip->onTouch = [this] (int t) { select (t); editor->invalidate(); };
            for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                     { "length", "Length", "steps per cycle, 1-32; primes (5, 7, 11, 13...) drift against each other" },
                     { "rate", "Rate", "the figure of one step: straight, triplet (T) or dotted (D)" },
                     { "mult", "Rate x", "multiplies the figure: x2 makes each step twice as long" },
                     { "pattern", "Pattern", "how the layer walks through the chord" },
                     { "octaves", "Octaves", "range the pattern spans" }, { "oct_mode", "Octave mode", "Stack climbs an octave per pass, Alternate goes note by note, Random draws it" },
                     { "octave", "Register", "octave shift of the layer (the radius of its orbit in the atom)" },
                     { "shift", "Index shift", "starts the pattern on another note of the chord" },
                     { "gate", "Gate", "note length, part of the step" }, { "swing", "Swing", "delays every other step; 50 % is straight" },
                     { "eu_pulses", "Euclid", "pulses spread evenly over the steps (Steps: the On row decides)" },
                     { "eu_rot", "Rotate", "turns the Euclid pattern" },
                     { "vel_mode", "Velocity", "Fixed (the value at the right), as played, or the Velocity row" },
                     { "velocity", "Fixed vel.", "the velocity with Velocity on Fixed" },
                     { "humanize", "Humanize", "reproducible lateness, up to this many ms (never early)" },
                     { "mutate", "Mutate", "every cycle, a step changes a little (its own seed)" },
                     { "restart", "New chord", "Restart goes back to step 0 with each new chord; Keep phase does not" },
                     { "cc", "CC row", "controller number for the CC row (CC 74 opens the preview filter)" },
                     { "channel", "Channel", "with Output channels on Per layer (Outputs page)" },
                     { "voice", "Preview voice", "the built-in sound of this layer" } })
            {
                auto* b = make<ParamBox> (p, pre + id, lab, tip);
                b->tint = col::layer (l);
                L.boxes.push_back (b);
            }
        }
        select (0);
        startTimerHz (30);
    }
    void refresh() override { sample (true); repaint(); }
    void select (int l)
    {
        sel = juce::jlimit (0, kLayers - 1, l);
        for (int i = 0; i < kLayers; ++i) for (auto* b : lay[(size_t) i].boxes) b->setVisible (i == sel);
        editor->layer = sel;
        for (int i = 0; i < kLayers; ++i) lay[(size_t) i].strip->selected = i == sel;
        sample (true);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        panel (g, top, "Layers");
        for (int l = 0; l < kLayers; ++l)
        {
            const auto& L = lay[(size_t) l];
            if (l == sel)
            {
                g.setColour (col::raised); g.fillRoundedRectangle (L.row.toFloat().expanded (6.0f, 3.0f), 8.0f);
                g.setColour (col::layer (l).withAlpha (0.6f)); g.drawRoundedRectangle (L.row.toFloat().expanded (6.0f, 3.0f), 8.0f, 1.0f);
            }
            g.setColour (col::layer (l)); g.fillEllipse ((float) L.label.getX(), (float) L.label.getCentreY() - 5.0f, 10.0f, 10.0f);
            g.setColour (l == sel ? col::text : col::dim); g.setFont (font (13.5f, true));
            g.drawText ("Layer " + juce::String (l + 1), L.label.withTrimmedLeft (16), juce::Justification::centredLeft);
            const auto& k = L.strip->look;
            g.setColour (col::dim); g.setFont (font (12.0f));
            juce::String info = juce::String (k.len) + " steps   " + figureName (k.rate) + (k.mult > 1 ? juce::String::charToString (0xd7) + juce::String (k.mult) : juce::String()) + "   " + patternName (k.pattern);
            if (k.pulses > 0) info << "   E" << juce::jmin (k.pulses, k.len);
            g.drawFittedText (info, L.info, juce::Justification::centredLeft, 1, 0.85f);
        }
        const auto in = panel (g, detail, {});
        juce::ignoreUnused (in);
        g.setColour (col::layer (sel)); g.fillEllipse ((float) detail.getX() + 14.0f, (float) detail.getY() + 12.0f, 10.0f, 10.0f);
        g.setColour (col::text); g.setFont (font (14.0f, true));
        g.drawText ("Layer " + juce::String (sel + 1), detail.getX() + 30, detail.getY() + 5, 120, 24, juce::Justification::centredLeft);
        g.setColour (col::faint); g.setFont (font (12.0f));
        g.drawText ("Click a layer above to edit it here", detail.getX() + 120, detail.getY() + 5, 400, 24, juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        top = r.removeFromTop (32 + kLayers * 30 + (kLayers - 1) * 8 + 16); r.removeFromTop (12);
        detail = r;
        auto t = top.withTrimmedTop (34).reduced (14, 4);
        for (int l = 0; l < kLayers; ++l)
        {
            auto& L = lay[(size_t) l];
            L.row = t.removeFromTop (30); t.removeFromTop (8);
            auto rr = L.row;
            L.label = rr.removeFromLeft (86);
            L.on->setBounds (rr.removeFromLeft (56).reduced (0, 2)); rr.removeFromLeft (12);
            L.info = rr.removeFromLeft (220); rr.removeFromLeft (8);
            L.strip->setBounds (rr);
        }
        auto d = detail.withTrimmedTop (32).reduced (12, 6);
        for (auto& L : lay) gridLayout (d.withHeight (2 * 44 + 8), { L.boxes.begin(), L.boxes.end() }, 10, 44, 8);
        d.removeFromTop (2 * 44 + 8 + 12);
        editor->setBounds (d.removeFromTop (RowEditor::neededHeight()));
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int l = 0; l < kLayers; ++l) if (lay[(size_t) l].row.contains (e.getPosition())) { select (l); return; }
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        int over = -1;
        for (int l = 0; l < kLayers; ++l) if (lay[(size_t) l].row.contains (e.getPosition())) over = l;
        if (over != hoverLane) { hoverLane = over; showHelp (over >= 0 ? "Layer " + juce::String (over + 1) + ": click to edit it below (all its parameters and step rows)" : juce::String()); }
    }
    void mouseExit (const juce::MouseEvent&) override { hoverLane = -1; showHelp ({}); }
private:
    struct Lay { juce::Rectangle<int> row, label, info; ParamBox* on = nullptr; StepStrip* strip = nullptr; std::vector<juce::Component*> boxes; };
    std::array<Lay, kLayers> lay;
    RowEditor* editor = nullptr;
    juce::Rectangle<int> top, detail;
    int sel = 0, hoverLane = -1;
    // Cursor y firma de cada capa: sólo se repinta lo que cambió.
    void sample (bool force)
    {
        const auto& v = proc.view();
        for (int l = 0; l < kLayers; ++l)
        {
            auto& L = lay[(size_t) l];
            const auto k = LayerLook::read (proc, l);
            const int cur = v.running && k.on ? v.step[(size_t) l] % k.len : -1;
            const bool isSel = l == sel;
            if (force || ! (k == L.strip->look))
            {
                L.strip->look = k; L.strip->cursor = cur; L.strip->invalidate();
                if (isSel) { editor->look = k; editor->cursor = cur; editor->invalidate(); }
                repaint (L.info);
                continue;
            }
            if (cur != L.strip->cursor)
            {
                L.strip->repaintStep (L.strip->cursor); L.strip->cursor = cur; L.strip->repaintStep (cur);
            }
            if (isSel && cur != editor->cursor) { editor->repaintColumn (editor->cursor); editor->cursor = cur; editor->repaintColumn (cur); }
        }
    }
    void timerCallback() override
    {
        if (! onScreen()) return;      // otra página, o el editor escondido: nada que repintar
        sample (false);
    }
};

// Teclado del núcleo (página Harmony): protones (las notas que suenan del acorde de entrada) cálidos, neutrones (las que
// agrega Complete) fríos.
class NucleusKeys : public juce::Component
{
public:
    std::array<uint8_t, 128> kind {};      // 0 nada, 1 protón, 2 neutrón
    static constexpr int lo = 24, hi = 96; // C1..C7
    void paint (juce::Graphics& g) override
    {
        int whites = 0; for (int n = lo; n <= hi; ++n) whites += isBlack (n) ? 0 : 1;
        const float w = (float) getWidth() / (float) whites, h = (float) getHeight();
        auto fillFor = [this] (int n, juce::Colour base) { return kind[(size_t) n] == 1 ? col::sun : kind[(size_t) n] == 2 ? col::cold : base; };
        int x = 0;
        for (int n = lo; n <= hi; ++n)
        {
            if (isBlack (n)) continue;
            const auto r = juce::Rectangle<float> ((float) x * w + 0.5f, 0.0f, w - 1.0f, h);
            g.setColour (fillFor (n, col::text.withAlpha (0.12f))); g.fillRoundedRectangle (r, 2.0f);
            if (n % 12 == 0) { g.setColour (col::faint); g.setFont (font (10.0f)); g.drawText ("C" + juce::String (n / 12 - 1), r.withTrimmedTop (h - 16).toNearestInt(), juce::Justification::centred); }
            ++x;
        }
        x = 0;
        for (int n = lo; n <= hi; ++n)
        {
            if (! isBlack (n)) { ++x; continue; }
            const auto r = juce::Rectangle<float> ((float) x * w - w * 0.32f, 0.0f, w * 0.64f, h * 0.6f);
            g.setColour (fillFor (n, col::bg)); g.fillRoundedRectangle (r, 2.0f);
            g.setColour (col::border); g.drawRoundedRectangle (r, 2.0f, 1.0f);
        }
    }
    void mouseEnter (const juce::MouseEvent&) override { showHelp ("The nucleus: warm keys are protons (the notes you hold, or Auto harmony's chord), cold keys are neutrons (the notes Complete adds)"); }
    void mouseExit (const juce::MouseEvent&) override { showHelp ({}); }
private:
    static bool isBlack (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
};

class HarmonyPage : public Page, private juce::Timer
{
public:
    explicit HarmonyPage (ValenceProcessor& p) : Page (p)
    {
        for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                 { "source", "Harmony", "Played: the notes you hold. Auto: a chord walk in the key plays by itself (press Play)" },
                 { "progression", "Progression", "Auto harmony: a walk through the scale's chords, two chords, or a drone on the tonic" },
                 { "chord_bars", "Chord length", "Auto harmony: bars per chord" },
                 { "complete", "Complete", "with one or two notes held, builds the scale chord on the lowest one (triad, seventh, ninth) or adds octaves; the added notes are neutrons" },
                 { "order", "Chord order", "the order the patterns walk through the chord: as played, low to high, high to low, or reproducible random" },
                 { "latch", "Latch", "keeps the chord after you let go; a new hand replaces it, adding notes adds them. The sustain pedal also latches while held" },
                 { "key", "Key", "tonic for Complete, Auto harmony and the fixed notes of the Note row" },
                 { "scale", "Scale", "for Complete, Auto harmony and fixed notes" },
                 { "transpose", "Transpose", "moves everything that plays, in semitones" } })
            harm.push_back (make<ParamBox> (p, id, lab, tip));
        for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                 { "clock", "Clock", "Host follows the DAW's transport and tempo; Free uses its own tempo" },
                 { "tempo", "Tempo", "with the Free clock, or the DAW stopped" },
                 { "phrase", "Phrase", "every N bars all layers go back to step 0 together (Off: free polymeter, they realign only at the fusion)" },
                 { "seed", "Variation", "seeds every random choice (patterns, probability, mutation, humanize): the same number plays the same notes everywhere" },
                 { "accent", "Accent", "what an accented step adds to the velocity" } })
            timeB.push_back (make<ParamBox> (p, id, lab, tip));
        dice = make<juce::TextButton> ("New variation");
        dice->onClick = [this] { proc.newVariation(); };
        dice->setTooltip ("Picks another Variation number: other random choices, the same settings");
        keys = make<NucleusKeys>();
        startTimerHz (8);
    }
    void paint (juce::Graphics& g) override
    {
        panel (g, a, "Input and harmony");
        panel (g, b, "Time and feel");
        panel (g, c, "Nucleus");
        // Los textos sólo si tocan el recorte: maquetar un párrafo cuesta aunque no se vea (el renglón de la fusión se
        // repinta varias veces por segundo).
        if (g.clipRegionIntersects (text))
        {
            const auto& v = proc.view();
            auto nm = [] (const std::array<uint8_t, kMaxPool>& n, int cnt) { juce::StringArray s; for (int i = 0; i < cnt; ++i) s.add (juce::String (noteName (n[(size_t) i] % 12)) + juce::String (n[(size_t) i] / 12 - 1)); return s.joinIntoString (" "); };
            auto r = text;
            auto col1 = r.removeFromLeft (r.getWidth() / 3), col2 = r.removeFromLeft (r.getWidth() / 2), col3 = r;
            auto pair = [&] (juce::Rectangle<int> q, const juce::String& lab, const juce::String& val, juce::Colour cc) {
                g.setColour (col::dim); g.setFont (font (12.5f)); g.drawText (lab, q.removeFromTop (18), juce::Justification::centredLeft);
                g.setColour (cc); g.setFont (font (16.0f, true)); g.drawFittedText (val, q.removeFromTop (24), juce::Justification::centredLeft, 1, 0.8f);
            };
            pair (col1, "Chord", v.chord[0] != 0 ? juce::String (v.chord) : juce::String ("-"), col::text);
            pair (col2, "Protons (held)", v.protons > 0 ? nm (v.protonNotes, v.protons) : juce::String ("-"), col::sun);
            pair (col3, "Neutrons (added)", v.neutrons > 0 ? nm (v.neutronNotes, v.neutrons) : juce::String ("-"), col::cold);
        }
        g.setColour (col::dim); g.setFont (font (13.0f));
        g.drawText (fusionLine, fusionR, juce::Justification::centredLeft);
        g.setColour (col::faint); g.setFont (font (13.0f));
        if (g.clipRegionIntersects (noteA))
            g.drawFittedText ("Hold a chord: its notes are the protons, and every layer walks through them with its own pattern. With one or two "
                              "notes, Complete builds the rest of the chord from the scale (the neutrons). Latch keeps the chord after you let go; "
                              "the sustain pedal latches while it is down.\nWith Harmony on Auto, Valence plays a chord walk in the key by itself "
                              "when you press Play: no input needed.", noteA, juce::Justification::topLeft, 8);
        if (g.clipRegionIntersects (noteB))
            g.drawFittedText ("Each layer has its own length, so lengths like 16, 15 and 13 drift apart and come back together at the fusion "
                              "(below, and the flash in the atom). Phrase brings them back to step 0 every N bars.\nThe same Variation, settings "
                              "and input always play the same notes, in any DAW and at any buffer size.", noteB, juce::Justification::topLeft, 8);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        auto upper = r.removeFromTop (r.getHeight() - 330); r.removeFromTop (12);
        a = upper.removeFromLeft (upper.getWidth() * 3 / 5); upper.removeFromLeft (12); b = upper;
        c = r;
        auto ai = a.withTrimmedTop (32).reduced (12, 6);
        gridLayout (ai.removeFromTop (3 * 44 + 2 * 14), { harm.begin(), harm.end() }, 3, 44, 14);
        ai.removeFromTop (22);
        noteA = ai.reduced (2, 0);
        auto bi = b.withTrimmedTop (32).reduced (12, 6);
        gridLayout (bi.removeFromTop (3 * 44 + 2 * 14), { timeB.begin(), timeB.end() }, 2, 44, 14);
        const int half = (bi.getWidth() - 14) / 2;
        dice->setBounds (bi.getX() + half + 14, bi.getY() - 29, half, 28);      // al lado de Accent, alineado con su caja
        bi.removeFromTop (22);
        noteB = bi.reduced (2, 0);
        auto ci = c.withTrimmedTop (32).reduced (12, 6);
        text = ci.removeFromTop (44); ci.removeFromTop (4);
        fusionR = ci.removeFromTop (22); ci.removeFromTop (8);
        keys->setBounds (ci.withSizeKeepingCentre (ci.getWidth(), std::min (ci.getHeight(), 200)));
    }
private:
    std::vector<ParamBox*> harm, timeB;
    juce::TextButton* dice = nullptr;
    NucleusKeys* keys = nullptr;
    juce::Rectangle<int> a, b, c, text, fusionR, noteA, noteB;
    juce::String fusionLine;
    uint32_t lastSerial = 0xffffffffu; int lastP = -1, lastN = -1;
    void timerCallback() override
    {
        if (! onScreen()) return;
        const auto& v = proc.view();
        if (v.chordSerial != lastSerial || v.protons != lastP || v.neutrons != lastN)
        {
            lastSerial = v.chordSerial; lastP = v.protons; lastN = v.neutrons;
            keys->kind.fill (0);
            for (int i = 0; i < v.neutrons; ++i) keys->kind[(size_t) (v.neutronNotes[(size_t) i] & 127)] = 2;
            for (int i = 0; i < v.protons; ++i) keys->kind[(size_t) (v.protonNotes[(size_t) i] & 127)] = 1;
            keys->repaint(); repaint (text);
        }
        juce::String f = "Fusion (all layers back at step 0 together): -";
        if (v.running && v.fusionBeats >= 0.0) f = "Fusion (all layers back at step 0 together) in " + juce::String (v.fusionBeats / 4.0, 1) + " bars";
        else if (v.running) f = "Fusion: never (the layers restarted out of phase)";
        if (f != fusionLine) { fusionLine = f; repaint (fusionR); }
    }
};

class OutputsPage : public Page, private juce::Timer
{
public:
    explicit OutputsPage (ValenceProcessor& p) : Page (p)
    {
        for (int l = 0; l < kLayers; ++l)
        {
            auto* cb = make<juce::ComboBox>();
            cb->onChange = [this, l, cb] {
                if (cb->getSelectedId() == 1000) return;          // "Not connected": se conserva el puerto elegido
                const int i = cb->getSelectedItemIndex();
                proc.direct.setLayerPort (l, i <= 0 || i - 1 >= devices.size() ? juce::String() : devices[i - 1].identifier);
            };
            ports.push_back (cb);
            auto* ch = make<ParamBox> (p, "l" + juce::String (l + 1) + "_channel", "Channel", "", false);
            ch->tint = col::layer (l);
            chans.push_back (ch);
        }
        rescan = make<juce::TextButton> ("Rescan ports");
        rescan->onClick = [this] { fillPorts(); };
        for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                 { "out_mode", "Output channels", "All on 1 works with any synth; Per layer sends each layer on its own channel" },
                 { "clock_out", "MIDI clock out", "24 pulses per beat plus start and stop, to the direct ports" },
                 { "link", "Link", "Lead shares its chord; Follow instances in other tracks play over it" },
                 { "preview", "Preview sound", "the built-in sound; turn it off when your synths play" }, { "preview_vol", "Preview volume", "" },
                 { "preview_space", "Preview space", "" },
                 { "midi_ports", "Layers as MIDI ports", "one virtual MIDI port per layer (macOS and Linux)" } })
        {
            if (juce::String (id) == "midi_ports" && ! ValenceProcessor::virtualPortsSupported()) continue;
            opts.push_back (make<ParamBox> (p, id, lab, tip));
        }
        fillPorts();
        startTimer (3000);
    }
    void paint (juce::Graphics& g) override
    {
        panel (g, a, "Direct MIDI outputs");
        panel (g, b, "Options");
        for (int l = 0; l < kLayers; ++l)
        {
            const auto r = layLab[(size_t) l];
            g.setColour (col::layer (l)); g.fillEllipse ((float) r.getX(), (float) r.getCentreY() - 5.0f, 10.0f, 10.0f);
            g.setColour (col::text); g.setFont (font (13.5f, true));
            g.drawText ("Layer " + juce::String (l + 1), r.withTrimmedLeft (16), juce::Justification::centredLeft);
        }
        g.setColour (col::dim); g.setFont (font (13.0f));
        g.drawFittedText (juce::String ("Every layer always goes to the DAW. By default all of them on channel 1, so any synth plays them; "
                          "with Output channels on Per layer each one keeps its channel, for a multitimbral synth.\n")
                          + (ValenceProcessor::virtualPortsSupported()
                                ? "- Layers as MIDI ports: one virtual port per layer (Valence Layer 1...) to pick as another track's MIDI input.\n"
                                : "- Windows has no virtual MIDI ports of its own: install loopMIDI, create a port and pick it on the left.\n")
                          + "- Hardware synths: their USB or MIDI interface port, on the left.",
                          note, juce::Justification::topLeft, 10);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        a = r.removeFromLeft (r.getWidth() * 55 / 100); r.removeFromLeft (14); b = r;
        auto in = a.withTrimmedTop (32).reduced (12, 6);
        rescan->setBounds (in.removeFromTop (30).removeFromRight (130));
        in.removeFromTop (10);
        for (int l = 0; l < kLayers; ++l)
        {
            auto row = in.removeFromTop (36); in.removeFromTop (6);
            layLab[(size_t) l] = row.removeFromLeft (120);
            chans[(size_t) l]->setBounds (row.removeFromRight (80).reduced (0, 3)); row.removeFromRight (10);
            ports[(size_t) l]->setBounds (row.reduced (0, 3));
        }
        auto o = b.withTrimmedTop (32).reduced (12, 6);
        gridLayout (o.removeFromTop (4 * 52), { opts.begin(), opts.end() }, 2, 44, 8);
        o.removeFromTop (16);
        note = o;
    }
private:
    std::vector<juce::ComboBox*> ports; std::vector<ParamBox*> chans, opts;
    juce::TextButton* rescan = nullptr;
    juce::Array<juce::MidiDeviceInfo> devices;
    std::array<juce::Rectangle<int>, kLayers> layLab {};
    juce::Rectangle<int> a, b, note;
    void fillPorts()
    {
        devices = juce::MidiOutput::getAvailableDevices();
        proc.direct.reopenMissing();
        for (int l = 0; l < kLayers; ++l)
        {
            auto* cb = ports[(size_t) l];
            cb->clear (juce::dontSendNotification);
            cb->addItem ("DAW only", 1);
            for (int i = 0; i < devices.size(); ++i) cb->addItem (devices[i].name, i + 2);
            int sel = 0;
            for (int i = 0; i < devices.size(); ++i) if (devices[i].identifier == proc.direct.layerPort (l)) sel = i + 1;
            if (! proc.direct.layerConnected (l)) { cb->addItem ("Not connected (" + proc.direct.layerPort (l) + ")", 1000); cb->setSelectedId (1000, juce::dontSendNotification); continue; }
            cb->setSelectedItemIndex (sel, juce::dontSendNotification);
        }
    }
    void timerCallback() override
    {
        if (! dh::reallyShowing (*this)) return;
        const auto now = juce::MidiOutput::getAvailableDevices();
        if (now.size() != devices.size()) fillPorts();
    }
};

} // namespace dh::val::ui
