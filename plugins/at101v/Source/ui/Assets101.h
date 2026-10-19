#pragma once
// Imágenes renderizadas en Blender (assets/ui, embebidas como BinaryData) y la geometría del panel.
//
// Proyección (tools/blender/scene101.py): un punto del panel (x, y) mm a altura z mm cae en el fondo
//   px = (x − x0)·k,   py = ((y − y0)·cosθ − z·sinθ)·k     con k = 5,2 px/mm (imágenes a 2x).
// En coordenadas del editor (1x) se divide por 2.

#include <JuceHeader.h>

namespace dh {

struct Sprite
{
    juce::Image image;
    int size = 0, frames = 1, cols = 1;
    float anchorX = 0, anchorY = 0;
    juce::Rectangle<int> frame (int f) const
    {
        f = ((f % frames) + frames) % frames;
        return { (f % cols) * size, (f / cols) * size, size, size };
    }
};

class Assets101
{
public:
    static Assets101& get() { static Assets101 a; return a; }

    std::array<juce::Image, 3> backgrounds;     // gris, azul, rojo
    juce::Image keysDown;
    juce::Rectangle<int> keysCrop;
    Sprite capGreen, capOrange, swcap, knob, nub, lever, buttonDown, powerDown, ledOn;
    struct KeyShape { int note; bool black; juce::Path hull; };
    std::vector<KeyShape> keys;
    juce::var layout;
    float x0 = -8, y0 = -8, cosT = 0.990f, sinT = 0.139f, pxmm = 5.2f;
    int bgW = 3047, bgH = 1684;
    bool ok = false;

    juce::Point<float> toView (float x, float y, float z = 0.0f) const
    {
        const float s = pxmm * 0.5f;
        return { (x - x0) * s, ((y - y0) * cosT - z * sinT) * s };
    }
    float mmToView (float mm) const { return mm * pxmm * 0.5f; }
    float viewWidth() const { return (float) bgW * 0.5f; }
    float viewHeight() const { return (float) bgH * 0.5f; }

    // Dibuja un sprite con su ancla en p (coordenadas de vista), escalado.
    void draw (juce::Graphics& g, const Sprite& s, juce::Point<float> p, int frame = 0, float scale = 1.0f) const
    {
        if (! s.image.isValid()) return;
        const auto src = s.frame (frame);
        const float k = 0.5f * scale;
        g.setOpacity (1.0f);
        g.drawImageTransformed (s.image.getClippedImage (src),
                                juce::AffineTransform::scale (k).translated (p.x - s.anchorX * k, p.y - s.anchorY * k));
    }

private:
    Assets101()
    {
       #if AT101V_HAS_ASSETS
        auto img = [] (const char* name) {
            int sz = 0;
            const char* d = BinaryData::getNamedResource (name, sz);
            return d != nullptr ? juce::ImageCache::getFromMemory (d, sz) : juce::Image();
        };
        backgrounds = { img ("bg_grey_jpg"), img ("bg_blue_jpg"), img ("bg_red_jpg") };
        keysDown = img ("keysdown_jpg");
        int sz = 0;
        const char* metaData = BinaryData::getNamedResource ("ui_json", sz);
        const auto meta = juce::JSON::parse (juce::String::fromUTF8 (metaData, sz));
        const auto bg = meta.getProperty ("background", {});
        x0 = bg.getProperty ("x0", x0); y0 = bg.getProperty ("y0", y0);
        cosT = bg.getProperty ("cosT", cosT); sinT = bg.getProperty ("sinT", sinT);
        pxmm = bg.getProperty ("pxmm", pxmm);
        bgW = bg.getProperty ("width", bgW); bgH = bg.getProperty ("height", bgH);
        layout = meta.getProperty ("layout", {});
        const auto sp = meta.getProperty ("sprites", {});
        auto sprite = [&] (Sprite& s, const char* imgName, const char* key) {
            s.image = img (imgName);
            const auto m = sp.getProperty (key, {});
            s.size = (int) m.getProperty ("size", 0); s.frames = (int) m.getProperty ("frames", 1); s.cols = (int) m.getProperty ("cols", 1);
            s.anchorX = (float) m.getProperty ("anchorX", 0.0f); s.anchorY = (float) m.getProperty ("anchorY", 0.0f);
            if (s.size == 0 && s.image.isValid()) s.size = s.image.getWidth();
            if (s.anchorX == 0.0f) { s.anchorX = s.anchorY = (float) s.size * 0.5f; }
        };
        sprite (capGreen, "cap_green_png", "cap");
        sprite (capOrange, "cap_orange_png", "cap");
        sprite (swcap, "swcap_yellow_png", "swcap");
        sprite (knob, "knob_png", "knob");
        sprite (nub, "nub_png", "nub");
        sprite (lever, "lever_png", "lever");
        sprite (buttonDown, "button_down_png", "button");
        sprite (powerDown, "power_down_png", "power");
        sprite (ledOn, "led_on_png", "led");
        const auto km = meta.getProperty ("keys", {});
        if (auto* c = km.getProperty ("crop", {}).getArray(); c != nullptr && c->size() == 4)
            keysCrop = { (int) (*c)[0], (int) (*c)[1], (int) (*c)[2] - (int) (*c)[0], (int) (*c)[3] - (int) (*c)[1] };
        if (auto* arr = km.getProperty ("keys", {}).getArray())
            for (auto& k : *arr)
            {
                KeyShape ks { (int) k.getProperty ("note", 0), (bool) k.getProperty ("black", false), {} };
                if (auto* pts = k.getProperty ("hull", {}).getArray())
                    for (int i = 0; i < pts->size(); ++i)
                    {
                        const auto& pt = pts->getReference (i);
                        const juce::Point<float> q ((float) pt[0] * 0.5f, (float) pt[1] * 0.5f);
                        if (i == 0) ks.hull.startNewSubPath (q); else ks.hull.lineTo (q);
                    }
                ks.hull.closeSubPath();
                keys.push_back (ks);
            }
        ok = backgrounds[0].isValid() && knob.image.isValid();
       #endif
    }
};

// Colores de la casa (serigrafía, LEDs, plásticos), compartidos por las páginas dibujadas.
namespace col101 {
    const juce::Colour silk { 0xffeeece6 }, silkDim { 0xffb9b6ad }, slot { 0xff121213 }, ledRed { 0xffff3b22 },
                       ledOff { 0xff3a1414 }, cream { 0xffd9d3c4 }, orange { 0xfff0641e }, green { 0xff3fb34f },
                       yellow { 0xfff2c230 }, editorBg { 0xff0e0e0f };
    inline juce::Colour body (int c)
    {
        switch (c) { case 1: return juce::Colour (0xff24629b); case 2: return juce::Colour (0xffae352f); default: return juce::Colour (0xff53524f); }
    }
    inline juce::Font font (float h, bool bold = true)
    {
        return juce::FontOptions ("Helvetica Neue", h, bold ? juce::Font::bold : juce::Font::plain).withFallbacks ({ "Arial", "Liberation Sans" });
    }
}

} // namespace dh
