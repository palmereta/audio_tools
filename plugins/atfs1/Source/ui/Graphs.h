#pragma once
// Gráficos en paneles de LCD: el algoritmo (los 88, con el operador elegido y el nivel en vivo), las
// envolventes editables a mano, el espectro de cada operador y el Formant Sequence.

#include <JuceHeader.h>
#include "Widgets.h"

namespace dh::fs::ui {

// Capa de dibujo sin eventos: la de fondo se guarda como imagen (setBufferedToImage) y sólo se rehace cuando
// cambia lo que muestra; la de adelante dibuja lo que se mueve y se repinta por rectángulos chicos.
struct Layer : public juce::Component
{
    std::function<void (juce::Graphics&)> draw;
    explicit Layer (bool buffered = false) { setInterceptsMouseClicks (false, false); setBufferedToImage (buffered); }
    void paint (juce::Graphics& g) override { if (draw) draw (g); }
};

inline const juce::StringArray& formNames() { static juce::StringArray s { "sine", "all1", "all2", "odd1", "odd2", "res1", "res2", "frmt" }; return s; }

// ───────── geometría de un algoritmo ─────────
struct AlgoLayout
{
    std::array<juce::Point<float>, 8> pos {};   // centro de cada operador, en unidades de grilla
    int levels = 1; float width = 8;
};

inline AlgoLayout layoutAlgorithm (int index)
{
    const auto& A = algorithm (index);
    AlgoLayout L;
    std::array<int, 8> level {};
    // Nivel = el camino más largo hasta una portadora (las portadoras abajo).
    for (int it = 0; it < 8; ++it)
        for (int s = 7; s >= 0; --s)
        {
            int lv = 0;
            for (int d = 0; d < 8; ++d) if (A.modMask[(size_t) d] >> s & 1) lv = std::max (lv, level[(size_t) d] + 1);
            level[(size_t) s] = (A.carriers >> s & 1) && lv == 0 ? 0 : lv;
        }
    int maxL = 0; for (int l : level) maxL = std::max (maxL, l);
    L.levels = maxL + 1;
    // Portadoras repartidas abajo, en orden; los moduladores sobre el promedio de sus destinos.
    std::array<float, 8> x {};
    int nc = 0; for (int k = 0; k < 8; ++k) if (A.carriers >> k & 1) ++nc;
    int ci = 0;
    for (int k = 0; k < 8; ++k) if (A.carriers >> k & 1) x[(size_t) k] = (float) ci++;
    for (int lv = 1; lv <= maxL; ++lv)
    {
        std::vector<std::pair<float, int>> row;
        for (int s = 0; s < 8; ++s)
            if (level[(size_t) s] == lv)
            {
                float sum = 0; int n = 0;
                for (int d = 0; d < 8; ++d) if (A.modMask[(size_t) d] >> s & 1) { sum += x[(size_t) d]; ++n; }
                row.push_back ({ n > 0 ? sum / (float) n : 0.0f, s });
            }
        std::sort (row.begin(), row.end());
        for (size_t i = 1; i < row.size(); ++i) row[i].first = std::max (row[i].first, row[i - 1].first + 1.0f);
        // Centrar el grupo sobre sus destinos si se corrió.
        for (auto& [px, s] : row) x[(size_t) s] = px;
    }
    float mn = 1e9f, mx = -1e9f;
    for (int k = 0; k < 8; ++k) { mn = std::min (mn, x[(size_t) k]); mx = std::max (mx, x[(size_t) k]); }
    for (int k = 0; k < 8; ++k) L.pos[(size_t) k] = { x[(size_t) k] - mn, (float) (maxL - level[(size_t) k]) };
    L.width = mx - mn + 1.0f;
    juce::ignoreUnused (nc);
    return L;
}

// Geometría de un algoritmo dibujado en r: centro de cada operador y tamaño de las cajas.
struct AlgoGeom { std::array<juce::Point<float>, 8> centre {}; float bw = 0, bh = 0, cw = 0, ch = 0, outY = 0; };
inline AlgoGeom algoGeom (juce::Rectangle<float> r, int index, bool small)
{
    const auto L = layoutAlgorithm (index);
    AlgoGeom G;
    G.cw = std::min (r.getWidth() / std::max (4.0f, L.width), small ? 1e9f : 120.0f);
    G.ch = std::min (r.getHeight() / (float) L.levels, small ? 1e9f : 96.0f);
    G.bw = std::min (G.cw * 0.66f, small ? 30.0f : 84.0f); G.bh = std::min (G.ch * 0.56f, small ? 14.0f : 50.0f);
    const float ox = r.getCentreX() - G.cw * L.width / 2, oy = r.getCentreY() - G.ch * (float) L.levels / 2;
    for (int k = 0; k < 8; ++k) G.centre[(size_t) k] = { ox + (L.pos[(size_t) k].x + 0.5f) * G.cw, oy + (L.pos[(size_t) k].y + 0.5f) * G.ch };
    G.outY = oy + G.ch * (float) L.levels - (small ? 2.0f : 8.0f);
    return G;
}

inline void drawAlgorithm (juce::Graphics& g, juce::Rectangle<float> r, int index, int selected, const std::array<float, 8>* meters,
                           const VoiceData* v, bool small)
{
    const auto& A = algorithm (index);
    const auto G = algoGeom (r, index, small);
    const float bw = G.bw, bh = G.bh;
    auto centre = [&] (int k) { return G.centre[(size_t) k]; };
    const float lw = small ? 1.0f : 1.6f;
    g.setColour (col::ink);
    for (int d = 0; d < 8; ++d)
        for (int s = 0; s < 8; ++s)
            if (A.modMask[(size_t) d] >> s & 1)
            {
                const auto a = centre (s).translated (0, bh / 2), b = centre (d).translated (0, -bh / 2);
                juce::Path p; p.startNewSubPath (a); p.lineTo (a.x, (a.y + b.y) / 2); p.lineTo (b.x, (a.y + b.y) / 2); p.lineTo (b);
                g.strokePath (p, juce::PathStrokeType (lw));
            }
    // Salida: barra bajo las portadoras.
    float cx0 = 1e9f, cx1 = -1e9f;
    for (int k = 0; k < 8; ++k) if (A.carriers >> k & 1) { cx0 = std::min (cx0, centre (k).x); cx1 = std::max (cx1, centre (k).x); }
    const float outY = G.outY;
    for (int k = 0; k < 8; ++k) if (A.carriers >> k & 1) g.drawLine (centre (k).x, centre (k).y + bh / 2, centre (k).x, outY, lw);
    g.drawLine (cx0, outY, cx1, outY, lw);
    // Realimentación: lazo del lado derecho de la fuente al destino.
    if (A.fbSrc >= 0)
    {
        const auto s = centre (A.fbSrc), d = centre (A.fbDst);
        const float xr = std::max (s.x, d.x) + bw / 2 + (small ? 3.0f : 9.0f);
        juce::Path p;
        p.startNewSubPath (s.x + bw / 2, s.y);
        p.lineTo (xr, s.y);
        p.lineTo (xr, d.y - bh / 2 - (small ? 3.0f : 8.0f));
        p.lineTo (d.x, d.y - bh / 2 - (small ? 3.0f : 8.0f));
        p.lineTo (d.x, d.y - bh / 2);
        g.strokePath (p, juce::PathStrokeType (lw));
    }
    for (int k = 0; k < 8; ++k)
    {
        const auto c = centre (k);
        const auto box = juce::Rectangle<float> (bw, bh).withCentre (c);
        const bool sel = k == selected;
        const bool used = v == nullptr || v->op[(size_t) k].level > 0 || v->op[(size_t) k].fseq;
        g.setColour (sel ? col::ink : col::lcdHi);
        g.fillRect (box);
        g.setColour (col::ink.withAlpha (used ? 1.0f : 0.35f));
        g.drawRect (box, lw);
        if (meters != nullptr)
        {
            const float m = juce::jlimit (0.0f, 1.0f, (*meters)[(size_t) k]);
            if (m > 0.002f)
            {
                const float h = (box.getHeight() - 4) * juce::jlimit (0.0f, 1.0f, 1.0f + 20.0f * std::log10 (m) / 60.0f);
                g.setColour ((sel ? col::lcdHi : col::ink).withAlpha (0.35f));
                g.fillRect (box.getX() + 2, box.getBottom() - 2 - h, 4.0f, h);
            }
        }
        g.setColour (sel ? col::lcdHi : col::ink.withAlpha (used ? 1.0f : 0.45f));
        if (small) { g.setFont (font (9.0f, true)); g.drawText (juce::String (k + 1), box, juce::Justification::centred); }
        else
        {
            g.setFont (font (15.0f, true));
            g.drawText (juce::String (k + 1), box.withTrimmedBottom (box.getHeight() * 0.4f), juce::Justification::centred);
            if (v != nullptr)
            {
                g.setFont (font (11.0f));
                g.drawText (formNames()[v->op[(size_t) k].form & 7], box.withTrimmedTop (box.getHeight() * 0.52f), juce::Justification::centred);
            }
        }
    }
}

class AlgoView : public juce::Component, private juce::Timer
{
public:
    std::function<void (int)> onSelect;
    AlgoView (ATFS1Processor& p, bool liveMeters = true) : proc (p), live (liveMeters)
    {
        back.draw = [this] (juce::Graphics& g) { paintStatic (g); };
        front.draw = [this] (juce::Graphics& g) { paintMeters (g); };
        addAndMakeVisible (back); addAndMakeVisible (front);
        if (live) startTimerHz (15);
    }
    int selected = 0;
    void repaint() { back.repaint(); front.repaint(); }   // cambio de voz o de operador elegido
    void resized() override { back.setBounds (getLocalBounds()); front.setBounds (getLocalBounds()); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto G = algoGeom (area(), proc.voice().algorithm, false);
        for (int k = 0; k < 8; ++k)
            if (G.centre[(size_t) k].getDistanceFrom (e.position) < std::min (G.cw, G.ch) * 0.4f) { selected = k; if (onSelect) onSelect (k); repaint(); return; }
    }
private:
    ATFS1Processor& proc;
    bool live;
    Layer back { true }, front;
    std::array<float, 8> meters {};
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (18, 30).withTrimmedTop (6); }
    void paintStatic (juce::Graphics& g)
    {
        const auto r = getLocalBounds().toFloat();
        lcdPanel (g, r);
        const auto& v = proc.voice();
        drawAlgorithm (g, area(), v.algorithm, selected, nullptr, &v, false);
        dotText (g, "ALG " + juce::String (v.algorithm + 1).paddedLeft ('0', 2), r.getX() + 12, r.getY() + 10, 2.0f, col::ink, false);
        const auto& A = algorithm (v.algorithm);
        const juce::String fb = A.fbSrc < 0 ? "FB --" : (A.fbSrc == A.fbDst ? "FB OP" + juce::String (A.fbSrc + 1) : "FB " + juce::String (A.fbDst + 1) + "-" + juce::String (A.fbSrc + 1))
                                + " " + juce::String (v.feedback);
        dotText (g, fb, r.getRight() - 12 - (float) fb.length() * 12.0f, r.getY() + 10, 2.0f, col::ink, false);
    }
    juce::Rectangle<float> meterRect (const AlgoGeom& G, int k) const
    {
        const auto box = juce::Rectangle<float> (G.bw, G.bh).withCentre (G.centre[(size_t) k]);
        return { box.getX() + 2.0f, box.getY() + 2.0f, 4.0f, box.getHeight() - 4.0f };
    }
    void paintMeters (juce::Graphics& g)
    {
        if (! live) return;
        const auto G = algoGeom (area(), proc.voice().algorithm, false);
        for (int k = 0; k < 8; ++k)
        {
            const float m = juce::jlimit (0.0f, 1.0f, meters[(size_t) k]);
            if (m <= 0.002f) continue;
            const auto mr = meterRect (G, k);
            const float h = mr.getHeight() * juce::jlimit (0.0f, 1.0f, 1.0f + 20.0f * std::log10 (m) / 60.0f);
            g.setColour ((k == selected ? col::lcdHi : col::ink).withAlpha (0.4f));
            g.fillRect (mr.withTop (mr.getBottom() - h));
        }
    }
    void timerCallback() override
    {
        const auto G = algoGeom (area(), proc.voice().algorithm, false);
        for (int k = 0; k < 8; ++k)
        {
            const float m = proc.live.opMeter[(size_t) k].load();
            if (std::abs (m - meters[(size_t) k]) > 0.003f) { meters[(size_t) k] = m; front.repaint (meterRect (G, k).toNearestInt().expanded (1)); }
        }
    }
};

// Grilla de los 88 algoritmos para elegir uno.
class AlgoPicker : public juce::Component
{
public:
    std::function<void (int)> onPick;
    explicit AlgoPicker (int current) : cur (current) { setSize (11 * 86, 8 * 70); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        for (int i = 0; i < 88; ++i)
        {
            const auto r = cell (i).toFloat().reduced (3);
            lcdPanel (g, r, 1.5f);
            if (i == cur || i == hover) { g.setColour (i == cur ? col::ink : col::ink.withAlpha (0.4f)); g.drawRect (r.reduced (2), 2.0f); }
            drawAlgorithm (g, r.reduced (5, 10).withTrimmedTop (4), i, -1, nullptr, nullptr, true);
            g.setColour (col::ink); g.setFont (font (9.5f, true));
            g.drawText (juce::String (i + 1), r.reduced (4, 2).toNearestInt(), juce::Justification::topLeft);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseUp (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h >= 0 && onPick) onPick (h); }
private:
    int cur, hover = -1;
    juce::Rectangle<int> cell (int i) const { return { (i % 11) * 86, (i / 11) * 70, 86, 70 }; }
    int hit (juce::Point<float> p) const { for (int i = 0; i < 88; ++i) if (cell (i).toFloat().contains (p)) return i; return -1; }
};

// ───────── envolventes editables ─────────
// Un modelo genérico: puntos (tiempo 0..99, nivel) con setters; la primera posición es el punto de partida.
struct EgModel
{
    juce::String title;
    int nSeg = 4;                       // segmentos con tiempo
    bool bipolar = false;               // −50..+50 (altura, filtro) o 0..99 (amplitud)
    int sustainAfter = 3;               // índice de segmento tras el que se sostiene (−1: sin sostén)
    std::function<int (const VoiceData&)> start;                 // nivel de partida
    std::function<void (VoiceData&, int)> setStart;              // nullptr: fijo
    std::function<int (const VoiceData&, int)> level, time;      // del segmento i (nivel al final)
    std::function<void (VoiceData&, int, int)> setLevel, setTime;
};

class EgView : public juce::Component
{
public:
    EgView (ATFS1Processor& p, EgModel m) : proc (p), model (std::move (m)) { setRepaintsOnMouseActivity (true); }
    void setModel (EgModel m) { model = std::move (m); repaint(); }
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        lcdPanel (g, r);
        const auto pts = points();
        const auto area = plotArea();
        g.setColour (col::ink.withAlpha (0.18f));
        if (model.bipolar) g.drawHorizontalLine ((int) yOf (0), area.getX(), area.getRight());
        // Zona de sostén.
        if (model.sustainAfter >= 0)
        {
            const float sx = pts[(size_t) model.sustainAfter + 1].x;
            g.setColour (col::ink.withAlpha (0.08f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (sx, area.getY(), sx + sustainW(), area.getBottom()));
        }
        juce::Path p; p.startNewSubPath (pts[0]);
        for (size_t i = 1; i < pts.size(); ++i)
        {
            if ((int) i == model.sustainAfter + 2) p.lineTo (pts[i - 1].x + sustainW(), pts[i - 1].y);
            p.lineTo (pts[i]);
        }
        g.setColour (col::ink); g.strokePath (p, juce::PathStrokeType (2.0f));
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const bool hot = (int) i == drag || (drag < 0 && (int) i == hoverPt);
            const float s = hot ? 9.0f : 6.0f;
            g.setColour (hot ? col::ink : col::lcdHi); g.fillRect (juce::Rectangle<float> (s, s).withCentre (pts[i]));
            g.setColour (col::ink); g.drawRect (juce::Rectangle<float> (s, s).withCentre (pts[i]), 1.5f);
        }
        dotText (g, model.title, r.getX() + 10, r.getY() + 8, 1.6f, col::ink, false);
        if (hoverPt >= 0 || drag >= 0)
        {
            const int i = drag >= 0 ? drag : hoverPt;
            const auto& v = proc.voice();
            juce::String s = i == 0 ? "L" + juce::String (startLabel()) + " " + juce::String (model.start (v))
                                    : "T" + juce::String (i) + " " + juce::String (model.time (v, i - 1)) + "  L" + juce::String (i) + " " + juce::String (model.level (v, i - 1));
            dotText (g, s, r.getRight() - 10 - (float) s.length() * 9.6f, r.getY() + 8, 1.6f, col::ink, false);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hoverPt) { hoverPt = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hoverPt = -1; repaint(); showHelp ({}); }
    void mouseEnter (const juce::MouseEvent&) override { showHelp (model.title + ": drag a point (left/right = time, up/down = level)"); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = hit (e.position);
        if (drag < 0) return;
        const auto& v = proc.voice();
        startT = drag > 0 ? model.time (v, drag - 1) : 0;
        startL = drag > 0 ? model.level (v, drag - 1) : model.start (v);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag < 0) return;
        const auto area = plotArea();
        const float range = model.bipolar ? 100.0f : 99.0f;
        const int dl = (int) std::lround (-(float) e.getDistanceFromDragStartY() / area.getHeight() * range);
        const int dt = (int) std::lround ((float) e.getDistanceFromDragStartX() / (area.getWidth() / (float) model.nSeg) * 99.0f * 0.6f);
        const int lo = model.bipolar ? -50 : 0, hi = model.bipolar ? 50 : 99;
        proc.editVoiceWith ([&] (VoiceData& v) {
            if (drag == 0) { if (model.setStart) model.setStart (v, juce::jlimit (lo, hi, startL + dl)); }
            else
            {
                model.setTime (v, drag - 1, juce::jlimit (0, 99, startT + dt));
                if (model.setLevel) model.setLevel (v, drag - 1, juce::jlimit (lo, hi, startL + dl));
            }
        });
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { drag = -1; repaint(); }

private:
    ATFS1Processor& proc;
    EgModel model;
    int drag = -1, hoverPt = -1, startT = 0, startL = 0;
    juce::String startLabel() const { return model.bipolar && model.nSeg == 4 && model.setStart ? "0" : "4"; }
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (16, 12).withTrimmedTop (18); }
    float sustainW() const { return model.sustainAfter >= 0 ? plotArea().getWidth() * 0.14f : 0.0f; }
    float yOf (int level) const
    {
        const auto a = plotArea();
        const float t = model.bipolar ? ((float) level + 50.0f) / 100.0f : (float) level / 99.0f;
        return a.getBottom() - t * a.getHeight();
    }
    std::vector<juce::Point<float>> points() const
    {
        const auto& v = proc.voice();
        const auto a = plotArea();
        const float segW = (a.getWidth() - sustainW()) / (float) model.nSeg;
        std::vector<juce::Point<float>> pts;
        float x = a.getX();
        pts.push_back ({ x, yOf (model.start (v)) });
        for (int i = 0; i < model.nSeg; ++i)
        {
            x += segW * (0.08f + 0.92f * (float) model.time (v, i) / 99.0f);
            if (i == model.sustainAfter + 1) x += sustainW();
            pts.push_back ({ x, yOf (model.level (v, i)) });
        }
        return pts;
    }
    int hit (juce::Point<float> p) const
    {
        const auto pts = points();
        for (int i = (int) pts.size() - 1; i >= 0; --i) if (pts[(size_t) i].getDistanceFrom (p) < 9.0f) return i;
        return -1;
    }
};

inline EgModel ampEgModel (int k, bool unvoiced)
{
    EgModel m;
    m.title = unvoiced ? "UNVOICED EG" : "VOICED EG";
    auto eg = [k, unvoiced] (VoiceData& v) -> AmpEgData& { return unvoiced ? v.nop[(size_t) k].eg : v.op[(size_t) k].eg; };
    auto egc = [k, unvoiced] (const VoiceData& v) -> const AmpEgData& { return unvoiced ? v.nop[(size_t) k].eg : v.op[(size_t) k].eg; };
    m.start = [egc] (const VoiceData& v) { return egc (v).level[3]; };
    m.setStart = [eg] (VoiceData& v, int x) { eg (v).level[3] = x; };
    m.level = [egc] (const VoiceData& v, int i) { return egc (v).level[(size_t) i]; };
    m.time = [egc] (const VoiceData& v, int i) { return egc (v).time[(size_t) i]; };
    m.setLevel = [eg] (VoiceData& v, int i, int x) { eg (v).level[(size_t) i] = x; };
    m.setTime = [eg] (VoiceData& v, int i, int x) { eg (v).time[(size_t) i] = x; };
    m.sustainAfter = 2;
    return m;
}

inline EgModel pitchEgModel()
{
    EgModel m; m.title = "PITCH EG"; m.bipolar = true; m.sustainAfter = 2;
    m.start = [] (const VoiceData& v) { return v.pegLevel[0]; };
    m.setStart = [] (VoiceData& v, int x) { v.pegLevel[0] = x; };
    m.level = [] (const VoiceData& v, int i) { return v.pegLevel[(size_t) i + 1]; };
    m.time = [] (const VoiceData& v, int i) { return v.pegTime[(size_t) i]; };
    m.setLevel = [] (VoiceData& v, int i, int x) { v.pegLevel[(size_t) i + 1] = x; };
    m.setTime = [] (VoiceData& v, int i, int x) { v.pegTime[(size_t) i] = x; };
    return m;
}

inline EgModel filterEgModel()
{
    EgModel m; m.title = "FILTER EG"; m.bipolar = true; m.sustainAfter = 2;
    m.start = [] (const VoiceData&) { return 0; };
    m.level = [] (const VoiceData& v, int i) { return v.fegLevel[(size_t) i]; };
    m.time = [] (const VoiceData& v, int i) { return v.fegTime[(size_t) i]; };
    m.setLevel = [] (VoiceData& v, int i, int x) { v.fegLevel[(size_t) i] = x; };
    m.setTime = [] (VoiceData& v, int i, int x) { v.fegTime[(size_t) i] = x; };
    return m;
}

inline EgModel freqEgModel (int k, bool unvoiced)
{
    EgModel m; m.title = "FREQ EG"; m.bipolar = true; m.nSeg = 2; m.sustainAfter = -1;
    auto fe = [k, unvoiced] (VoiceData& v) -> FreqEgData& { return unvoiced ? v.nop[(size_t) k].feg : v.op[(size_t) k].feg; };
    auto fec = [k, unvoiced] (const VoiceData& v) -> const FreqEgData& { return unvoiced ? v.nop[(size_t) k].feg : v.op[(size_t) k].feg; };
    m.start = [fec] (const VoiceData& v) { return fec (v).init; };
    m.setStart = [fe] (VoiceData& v, int x) { fe (v).init = x; };
    m.level = [fec] (const VoiceData& v, int i) { return i == 0 ? fec (v).attack : 0; };
    m.time = [fec] (const VoiceData& v, int i) { return i == 0 ? fec (v).attackTime : fec (v).decayTime; };
    m.setLevel = [fe] (VoiceData& v, int i, int x) { if (i == 0) fe (v).attack = x; };
    m.setTime = [fe] (VoiceData& v, int i, int x) { if (i == 0) fe (v).attackTime = x; else fe (v).decayTime = x; };
    return m;
}

// ───────── espectro de un operador (nota A3, 220 Hz) ─────────
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (ATFS1Processor& p) : proc (p) {}
    int op = 0; bool unvoiced = false;
    void refresh() { dirty = true; repaint(); }
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        lcdPanel (g, r);
        if (dirty) compute();
        const auto a = r.reduced (16, 12).withTrimmedTop (18).withTrimmedBottom (14);
        auto xOf = [&] (float hz) { return a.getX() + a.getWidth() * std::log2 (hz / 50.0f) / std::log2 (12000.0f / 50.0f); };
        auto yOf = [&] (float db) { return a.getY() + a.getHeight() * juce::jlimit (0.0f, 1.0f, -db / 72.0f); };
        g.setColour (col::ink.withAlpha (0.15f));
        for (float hz : { 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            g.drawVerticalLine ((int) xOf (hz), a.getY(), a.getBottom());
            g.setFont (font (9.5f)); g.setColour (col::ink.withAlpha (0.55f));
            g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz), juce::Rectangle<float> (xOf (hz) - 14, a.getBottom() + 1, 28, 12), juce::Justification::centred);
            g.setColour (col::ink.withAlpha (0.15f));
        }
        g.setColour (col::ink);
        if (unvoiced)
        {
            juce::Path p; bool first = true;
            for (size_t i = 0; i < mags.size(); ++i)
            {
                const float hz = (float) i * 48000.0f / (float) (2 * mags.size());
                if (hz < 50.0f || hz > 12000.0f) continue;
                const auto pt = juce::Point<float> (xOf (hz), yOf (mags[i]));
                if (first) { p.startNewSubPath (pt); first = false; } else p.lineTo (pt);
            }
            g.strokePath (p, juce::PathStrokeType (1.5f));
        }
        else
            for (int h = 1; h < (int) harm.size(); ++h)
            {
                const float hz = 220.0f * (float) h;
                if (hz > 12000.0f) break;
                const float y = yOf (harm[(size_t) h]);
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xOf (hz) - 1.5f, y, xOf (hz) + 1.5f, a.getBottom()));
            }
        dotText (g, (unvoiced ? "N" : "V") + juce::String (op + 1) + " SPECTRUM AT A3", r.getX() + 10, r.getY() + 8, 1.6f, col::ink, false);
    }
private:
    ATFS1Processor& proc;
    bool dirty = true;
    std::vector<float> harm, mags;
    void compute()
    {
        dirty = false;
        const auto& v = proc.voice();
        constexpr int N = 8192;
        std::vector<float> x ((size_t) N);
        const float fs = 48000.0f;
        if (! unvoiced)
        {
            const auto& o = v.op[(size_t) op];
            FormOperator fo;
            const float note = 220.0f;
            float hz = o.fixed ? fixedHz (o.coarse, o.fine) : note * ratioOf (o.coarse, o.fine);
            const float win = 1.0f / (3.1f * std::exp2 ((float) o.bwRes / 8.0f));
            // Un período exacto de 220 Hz no cabe en 8192 muestras: se mide por DFT en cada armónico.
            for (int i = 0; i < 2 * N; ++i) { const float y = fo.process (o.form, note * std::exp2 ((float) o.transpose / 12.0f), hz, win, Tables::pIndex (o.form == Frmt, o.skirt), o.bwRes, 0.0f, fs); if (i >= N) x[(size_t) (i - N)] = y; }
            harm.assign (60, -120.0f);
            for (int h = 1; h < 60; ++h)
            {
                double re = 0, im = 0, ws = 0;
                const double w0 = 2.0 * juce::MathConstants<double>::pi * 220.0 * h / fs;
                for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (N - 1)); re += w * x[(size_t) i] * std::cos (w0 * i); im += w * x[(size_t) i] * std::sin (w0 * i); ws += w; }
                harm[(size_t) h] = (float) (20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / ws + 1e-9));
            }
        }
        else
        {
            const auto& u = v.nop[(size_t) op];
            NoiseOperator no;
            const float hz = u.mode == 1 ? 220.0f : fixedHz (u.coarse, u.fine);
            juce::dsp::FFT fft (13);
            std::vector<float> acc ((size_t) N / 2, 0.0f), buf ((size_t) N * 2);
            for (int rep = 0; rep < 6; ++rep)
            {
                for (int i = 0; i < N; ++i) buf[(size_t) i] = no.process (hz, u.bandwidth, u.skirt, u.resonance, fs) * (0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) (N - 1)));
                std::fill (buf.begin() + N, buf.end(), 0.0f);
                fft.performFrequencyOnlyForwardTransform (buf.data());
                for (int i = 0; i < N / 2; ++i) acc[(size_t) i] += buf[(size_t) i] * buf[(size_t) i];
            }
            mags.assign ((size_t) N / 2, -120.0f);
            float mx = 1e-12f; for (auto a : acc) mx = std::max (mx, a);
            // Suavizado de 1/12 de octava para que el ruido se lea como banda.
            for (int i = 1; i < N / 2; ++i)
            {
                const int w = std::max (1, i / 24); double s = 0; int n = 0;
                for (int j = std::max (1, i - w); j <= std::min (N / 2 - 1, i + w); ++j) { s += acc[(size_t) j]; ++n; }
                mags[(size_t) i] = (float) (10.0 * std::log10 (s / n / mx + 1e-12));
            }
        }
    }
};

// ───────── espectro de la salida ─────────
class ScopeView : public juce::Component, private juce::Timer
{
public:
    explicit ScopeView (ATFS1Processor& p) : proc (p), fft (12)
    {
        back.draw = [this] (juce::Graphics& g) { paintBack (g); };
        front.draw = [this] (juce::Graphics& g) { paintCurve (g); };
        addAndMakeVisible (back); addAndMakeVisible (front);
        startTimerHz (20);
    }
    void resized() override { back.setBounds (getLocalBounds()); front.setBounds (getLocalBounds()); }
private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (16, 12).withTrimmedTop (18).withTrimmedBottom (14); }
    float xOf (float hz) const { const auto a = plot(); return a.getX() + a.getWidth() * std::log2 (hz / 40.0f) / std::log2 (16000.0f / 40.0f); }
    float yOf (float db) const { const auto a = plot(); return a.getY() + a.getHeight() * juce::jlimit (0.0f, 1.0f, -(db + 6.0f) / 84.0f); }
    void paintBack (juce::Graphics& g)
    {
        const auto r = getLocalBounds().toFloat();
        lcdPanel (g, r);
        const auto a = plot();
        g.setFont (font (9.5f));
        for (float hz : { 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            g.setColour (col::ink.withAlpha (0.13f)); g.drawVerticalLine ((int) xOf (hz), a.getY(), a.getBottom());
            g.setColour (col::ink.withAlpha (0.5f));
            g.drawText (hz >= 1000 ? juce::String ((int) (hz / 1000)) + "k" : juce::String ((int) hz), juce::Rectangle<float> (xOf (hz) - 14, a.getBottom() + 1, 28, 12), juce::Justification::centred);
        }
        dotText (g, "OUTPUT SPECTRUM", r.getX() + 10, r.getY() + 8, 1.6f, col::ink, false);
    }
    void paintCurve (juce::Graphics& g)
    {
        const auto a = plot();
        juce::Path p; bool first = true;
        const float fs = (float) proc.getSampleRate() > 0 ? (float) proc.getSampleRate() : 48000.0f;
        for (int i = 2; i < 2048; ++i)
        {
            const float hz = (float) i * fs / 4096.0f;
            if (hz < 40.0f) continue;
            if (hz > 16000.0f) break;
            const juce::Point<float> pt (xOf (hz), yOf (smooth[(size_t) i]));
            if (first) { p.startNewSubPath (xOf (hz), a.getBottom()); p.lineTo (pt); first = false; } else p.lineTo (pt);
        }
        if (! first)
        {
            auto fill = p; fill.lineTo (p.getCurrentPosition().x, a.getBottom()); fill.closeSubPath();
            g.setColour (col::ink.withAlpha (0.16f)); g.fillPath (fill);
            g.setColour (col::ink); g.strokePath (p, juce::PathStrokeType (1.4f));
        }
    }
    ATFS1Processor& proc;
    juce::dsp::FFT fft;
    std::array<float, 8192> buf {};
    std::array<float, 2048> smooth {};
    void timerCallback() override
    {
        const int pos = proc.live.scopePos.load (std::memory_order_acquire);
        for (int i = 0; i < 4096; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / 4095.0f);
            buf[(size_t) i] = proc.live.scope[(size_t) ((pos + i) & 4095)] * w;
        }
        std::fill (buf.begin() + 4096, buf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        bool any = false;
        for (int i = 0; i < 2048; ++i)
        {
            const float db = 20.0f * std::log10 (buf[(size_t) i] / 1024.0f + 1e-7f);
            auto& s = smooth[(size_t) i];
            s = db > s ? s + (db - s) * 0.6f : s + (db - s) * 0.18f;
            if (s < -90.0f) s = -90.0f;
            any |= s > -89.0f;
        }
        if (any || ! wasSilent) front.repaint (plot().toNearestInt().expanded (2));
        wasSilent = ! any;
    }
    bool wasSilent = false;
    Layer back { true }, front;
};

// ───────── Formant Sequence ─────────
class FseqView : public juce::Component, private juce::Timer
{
public:
    explicit FseqView (ATFS1Processor& p) : proc (p)
    {
        back.draw = [this] (juce::Graphics& g) { paintTracks (g); };
        front.draw = [this] (juce::Graphics& g) { paintLive (g); };
        addAndMakeVisible (back); addAndMakeVisible (front);
        startTimerHz (30);
    }
    void repaint() { back.repaint(); front.repaint(); }    // Fseq nuevo
    void resized() override { back.setBounds (getLocalBounds()); front.setBounds (getLocalBounds()); }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto& f = proc.fseq();
        if (f.empty()) return;
        const int fr = frameAt (e.position.x);
        if (fr != hoverFrame) { hoverFrame = fr; front.repaint (infoRect()); }
    }
    void mouseExit (const juce::MouseEvent&) override { hoverFrame = -1; front.repaint (infoRect()); showHelp ({}); }
    void mouseEnter (const juce::MouseEvent&) override { showHelp ("Formant Sequence: 8 voiced formants (lines), 8 unvoiced (dots), pitch below. Drag near a loop edge to move it; drag elsewhere to set a new loop."); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto& f = proc.fseq();
        if (f.empty()) return;
        const int fr = frameAt (e.position.x);
        const int ls = (int) proc.value ("fseq_lp_start"), le = (int) proc.value ("fseq_lp_end");
        const float px = area().getWidth() / (float) std::max (1, (int) f.frames.size() - 1);
        if ((int) proc.value ("fseq_loop") == 0) { proc.setValue ("fseq_loop", 2.0f); proc.setValue ("fseq_lp_start", (float) f.loopStart); proc.setValue ("fseq_lp_end", (float) f.loopEnd); }
        dragEdge = std::abs ((float) (fr - ls)) * px < 8.0f ? 0 : (std::abs ((float) (fr - le)) * px < 8.0f ? 1 : 2);
        anchor = fr;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto& f = proc.fseq();
        if (f.empty()) return;
        const int fr = frameAt (e.position.x);
        if (dragEdge == 0) proc.setValue ("fseq_lp_start", (float) std::min (fr, (int) proc.value ("fseq_lp_end") - 1));
        else if (dragEdge == 1) proc.setValue ("fseq_lp_end", (float) std::max (fr, (int) proc.value ("fseq_lp_start") + 1));
        else { proc.setValue ("fseq_lp_start", (float) std::min (anchor, fr)); proc.setValue ("fseq_lp_end", (float) std::max (anchor, fr)); }
    }
private:
    ATFS1Processor& proc;
    float pos = -1.0f;
    int hoverFrame = -1, dragEdge = -1, anchor = 0;
    std::array<int, 3> loopShown { -1, -1, -1 };
    const void* shownData = nullptr; size_t shownSize = 0;
    Layer back { true }, front;
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (14, 10).withTrimmedTop (20).withTrimmedBottom (std::min (70.0f, (float) getHeight() * 0.2f)); }
    juce::Rectangle<float> pitchArea() const { const auto a = area(); return { a.getX(), a.getBottom() + 6, a.getWidth(), std::min (70.0f, (float) getHeight() * 0.2f) - 10 }; }
    juce::Rectangle<int> infoRect() const { return getLocalBounds().withHeight (24).withTrimmedLeft (getWidth() / 3); }
    int frameAt (float x) const { const auto& f = proc.fseq(); return juce::jlimit (0, std::max (0, (int) f.frames.size() - 1), (int) std::lround ((x - area().getX()) / area().getWidth() * (float) (f.frames.size() - 1))); }
    float xOf (float fr) const { const auto a = area(); return a.getX() + a.getWidth() * fr / (float) std::max (1, (int) proc.fseq().frames.size() - 1); }
    std::array<int, 3> loopNow() const
    {
        const auto& f = proc.fseq();
        const int mode = (int) proc.value ("fseq_loop");
        return mode != 0 ? std::array<int, 3> { mode, (int) proc.value ("fseq_lp_start"), (int) proc.value ("fseq_lp_end") } : std::array<int, 3> { 0, f.loopStart, f.loopEnd };
    }
    void paintTracks (juce::Graphics& g)
    {
        const auto r = getLocalBounds().toFloat();
        lcdPanel (g, r);
        const auto& f = proc.fseq();
        if (f.empty()) { dotText (g, "NO FSEQ", r.getCentreX() - 42, r.getCentreY() - 8, 2.0f, col::ink, false); return; }
        const auto a = area(), pa = pitchArea();
        const int n = (int) f.frames.size();
        auto yOf = [&] (float hz) { return a.getBottom() - a.getHeight() * juce::jlimit (0.0f, 1.0f, std::log2 (hz / 80.0f) / std::log2 (12000.0f / 80.0f)); };
        g.setColour (col::ink.withAlpha (0.12f));
        for (float hz : { 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f }) g.drawHorizontalLine ((int) yOf (hz), a.getX(), a.getRight());
        const int stride = std::max (1, n / (int) a.getWidth());
        for (int k = 7; k >= 0; --k)
            for (int i = 0; i < n; i += stride)
            {
                const auto& fr = f.frames[(size_t) i];
                const float ln = fseqLevelGain (fr.nLevel[(size_t) k]);
                if (ln > 0.004f) { g.setColour (col::ink.withAlpha (juce::jlimit (0.1f, 0.6f, ln * 1.4f))); g.fillRect (xOf ((float) i) - 0.8f, yOf (fr.nFreq[(size_t) k]) - 0.8f, 1.6f, 1.6f); }
            }
        for (int k = 7; k >= 0; --k)
            for (int i = 0; i + stride < n; i += stride)
            {
                const auto& a0 = f.frames[(size_t) i]; const auto& a1 = f.frames[(size_t) (i + stride)];
                const float lv = fseqLevelGain (a0.vLevel[(size_t) k]);
                if (lv < 0.003f) continue;
                g.setColour (col::ink.withAlpha (juce::jlimit (0.25f, 1.0f, 0.3f + lv)));
                g.drawLine (xOf ((float) i), yOf (a0.vFreq[(size_t) k]), xOf ((float) (i + stride)), yOf (a1.vFreq[(size_t) k]), 1.0f + 3.0f * std::sqrt (lv));
            }
        g.setColour (col::ink.withAlpha (0.12f)); g.drawRect (pa, 1.0f);
        juce::Path pp; bool first = true;
        for (int i = 0; i < n; i += stride)
        {
            const float hz = f.frames[(size_t) i].pitch;
            const auto pt = juce::Point<float> (xOf ((float) i), pa.getBottom() - pa.getHeight() * juce::jlimit (0.0f, 1.0f, std::log2 (hz / 60.0f) / std::log2 (600.0f / 60.0f)));
            if (first) { pp.startNewSubPath (pt); first = false; } else pp.lineTo (pt);
        }
        g.setColour (col::ink); g.strokePath (pp, juce::PathStrokeType (1.5f));
        dotText (g, juce::String (f.name).toUpperCase() + "  " + juce::String (n) + " FR", r.getX() + 10, r.getY() + 8, 1.6f, col::ink, false);
    }
    void paintLive (juce::Graphics& g)
    {
        const auto& f = proc.fseq();
        if (f.empty()) return;
        const auto a = area(), pa = pitchArea();
        const auto L = loopNow();
        if (L[2] > L[1])
        {
            g.setColour (col::ink.withAlpha (0.1f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xOf ((float) L[1]), a.getY(), xOf ((float) L[2]), pa.getBottom()));
            g.setColour (col::ink.withAlpha (0.6f));
            g.drawVerticalLine ((int) xOf ((float) L[1]), a.getY(), pa.getBottom()); g.drawVerticalLine ((int) xOf ((float) L[2]), a.getY(), pa.getBottom());
        }
        if (pos >= 0.0f) { g.setColour (col::ink); g.fillRect (xOf (pos) - 1.0f, a.getY(), 2.0f, pa.getBottom() - a.getY()); }
        const int n = (int) f.frames.size();
        if (hoverFrame >= 0 && hoverFrame < n)
        {
            const auto& fr = f.frames[(size_t) hoverFrame];
            juce::String s = "#" + juce::String (hoverFrame) + " F0 " + juce::String ((int) fr.pitch) + "  F1 " + juce::String ((int) fr.vFreq[0]) + "  F2 " + juce::String ((int) fr.vFreq[1]);
            dotText (g, s, (float) getWidth() - 10 - (float) s.length() * 9.6f, 8.0f, 1.6f, col::ink, false);
        }
    }
    void timerCallback() override
    {
        const auto& f = proc.fseq();
        if ((const void*) f.frames.data() != shownData || f.frames.size() != shownSize) { shownData = f.frames.data(); shownSize = f.frames.size(); back.repaint(); front.repaint(); }
        const auto L = loopNow();
        if (L != loopShown) { loopShown = L; front.repaint(); }
        const float p = proc.live.fseqPos.load();
        if (std::abs (p - pos) > 0.3f)
        {
            const auto a = area(), pa = pitchArea();
            auto strip = [&] (float fr) { return juce::Rectangle<float> (xOf (fr) - 3.0f, a.getY(), 6.0f, pa.getBottom() - a.getY()).toNearestInt(); };
            if (pos >= 0.0f) front.repaint (strip (pos));
            pos = p;
            if (pos >= 0.0f) front.repaint (strip (pos));
        }
    }
};

} // namespace dh::fs::ui
