#pragma once
// El panel renderizado (tools/blender/pallor.py) y todo lo que se mueve encima. Tres capas:
//   - StaticLayer: el render y la serigrafía fija (texto vectorial). Se cachea como imagen (setBufferedToImage):
//     no se vuelve a pintar salvo al cambiar el tamaño.
//   - Overlay: tapas de perilla con su índice y su arco, el brillo de cada pad (recortes de lit.jpg con la opacidad
//     del golpe), teclas y pads apretados (recortes de down.jpg), las lámparas de las teclas, el display y los rótulos
//     que cambian con la voz. Repinta sólo los rectángulos que cambiaron.
// Las coordenadas del layout vienen en px del render (8 px/mm); la interfaz trabaja a 4 px/mm (la mitad).

#include "Ctx.h"
#include "ThemePl.h"

namespace dh::pal::ui {

struct Layout
{
    struct KnobPos { juce::String id; juce::Point<float> c; float r; bool big; };
    struct Key { juce::String id; juce::Rectangle<float> r; bool led; };
    struct Silk { juce::String t; juce::Point<float> c; float size; juce::String align; bool dim, semi; float track; };
    juce::Rectangle<float> display, screen, trough;
    std::vector<KnobPos> knobs; std::vector<Key> keys; std::vector<juce::Rectangle<float>> pads; std::vector<Silk> silk;
    float knobLabelDy = 20, padLabelDy = 18;
    static const Layout& get()
    {
        static Layout L = [] {
            Layout l;
            auto v = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::layout_json, BinaryData::layout_jsonSize));
            auto rect = [] (const juce::var& a) { return juce::Rectangle<float> ((float) a[0], (float) a[1], (float) a[2], (float) a[3]) * 0.5f; };
            l.display = rect (v["display"]); l.screen = rect (v["screen"]); l.trough = rect (v["trough"]);
            for (auto& k : *v["knobs"].getArray()) l.knobs.push_back ({ k["id"].toString(), juce::Point<float> ((float) k["c"][0], (float) k["c"][1]) * 0.5f, (float) k["r"] * 0.5f, k["size"].toString() == "big" });
            for (auto& k : *v["keys"].getArray()) l.keys.push_back ({ k["id"].toString(), rect (k["rect"]), (bool) k["led"] });
            for (auto& p : *v["pads"].getArray()) l.pads.push_back (rect (p));
            for (auto& s : *v["silk"].getArray())
                l.silk.push_back ({ s["t"].toString(), juce::Point<float> ((float) s["c"][0], (float) s["c"][1]) * 0.5f, (float) s["size"] * 0.5f, s["align"].toString(),
                                    s["ink"].toString() == "dim", (bool) s["semi"], (float) s["track"] });
            l.knobLabelDy = (float) v["knobLabelDy"] * 0.5f; l.padLabelDy = (float) v["padLabelDy"] * 0.5f;
            return l;
        }();
        return L;
    }
};

inline juce::Image loadUi (const char* data, int size) { return juce::ImageCache::getFromMemory (data, size); }

// ── El render y la serigrafía fija ──
class StaticLayer : public juce::Component
{
public:
    StaticLayer()
    {
        bg = loadUi (BinaryData::bg_jpg, BinaryData::bg_jpgSize);
        setOpaque (true); setBufferedToImage (true); setInterceptsMouseClicks (false, false);
    }
    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (bg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        const auto& L = Layout::get();
        for (const auto& s : L.silk)
        {
            const bool onKey = std::any_of (L.keys.begin(), L.keys.end(), [&] (const Layout::Key& k) { return k.r.contains (s.c); });
            g.setColour (onKey ? col::silk.withAlpha (0.82f) : (s.dim ? col::silkDim : col::silk));
            g.setFont (tracked (s.size, s.track, s.semi));
            const auto j = s.align == "center" ? juce::Justification::centred : (s.align == "right" ? juce::Justification::centredRight : juce::Justification::centredLeft);
            const float w = 400.0f;
            auto r = s.align == "center" ? juce::Rectangle<float> (s.c.x - w / 2, s.c.y - s.size, w, s.size * 2)
                                         : juce::Rectangle<float> (s.c.x, s.c.y - s.size, w, s.size * 2);
            g.drawText (s.t, r, j, false);
        }
        // Números de los pads (tinta oscura sobre la cerámica).
        g.setColour (col::padInk.withAlpha (0.55f)); g.setFont (tracked (9.5f, 0.1f));
        for (size_t i = 0; i < L.pads.size(); ++i)
            g.drawText (juce::String ((int) i + 1).paddedLeft ('0', 2), L.pads[i].withY (L.pads[i].getY() - 11.0f).withHeight (10.0f).translated (2.0f, 0), juce::Justification::centredLeft, false);
        // Prev / next: el triángulo está en el render; la serigrafía de las perillas grandes va en el Overlay (cambia).
    }
private:
    juce::Image bg;
};

// ── Lo que se mueve ──
class Overlay : public juce::Component, public juce::DragAndDropTarget, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
{
public:
    explicit Overlay (Ctx& c) : ctx (c), L (Layout::get())
    {
        auto lit = juce::ImageFileFormat::loadFrom (BinaryData::lit_jpg, (size_t) BinaryData::lit_jpgSize);
        auto dn = juce::ImageFileFormat::loadFrom (BinaryData::down_jpg, (size_t) BinaryData::down_jpgSize);
        // Sólo las franjas que se usan (los pads y las teclas): menos memoria por instancia.
        auto strip = [] (const juce::Image& im, juce::Rectangle<float> r1) { auto r = (r1 * 2.0f).getSmallestIntegerContainer().getIntersection (im.getBounds()); return std::make_pair (im.getClippedImage (r).createCopy(), r); };
        juce::Rectangle<float> keysTop, keysBot;
        for (auto& k : L.keys) { auto& u = k.r.getY() < L.screen.getY() ? keysTop : keysBot; u = u.isEmpty() ? k.r : u.getUnion (k.r); }
        litPads = strip (lit, L.trough.expanded (6)); downPads = strip (dn, L.trough.expanded (2));
        litKeys = strip (lit, keysBot.expanded (2)); downKeysBot = strip (dn, keysBot.expanded (2)); downKeysTop = strip (dn, keysTop.expanded (2));
        capBig = loadUi (BinaryData::cap_big_png, BinaryData::cap_big_pngSize);
        capSmall = loadUi (BinaryData::cap_small_png, BinaryData::cap_small_pngSize);
        for (auto& k : L.knobs) knobs.push_back ({ k, nullptr, {} });
        bindKnobs();
        ctx.onVoice.push_back ([this] { bindKnobs(); repaint(); });
    }

    // Las ocho perillas grandes siguen a la voz elegida; las tres del encabezado son fijas.
    void bindKnobs()
    {
        for (auto& k : knobs)
        {
            k.att.reset();
            k.param = k.pos.big ? ctx.vparam (k.pos.id.toRawUTF8()) : ctx.param (k.pos.id);
            if (k.param != nullptr)
            {
                auto r = knobRect (k.pos);
                k.att = std::make_unique<juce::ParameterAttachment> (*k.param, [this, r] (float) { repaint (r.expanded (0, 30).getSmallestIntegerContainer()); });
            }
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const auto clip = g.getClipBounds().toFloat();
        // Pads: brillo, apretado, elegido, silenciado.
        for (int i = 0; i < (int) L.pads.size(); ++i)
        {
            auto pr = L.pads[(size_t) i];
            if (! clip.intersects (pr.expanded (14))) continue;
            if (pressedPad == i) blit (g, downPads, pr.expanded (2), 1.0f);
            const float lv = juce::jlimit (0.0f, 1.0f, shownFlash[(size_t) i]);
            if (lv > 0.01f)
            {
                // La luz que sale por los costados del pad sobre la cerámica, y el pad encendido (recortado a su forma).
                const float a = std::pow (lv, 0.8f);
                g.setOpacity (a); g.drawImage (glowImage(), pr.expanded (12.0f)); g.setOpacity (1.0f);
                juce::Graphics::ScopedSaveState ss (g);
                juce::Path clipP; clipP.addRoundedRectangle (pr.expanded (0.5f), 10.0f);
                g.reduceClipRegion (clipP);
                blit (g, litPads, pr.expanded (1), a);
            }
            const bool muted = ctx.vvalue ("mute", i) > 0.5f, solo = ctx.vvalue ("solo", i) > 0.5f, sel = i == ctx.voice;
            bool anySolo = false; for (int s = 0; s < kNumSlots; ++s) anySolo |= ctx.vvalue ("solo", s) > 0.5f;
            if (muted || (anySolo && ! solo)) { g.setColour (juce::Colours::black.withAlpha (0.38f)); g.fillRoundedRectangle (pr.reduced (1.0f), 5.0f); }
            // Rótulo debajo del pad (tinta oscura), con la marca de la voz elegida.
            auto lr = juce::Rectangle<float> (pr.getX() - 6, pr.getBottom() + L.padLabelDy - 8.0f, pr.getWidth() + 12, 14.0f);
            g.setColour (col::padInk.withAlpha (sel ? 0.95f : 0.62f));
            g.setFont (tracked (11.5f, 0.1f, sel));
            g.drawText (juce::String (params::voiceName (i)).toUpperCase(), lr, juce::Justification::centred, false);
            if (sel) { g.setColour (col::rose); g.fillRoundedRectangle (pr.getX() + 10, lr.getBottom() + 1.5f, pr.getWidth() - 20, 3.0f, 1.5f); }
            if (muted || solo)
            {
                auto b = juce::Rectangle<float> (pr.getRight() - 17, pr.getY() + 4, 13, 13);
                g.setColour (solo ? col::amber : col::padInk.withAlpha (0.7f)); g.fillRoundedRectangle (b, 2.5f);
                g.setColour (solo ? col::ink : col::bone); g.setFont (font (11.0f, true)); g.drawText (solo ? "S" : "M", b, juce::Justification::centred, false);
            }
        }
        // Teclas.
        for (auto& k : L.keys)
        {
            if (! clip.intersects (k.r.expanded (6))) continue;
            if (pressedKey == k.id) blit (g, k.r.getY() < L.screen.getY() ? downKeysTop : downKeysBot, k.r.expanded (2), 1.0f);
            if (k.led && keyLit (k.id)) blit (g, litKeys, juce::Rectangle<float> (k.r.getCentreX() - 14, k.r.getY() + 2, 28, 12), keyLitLevel (k.id));
        }
        // Perillas.
        for (auto& k : knobs) if (clip.intersects (knobRect (k.pos))) paintKnob (g, k);
        // Rótulos de las perillas grandes (cambian con el modelo y la voz).
        const auto ch = ctx.charLabels();
        for (auto& k : knobs)
        {
            if (! k.pos.big) continue;
            auto lr = juce::Rectangle<float> (k.pos.c.x - 60, k.pos.c.y + k.pos.r + L.knobLabelDy - 8.0f, 120, 16);
            if (! clip.intersects (lr)) continue;
            juce::String t = k.pos.id == "char1" ? ch.first : (k.pos.id == "char2" ? ch.second : (k.param ? k.param->getName (40).fromLastOccurrenceOf (" ", false, false) : juce::String()));
            if (k.pos.id == "level") t = "Level";
            g.setColour (dragging == &k ? col::rose : col::silk); g.setFont (tracked (12.5f, 0.12f, true));
            g.drawText (dragging == &k && k.param ? k.param->getCurrentValueAsText() : t.toUpperCase(), lr, juce::Justification::centred, false);
        }
        // Rótulo de la fila: la voz que se edita.
        for (const auto& s : L.silk)
            if (s.t == "VOICE")
            {
                auto f = tracked (s.size, s.track, s.semi);
                const float w = juce::GlyphArrangement::getStringWidth (f, s.t) * (1.0f + s.track) + 14.0f;
                auto vr = juce::Rectangle<float> (s.c.x + w, s.c.y - s.size, 300, s.size * 2);
                if (! clip.intersects (vr)) continue;
                g.setColour (col::rose); g.setFont (tracked (s.size, s.track, true));
                g.drawText (juce::String (params::voiceName (ctx.voice)).toUpperCase(), vr, juce::Justification::centredLeft, false);
            }
        if (clip.intersects (L.display)) paintDisplay (g);
    }

    // Una vez por cuadro: lo que cambió se repinta.
    void tick()
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            const float v = std::round (ctx.flash[(size_t) i] * 12.0f) / 12.0f;     // 12 escalones de brillo
            if (std::abs (v - shownFlash[(size_t) i]) > 1.0e-4f) { shownFlash[(size_t) i] = v; repaint (L.pads[(size_t) i].expanded (14).getSmallestIntegerContainer()); }
        }
        const uint32_t ks = keyState();
        if (ks != shownKeys) { shownKeys = ks; for (auto& k : L.keys) if (k.led) repaint (k.r.expanded (4).getSmallestIntegerContainer()); }
        const auto dt = displayText();
        if (dt != shownDisplay) { shownDisplay = dt; repaint (L.display.expanded (2).getSmallestIntegerContainer()); }
        uint32_t ms = 0; for (int s = 0; s < kNumSlots; ++s) { if (ctx.vvalue ("mute", s) > 0.5f) ms |= 1u << s; if (ctx.vvalue ("solo", s) > 0.5f) ms |= 1u << (s + 16); }
        if (ms != shownMS || ctx.voice != shownVoice || modelSig() != shownModel)
        {
            shownMS = ms; shownVoice = ctx.voice; shownModel = modelSig();
            repaint (L.trough.expanded (2).getSmallestIntegerContainer());
            juce::Rectangle<float> row; for (auto& k : knobs) if (k.pos.big) row = row.isEmpty() ? knobRect (k.pos) : row.getUnion (knobRect (k.pos));
            repaint (row.withTop (row.getY() - 40).withBottom (row.getBottom() + 26).getSmallestIntegerContainer());
        }
    }

    bool hitTest (int x, int y) override { return ! L.screen.contains ((float) x, (float) y); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        for (int i = 0; i < (int) L.pads.size(); ++i)
            if (L.pads[(size_t) i].contains (p))
            {
                if (e.mods.isPopupMenu()) { padMenu (i); return; }
                ctx.selectVoice (i);
                if (! e.mods.isAltDown())
                {
                    // Más fuerte arriba del pad: 40 abajo, 127 arriba.
                    const float y = 1.0f - (p.y - L.pads[(size_t) i].getY()) / L.pads[(size_t) i].getHeight();
                    ctx.proc.audition (i, juce::jlimit (0.3f, 1.0f, 0.3f + 0.75f * y));
                }
                pressedPad = i; repaint (L.pads[(size_t) i].expanded (6).getSmallestIntegerContainer());
                return;
            }
        for (auto& k : L.keys)
            if (k.r.contains (p)) { pressedKey = k.id; key (k.id, true); repaint (k.r.expanded (4).getSmallestIntegerContainer()); return; }
        for (auto& k : knobs)
            if (knobRect (k.pos).reduced (8).contains (p) && k.param != nullptr)
            {
                dragging = &k; dragStart = k.param->getValue(); k.att->beginGesture();
                repaint (knobRect (k.pos).expanded (0, 30).getSmallestIntegerContainer());
                return;
            }
        if (L.display.contains (p))
        {
            if (ctx.showPage) ctx.showPage ("presets");
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging == nullptr || dragging->param == nullptr) return;
        const float px = e.mods.isShiftDown() ? 900.0f : 240.0f;
        const float v = juce::jlimit (0.0f, 1.0f, dragStart + ((float) e.getDistanceFromDragStartX() - (float) e.getDistanceFromDragStartY()) / px);
        dragging->att->setValueAsPartOfGesture (dragging->param->convertFrom0to1 (v));
        ctx.say (juce::String (params::voiceName (ctx.voice)) + "  " + dragging->param->getName (40).fromFirstOccurrenceOf (" ", false, false) + "  " + dragging->param->getCurrentValueAsText());
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging != nullptr) { dragging->att->endGesture(); repaint (knobRect (dragging->pos).expanded (0, 30).getSmallestIntegerContainer()); dragging = nullptr; }
        if (pressedPad >= 0) { repaint (L.pads[(size_t) pressedPad].expanded (6).getSmallestIntegerContainer()); pressedPad = -1; }
        if (pressedKey.isNotEmpty())
        {
            for (auto& k : L.keys) if (k.id == pressedKey) { key (k.id, false); repaint (k.r.expanded (4).getSmallestIntegerContainer()); }
            pressedKey.clear();
        }
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        for (auto& k : knobs)
            if (knobRect (k.pos).reduced (8).contains (e.position) && k.param != nullptr) k.att->setValueAsCompleteGesture (k.param->convertFrom0to1 (k.param->getDefaultValue()));
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        for (auto& k : knobs)
            if (knobRect (k.pos).contains (e.position) && k.param != nullptr) k.att->setValueAsCompleteGesture (k.param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, k.param->getValue() + w.deltaY * 0.05f)));
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        juce::String tip;
        for (int i = 0; i < (int) L.pads.size(); ++i) if (L.pads[(size_t) i].contains (e.position)) tip = "Click: play and select " + juce::String (params::voiceName (i)) + " (harder near the top). Alt-click: select only. Right-click: sample, mute, solo. Drop an audio file here to load it.";
        for (auto& k : L.keys) if (k.r.contains (e.position)) tip = keyTip (k.id);
        if (L.display.contains (e.position)) tip = "Click to browse presets by genre.";
        setTooltip (tip);
    }

    // Arrastrar una muestra (del navegador de SAMPLES) o un archivo del sistema sobre un pad.
    int padAt (juce::Point<int> p) const { for (int i = 0; i < (int) L.pads.size(); ++i) if (L.pads[(size_t) i].expanded (4).contains (p.toFloat())) return i; return -1; }
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("file:"); }
    void itemDropped (const SourceDetails& d) override
    {
        const int i = padAt (d.localPosition);
        if (i >= 0 && ctx.loadSample) ctx.loadSample (i, juce::File (d.description.toString().substring (5)));
    }
    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() == 1; }
    void filesDropped (const juce::StringArray& f, int x, int y) override
    {
        const int i = padAt ({ x, y });
        if (ctx.loadSample) ctx.loadSample (i >= 0 ? i : ctx.voice, juce::File (f[0]));
    }

private:
    struct KnobState { Layout::KnobPos pos; juce::RangedAudioParameter* param; std::unique_ptr<juce::ParameterAttachment> att; };
    Ctx& ctx; const Layout& L;
    std::pair<juce::Image, juce::Rectangle<int>> litPads, downPads, litKeys, downKeysBot, downKeysTop;
    juce::Image capBig, capSmall;
    std::vector<KnobState> knobs;
    KnobState* dragging = nullptr; float dragStart = 0;
    int pressedPad = -1; juce::String pressedKey;
    std::array<float, kNumSlots> shownFlash {};
    uint32_t shownKeys = 0, shownMS = 0; int shownVoice = -1; juce::String shownDisplay; int shownModel = -1;

    // La luz que sale por los costados del pad sobre la cerámica: una imagen hecha una vez (un degradé radial por
    // cuadro le costaba a CoreAnimation más que todo el resto del panel).
    static const juce::Image& glowImage()
    {
        static juce::Image im = [] {
            juce::Image i (juce::Image::ARGB, 96, 96, true);
            juce::Graphics g (i);
            juce::ColourGradient gr (col::rose.withAlpha (0.34f), 48, 48, col::rose.withAlpha (0.0f), 48, 94, true);
            g.setGradientFill (gr); g.fillRoundedRectangle (0, 0, 96, 96, 20.0f);
            return i;
        }();
        return im;
    }
    int modelSig() const { return (int) ctx.vvalue ("model") * 4 + (int) ctx.vvalue ("engine") + ctx.voice * 64; }

    // Un recorte de una franja (lit/down) dibujado en su lugar con opacidad.
    void blit (juce::Graphics& g, const std::pair<juce::Image, juce::Rectangle<int>>& s, juce::Rectangle<float> dst, float alpha) const
    {
        auto src = (dst * 2.0f).getSmallestIntegerContainer().getIntersection (s.second);
        if (src.isEmpty()) return;
        const auto d = src.toFloat() * 0.5f;
        g.setOpacity (juce::jlimit (0.0f, 1.0f, alpha));
        g.drawImage (s.first, juce::roundToInt (d.getX()), juce::roundToInt (d.getY()), juce::roundToInt (d.getWidth()), juce::roundToInt (d.getHeight()), src.getX() - s.second.getX(), src.getY() - s.second.getY(), src.getWidth(), src.getHeight());
        g.setOpacity (1.0f);
    }

    juce::Rectangle<float> knobRect (const Layout::KnobPos& k) const { const float sz = k.r * 2.0f + 16.0f + 14.0f; return juce::Rectangle<float> (sz, sz).withCentre (k.c); }

    void paintKnob (juce::Graphics& g, const KnobState& k)
    {
        const float v = k.param != nullptr ? k.param->getValue() : 0.5f;
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, ang = a0 + v * (a1 - a0);
        const auto c = k.pos.c; const float r = k.pos.r;
        // Arco del valor: una línea de luz rosa sobre la escala grabada.
        const float ra = r + (k.pos.big ? 14.4f : 10.0f);
        const bool bip = k.pos.id == "tune";
        juce::Path tr; tr.addCentredArc (c.x, c.y, ra, ra, 0, bip ? a0 + 0.5f * (a1 - a0) : a0, ang, true);
        g.setColour (col::rose.withAlpha (0.22f)); g.strokePath (tr, juce::PathStrokeType (k.pos.big ? 6.0f : 4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (dragging == &k ? col::roseHi : col::rose); g.strokePath (tr, juce::PathStrokeType (k.pos.big ? 2.2f : 1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // Sombra fija (la luz del render viene de arriba a la izquierda) y la tapa.
        const float sh = r * 1.12f;
        juce::ColourGradient sg (juce::Colours::black.withAlpha (0.6f), c.x + r * 0.22f, c.y + r * 0.3f, juce::Colours::transparentBlack, c.x + r * 0.22f + sh * 1.3f, c.y + r * 0.3f, true);
        g.setGradientFill (sg); g.fillEllipse (c.x - sh + r * 0.22f, c.y - sh + r * 0.3f, sh * 2, sh * 2);
        const auto& img = k.pos.big ? capBig : capSmall;
        const float isz = (r * 2.0f + (k.pos.big ? 32.0f : 32.0f)) ;           // la tapa trae 4 mm de aire por lado (16 px)
        g.drawImage (img, juce::Rectangle<float> (isz, isz).withCentre (c));
        // Índice: una raya de tinta en la cerámica.
        const float ri = r * 0.72f;
        juce::Path ind; ind.addRoundedRectangle (-r * 0.055f, -ri, r * 0.11f, ri * 0.55f, r * 0.04f);
        g.setColour (col::padInk.withAlpha (0.9f)); g.fillPath (ind, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    bool keyLit (const juce::String& id) const
    {
        auto& p = ctx.proc;
        if (id == "run") return p.view.running.load();
        if (id == "fill") return p.value ("fill") > 0.5f || p.view.fill.load();
        if (id == "mute") return ctx.vvalue ("mute") > 0.5f;
        if (id == "solo") return ctx.vvalue ("solo") > 0.5f;
        return false;
    }
    float keyLitLevel (const juce::String& id) const
    {
        if (id == "run")
        {
            // Late con el pulso: fuerte en cada negra.
            const double ppq = ctx.proc.view.ppq.load();
            const double f = ppq - std::floor (ppq);
            return 0.55f + 0.45f * (float) std::exp (-f * 6.0);
        }
        if (id == "fill" && ctx.proc.value ("fill") < 0.5f) return 0.6f;
        return 1.0f;
    }
    uint32_t keyState() const
    {
        uint32_t s = 0; int i = 0;
        for (auto& k : L.keys) { if (k.led && keyLit (k.id)) s |= 1u << i; ++i; }
        if (ctx.proc.view.running.load()) s |= (uint32_t) (std::exp (-(ctx.proc.view.ppq.load() - std::floor (ctx.proc.view.ppq.load())) * 6.0) * 3.0) << 20;
        return s;
    }
    juce::String keyTip (const juce::String& id) const
    {
        if (id == "run") return "Run: start or stop the internal clock (when Follow host is off, or the host is stopped).";
        if (id == "fill") return "Fill: the next pass plays the pattern's Fill. Click to latch, click again to release.";
        if (id == "mute") return "Mute the selected voice.";
        if (id == "solo") return "Solo the selected voice (several voices can be soloed).";
        if (id == "manual") return "The manual.";
        if (id == "prev") return "Previous preset.";
        if (id == "next") return "Next preset.";
        return {};
    }
    void key (const juce::String& id, bool down)
    {
        auto& p = ctx.proc;
        if (! down) return;
        auto flip = [&] (const juce::String& pid) { ctx.setParam (pid, p.value (pid) > 0.5f ? 0.0f : 1.0f); };
        if (id == "run") flip ("run");
        else if (id == "fill") flip ("fill");
        else if (id == "mute") flip (params::vid (ctx.voice, "mute"));
        else if (id == "solo") flip (params::vid (ctx.voice, "solo"));
        else if (id == "manual") { if (ctx.toggleManual) ctx.toggleManual(); }
        else if (id == "prev" || id == "next")
        {
            const int n = (int) p.getPresets().size();
            if (n > 0) p.loadPreset ((p.getCurrentProgram() + (id == "next" ? 1 : n - 1)) % n);
        }
    }

    void padMenu (int i)
    {
        juce::PopupMenu m;
        m.addSectionHeader (juce::String (params::voiceName (i)));
        m.addItem (1, "Load sample...");
        m.addItem (2, "Clear sample", ctx.proc.userSampleName (i).isNotEmpty());
        m.addSeparator();
        m.addItem (3, "Mute", true, ctx.vvalue ("mute", i) > 0.5f);
        m.addItem (4, "Solo", true, ctx.vvalue ("solo", i) > 0.5f);
        m.addItem (5, "Clear all solos");
        juce::Component::SafePointer<Overlay> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (L.pads[(size_t) i].toNearestInt())), [safe, i] (int r) {
            if (safe == nullptr || r == 0) return;
            auto& c = safe->ctx;
            if (r == 1)
            {
                safe->chooser = std::make_unique<juce::FileChooser> ("Load a sample for " + juce::String (params::voiceName (i)), juce::File(), "*.wav;*.aif;*.aiff;*.flac");
                safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, i] (const juce::FileChooser& fc) {
                    if (safe != nullptr && fc.getResult() != juce::File() && safe->ctx.loadSample) safe->ctx.loadSample (i, fc.getResult()); });
            }
            else if (r == 2) c.proc.clearUserSample (i);
            else if (r == 3) c.setParam (params::vid (i, "mute"), c.vvalue ("mute", i) > 0.5f ? 0.0f : 1.0f);
            else if (r == 4) c.setParam (params::vid (i, "solo"), c.vvalue ("solo", i) > 0.5f ? 0.0f : 1.0f);
            else if (r == 5) for (int s = 0; s < kNumSlots; ++s) if (c.vvalue ("solo", s) > 0.5f) c.setParam (params::vid (s, "solo"), 0.0f);
        });
    }
    std::unique_ptr<juce::FileChooser> chooser;

    // ── El display del encabezado ──
    juce::String displayText() const
    {
        auto& p = ctx.proc;
        juce::String t = p.presetDisplayName() + "|" + (ctx.hasMessage() ? ctx.message : juce::String()) + "|";
        const bool run = p.view.running.load();
        const double bpm = run ? p.view.bpm.load() : (double) p.value ("tempo");
        t << juce::String (bpm, 1) << "|" << (run ? 1 : 0);
        if (run)
        {
            const double ppq = p.view.ppq.load(), s0 = p.view.segStart.load();
            const int pat = p.view.pattern.load();
            const int beat = (int) std::floor (ppq - s0);
            t << "|" << pat << "|" << beat / 4 + 1 << "." << beat % 4 + 1 << "|" << p.view.entry.load();
        }
        return t;
    }
    void paintDisplay (juce::Graphics& g)
    {
        auto& p = ctx.proc;
        auto r = L.display.reduced (14.0f, 5.0f);
        // Izquierda: el preset (con su marca de modificado) y su género, o el mensaje del momento.
        auto left = r.removeFromLeft (r.getWidth() * 0.62f);
        g.setColour (col::bone); text (g, p.presetDisplayName(), left.removeFromTop (left.getHeight() * 0.58f), juce::Justification::bottomLeft, 21.0f, true);
        juce::String sub;
        if (ctx.hasMessage()) { sub = ctx.message; g.setColour (col::rose); }
        else
        {
            const int cp = p.getCurrentProgram();
            sub = cp >= 0 && cp < (int) p.getPresets().size() ? p.getPresets()[(size_t) cp].category : juce::String();
            g.setColour (col::boneDim);
        }
        text (g, sub.toUpperCase(), left, juce::Justification::topLeft, 13.0f, false, 0.08f);
        // Derecha: tempo, y tocando, el patrón y el compás.
        const bool run = p.view.running.load();
        const double bpm = run ? p.view.bpm.load() : (double) p.value ("tempo");
        auto tempoR = r.removeFromRight (104.0f);
        g.setColour (col::bone); text (g, juce::String (bpm, 1), tempoR.removeFromTop (tempoR.getHeight() * 0.62f), juce::Justification::bottomRight, 25.0f, true);
        g.setColour (col::boneDim); text (g, run && p.value ("sync") > 0.5f ? "HOST BPM" : "BPM", tempoR, juce::Justification::topRight, 12.0f, false, 0.12f);
        auto pos = r.reduced (8.0f, 0);
        if (run && p.view.pattern.load() >= 0)
        {
            const double ppq = p.view.ppq.load(), s0 = p.view.segStart.load();
            const int beat = (int) std::floor (ppq - s0);
            juce::String a = Ctx::patternName (p.view.pattern.load());
            const int e = p.view.entry.load();
            g.setColour (p.view.fill.load() ? col::rose : col::bone); text (g, a, pos.removeFromTop (pos.getHeight() * 0.58f), juce::Justification::bottomRight, 19.0f, true);
            g.setColour (col::boneDim);
            text (g, (e >= 0 ? "SONG " + juce::String (e + 1) + "  " : juce::String()) + "BAR " + juce::String (beat / 4 + 1) + "." + juce::String (beat % 4 + 1), pos, juce::Justification::topRight, 12.5f, false, 0.08f);
        }
        else
        {
            g.setColour (col::boneFaint); text (g, "STOPPED", pos, juce::Justification::centredRight, 13.0f, false, 0.14f);
        }
    }
};

} // namespace dh::pal::ui
