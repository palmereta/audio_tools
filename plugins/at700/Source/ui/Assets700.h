#pragma once
// Imágenes renderizadas en Blender (assets/ui, embebidas como BinaryData) y la geometría del lienzo.
// Proyección (tools/blender/scene700.py): punto (x, y) mm a altura z mm →
//   px = (x − x0)·k,  py = ((y − y0)·cosθ − z·sinθ)·k   con k = 4 px/mm (imagen a 2x).

#include <JuceHeader.h>

namespace dh {

struct Sprite700
{
    juce::Image image;
    int size = 0, frames = 1, cols = 1;
    float anchorX = 0, anchorY = 0, d = 0;
    juce::Rectangle<int> frame (int f) const
    {
        if (frames <= 1) return image.getBounds();
        f = ((f % frames) + frames) % frames;
        return { (f % cols) * size, (f / cols) * size, size, size };
    }
};

class Assets700
{
public:
    static Assets700& get() { static Assets700 a; return a; }

    juce::Image background;
    std::map<juce::String, Sprite700> knobs, buttons, plugs;
    Sprite700 faderV, faderH, nubV, nubH, lever, batUp, batMid, batDown, batLeft, batRight;
    float plugTopZ = 26.5f;
    juce::var layout;
    float x0 = 0, y0 = 0, cosT = 0.990f, sinT = 0.139f, pxmm = 4.0f;
    int bgW = 3600, bgH = 4318;
    bool ok = false;

    juce::Point<float> toView (float x, float y, float z = 0.0f) const
    {
        const float s = pxmm * 0.5f;
        return { (x - x0) * s, ((y - y0) * cosT - z * sinT) * s };
    }
    float mm (float v) const { return v * pxmm * 0.5f; }
    float yOf (float ymm) const { return (ymm - y0) * cosT * pxmm * 0.5f; }
    float viewWidth() const { return (float) bgW * 0.5f; }
    float viewHeight() const { return (float) bgH * 0.5f; }

    void draw (juce::Graphics& g, const Sprite700& s, juce::Point<float> p, int frame = 0, float scale = 1.0f, float opacity = 1.0f) const
    {
        if (! s.image.isValid()) return;
        const float k = 0.5f * scale;
        g.setOpacity (opacity);
        g.drawImageTransformed (s.frames > 1 ? s.image.getClippedImage (s.frame (frame)) : s.image,
                                juce::AffineTransform::scale (k).translated (p.x - s.anchorX * k, p.y - s.anchorY * k));
        g.setOpacity (1.0f);
    }

private:
    Assets700()
    {
       #if AT700_HAS_ASSETS
        auto img = [] (const juce::String& name) {
            int sz = 0;
            const char* d = BinaryData::getNamedResource (name.toRawUTF8(), sz);
            return d != nullptr ? juce::ImageCache::getFromMemory (d, sz) : juce::Image();
        };
        background = img ("bg_jpg");
        int sz = 0;
        const char* metaData = BinaryData::getNamedResource ("ui_json", sz);
        const auto meta = juce::JSON::parse (juce::String::fromUTF8 (metaData, sz));
        const auto bg = meta.getProperty ("background", {});
        x0 = bg.getProperty ("x0", x0); y0 = bg.getProperty ("y0", y0);
        cosT = bg.getProperty ("cosT", cosT); sinT = bg.getProperty ("sinT", sinT);
        pxmm = bg.getProperty ("pxmm", pxmm); bgW = bg.getProperty ("width", bgW); bgH = bg.getProperty ("height", bgH);
        layout = meta.getProperty ("layout", {});
        const auto sp = meta.getProperty ("sprites", {});
        auto fill = [] (Sprite700& s, const juce::var& m, juce::Image im) {
            s.image = im;
            s.size = (int) std::lround ((double) m.getProperty ("size", im.isValid() ? im.getWidth() : 0));
            s.frames = (int) m.getProperty ("frames", 1); s.cols = (int) m.getProperty ("cols", 1);
            const float w = (float) (double) m.getProperty ("w", (double) s.size), h = (float) (double) m.getProperty ("h", (double) s.size);
            s.anchorX = (float) (double) m.getProperty ("anchorX", w * 0.5); s.anchorY = (float) (double) m.getProperty ("anchorY", h * 0.5);
            if (s.frames <= 1 && im.isValid()) { s.anchorX *= (float) im.getWidth() / std::max (1.0f, w); s.anchorY *= (float) im.getHeight() / std::max (1.0f, h); }
            s.d = (float) m.getProperty ("d", 0.0f);
        };
        if (auto* o = sp.getProperty ("knobs", {}).getDynamicObject())
            for (auto& kv : o->getProperties())
                fill (knobs[kv.name.toString()], kv.value, img ("knob_" + kv.name.toString() + "_png"));
        if (auto* o = sp.getProperty ("buttons", {}).getDynamicObject())
            for (auto& kv : o->getProperties())
                fill (buttons[kv.name.toString()], kv.value, img (kv.value.getProperty ("file", "").toString() + "_png"));
        fill (faderV, sp.getProperty ("fader_v", {}), img ("fader_v_png"));
        fill (faderH, sp.getProperty ("fader_h", {}), img ("fader_h_png"));
        fill (nubV, sp.getProperty ("nub_v", {}), img ("nub_v_png"));
        fill (nubH, sp.getProperty ("nub_h", {}), img ("nub_h_png"));
        fill (lever, sp.getProperty ("lever", {}), img ("lever_png"));
        const auto bat = sp.getProperty ("bat", {});
        fill (batUp, bat, img ("bat_up_png")); fill (batMid, bat, img ("bat_mid_png")); fill (batDown, bat, img ("bat_down_png"));
        fill (batLeft, bat, img ("bat_left_png")); fill (batRight, bat, img ("bat_right_png"));
        const auto pm = sp.getProperty ("plugs", {});
        plugTopZ = (float) pm.getProperty ("topZ", 26.5f);
        for (auto c : { "red", "yellow", "blue", "green", "white", "black" }) fill (plugs[c], pm, img (juce::String ("plug_") + c + "_png"));
        ok = background.isValid() && ! knobs.empty();
       #endif
    }
};

namespace col700 {
    const juce::Colour editorBg { 0xff0e0e0f };
    inline juce::Colour cable (const juce::String& c)
    {
        if (c == "red") return juce::Colour (0xffa8201a);
        if (c == "blue") return juce::Colour (0xff1f4a9e);
        if (c == "yellow") return juce::Colour (0xffd0a01c);
        if (c == "green") return juce::Colour (0xff2b8238);
        if (c == "white") return juce::Colour (0xffd2d1cb);
        return juce::Colour (0xff1a1a1c);
    }
    inline juce::Colour led (const juce::String& c) { return c == "green" ? juce::Colour (0xff3ee020) : juce::Colour (0xffff2a18); }
    inline juce::Font font (float h, bool bold = true) { return juce::FontOptions ("Helvetica Neue", h, bold ? juce::Font::bold : juce::Font::plain); }
}

} // namespace dh
