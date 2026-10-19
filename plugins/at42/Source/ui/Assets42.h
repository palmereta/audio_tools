#pragma once
// Imágenes renderizadas en Blender (assets/ui, embebidas como BinaryData) y la geometría del panel.
// Proyección (tools/blender/scene42.py): punto del panel (x, y) mm a altura z mm →
//   px = (x − x0)·k,  py = ((y − y0)·cosθ − z·sinθ)·k   con k = 6 px/mm (imágenes a 2x).

#include <JuceHeader.h>

namespace dh {

struct Sprite42
{
    juce::Image image;
    int size = 0, frames = 1, cols = 1;
    float anchorX = 0, anchorY = 0, d = 0;
    juce::Rectangle<int> frame (int f) const
    {
        f = ((f % frames) + frames) % frames;
        return { (f % cols) * size, (f / cols) * size, size, size };
    }
};

class Assets42
{
public:
    static Assets42& get() { static Assets42 a; return a; }

    std::array<juce::Image, 4> backgrounds;          // crema, negro, maíz, azul
    std::map<juce::String, Sprite42> knobs, buttons, plugs;
    Sprite42 batUp, batMid, batDown, dkeyDown, joyCap;
    float plugTopZ = 13.0f;
    juce::var layout;
    float x0 = -7, y0 = -7, cosT = 0.990f, sinT = 0.139f, pxmm = 6.0f;
    int bgW = 3054, bgH = 1913;
    bool ok = false;

    juce::Point<float> toView (float x, float y, float z = 0.0f) const
    {
        const float s = pxmm * 0.5f;
        return { (x - x0) * s, ((y - y0) * cosT - z * sinT) * s };
    }
    float mm (float v) const { return v * pxmm * 0.5f; }
    float viewWidth() const { return (float) bgW * 0.5f; }
    float viewHeight() const { return (float) bgH * 0.5f; }

    void draw (juce::Graphics& g, const Sprite42& s, juce::Point<float> p, int frame = 0, float scale = 1.0f, float opacity = 1.0f) const
    {
        if (! s.image.isValid()) return;
        const float k = 0.5f * scale;
        g.setOpacity (opacity);
        g.drawImageTransformed (s.image.getClippedImage (s.frame (frame)),
                                juce::AffineTransform::scale (k).translated (p.x - s.anchorX * k, p.y - s.anchorY * k));
        g.setOpacity (1.0f);
    }

private:
    Assets42()
    {
       #if AT42_HAS_ASSETS
        auto img = [] (const juce::String& name) {
            int sz = 0;
            const char* d = BinaryData::getNamedResource (name.toRawUTF8(), sz);
            return d != nullptr ? juce::ImageCache::getFromMemory (d, sz) : juce::Image();
        };
        backgrounds = { img ("bg_cream_jpg"), img ("bg_black_jpg"), img ("bg_maize_jpg"), img ("bg_blue_jpg") };
        int sz = 0;
        const char* metaData = BinaryData::getNamedResource ("ui_json", sz);
        const auto meta = juce::JSON::parse (juce::String::fromUTF8 (metaData, sz));
        const auto bg = meta.getProperty ("background", {});
        x0 = bg.getProperty ("x0", x0); y0 = bg.getProperty ("y0", y0);
        cosT = bg.getProperty ("cosT", cosT); sinT = bg.getProperty ("sinT", sinT);
        pxmm = bg.getProperty ("pxmm", pxmm); bgW = bg.getProperty ("width", bgW); bgH = bg.getProperty ("height", bgH);
        layout = meta.getProperty ("layout", {});
        const auto sp = meta.getProperty ("sprites", {});
        auto fill = [] (Sprite42& s, const juce::var& m, juce::Image im) {
            s.image = im;
            s.size = (int) m.getProperty ("size", im.isValid() ? im.getWidth() : 0);
            s.frames = (int) m.getProperty ("frames", 1); s.cols = (int) m.getProperty ("cols", 1);
            s.anchorX = (float) m.getProperty ("anchorX", s.size * 0.5f); s.anchorY = (float) m.getProperty ("anchorY", s.size * 0.5f);
            s.d = (float) m.getProperty ("d", 0.0f);
        };
        if (auto* o = sp.getProperty ("knobs", {}).getDynamicObject())
            for (auto& kv : o->getProperties())
                fill (knobs[kv.name.toString()], kv.value, img ("knob_" + kv.name.toString() + "_png"));
        if (auto* o = sp.getProperty ("buttons", {}).getDynamicObject())
            for (auto& kv : o->getProperties())
                fill (buttons[kv.name.toString()], kv.value, img (kv.value.getProperty ("file", "").toString() + "_png"));
        const auto bat = sp.getProperty ("bat", {});
        fill (batUp, bat, img ("bat_up_png")); fill (batMid, bat, img ("bat_mid_png")); fill (batDown, bat, img ("bat_down_png"));
        fill (dkeyDown, sp.getProperty ("dkey", {}), img ("dkey_down_png"));
        fill (joyCap, sp.getProperty ("joy", {}), img ("joy_cap_png"));
        const auto pm = sp.getProperty ("plugs", {});
        plugTopZ = (float) pm.getProperty ("topZ", 13.0f);
        for (auto c : { "red", "yellow", "blue", "green", "white", "black" }) fill (plugs[c], pm, img (juce::String ("plug_") + c + "_png"));
        ok = backgrounds[0].isValid() && ! knobs.empty();
       #endif
    }
};

namespace col42 {
    const juce::Colour editorBg { 0xff0e0e0f };
    inline juce::Colour panel (int c)
    {
        switch (c) { case 1: return juce::Colour (0xff1e1e20); case 2: return juce::Colour (0xffd99a22); case 3: return juce::Colour (0xff2c5a88); default: return juce::Colour (0xffe6d8c4); }
    }
    inline juce::Colour ink (int c) { return (c == 1 || c == 3) ? juce::Colour (0xffeeece6) : juce::Colour (0xff161616); }
    inline juce::Colour cable (const juce::String& c)
    {
        if (c == "red") return juce::Colour (0xffb3231c);
        if (c == "blue") return juce::Colour (0xff2452a8);
        if (c == "yellow") return juce::Colour (0xffd8a21e);
        if (c == "green") return juce::Colour (0xff2f8a3c);
        if (c == "white") return juce::Colour (0xffd9d8d2);
        return juce::Colour (0xff1c1c1e);
    }
    inline juce::Colour led (const juce::String& c)
    {
        if (c == "amber") return juce::Colour (0xffffb300);
        if (c == "blue") return juce::Colour (0xff2aa8ff);
        if (c == "green") return juce::Colour (0xff4ce000);
        return juce::Colour (0xffff3020);
    }
    inline juce::Font font (float h, bool bold = true)
    {
        return juce::FontOptions ("Helvetica Neue", h, bold ? juce::Font::bold : juce::Font::plain);
    }
}

} // namespace dh
