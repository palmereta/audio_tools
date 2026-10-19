#pragma once
// Frente renderizado en Blender (assets/ui, embebido como BinaryData) y su geometría.
// Proyección (tools/blender/scenefs1.py): punto (x, y) mm a altura z mm →
//   px = (x − x0)·k,  py = ((y − y0)·cosθ − z·sinθ)·k   con k = pxmm/2 (la imagen está a 2x).

#include <JuceHeader.h>

namespace dh::fs::ui {

struct Sprite
{
    juce::Image image;
    int size = 0, frames = 1, cols = 1;
    float anchorX = 0, anchorY = 0;
    juce::Rectangle<int> frame (int f) const
    {
        if (frames <= 1) return image.getBounds();
        f = ((f % frames) + frames) % frames;
        return { (f % cols) * size, (f / cols) * size, size, size };
    }
};

class Assets
{
public:
    static Assets& get() { static Assets a; return a; }

    juce::Image background;
    Sprite knobBig, knobSmall, btnGreyUp, btnGreyDown, btnBlackUp, btnBlackDown, ledOn;
    juce::var layout;
    float x0 = 0, y0 = 0, cosT = 0.9925f, sinT = -0.1219f, pxmm = 8.0f;
    int bgW = 3861, bgH = 357;
    bool ok = false;

    juce::Point<float> toView (float x, float y, float z = 0.0f) const
    {
        const float s = pxmm * 0.5f;
        return { (x - x0) * s, ((y - y0) * cosT - z * sinT) * s };
    }
    float mm (float v) const { return v * pxmm * 0.5f; }
    float viewWidth() const { return (float) bgW * 0.5f; }
    float viewHeight() const { return (float) bgH * 0.5f; }

    void draw (juce::Graphics& g, const Sprite& s, juce::Point<float> p, int frame = 0, float opacity = 1.0f) const
    {
        if (! s.image.isValid()) return;
        const float k = 0.5f;
        g.setOpacity (opacity);
        g.drawImageTransformed (s.frames > 1 ? s.image.getClippedImage (s.frame (frame)) : s.image,
                                juce::AffineTransform::scale (k).translated (p.x - s.anchorX * k, p.y - s.anchorY * k));
        g.setOpacity (1.0f);
    }

private:
    Assets()
    {
       #if ATFS1_HAS_ASSETS
        auto img = [] (const juce::String& name) {
            int sz = 0;
            const char* d = BinaryData::getNamedResource (name.toRawUTF8(), sz);
            return d != nullptr ? juce::ImageCache::getFromMemory (d, sz) : juce::Image();
        };
        background = img ("bg_jpg");
        int sz = 0;
        const char* metaData = BinaryData::getNamedResource ("ui_json", sz);
        if (metaData == nullptr) return;
        const auto meta = juce::JSON::parse (juce::String::fromUTF8 (metaData, sz));
        const auto bg = meta.getProperty ("background", {});
        x0 = bg.getProperty ("x0", x0); y0 = bg.getProperty ("y0", y0);
        cosT = bg.getProperty ("cosT", cosT); sinT = bg.getProperty ("sinT", sinT);
        pxmm = bg.getProperty ("pxmm", pxmm); bgW = bg.getProperty ("width", bgW); bgH = bg.getProperty ("height", bgH);
        layout = meta.getProperty ("layout", {});
        const auto sp = meta.getProperty ("sprites", {});
        auto fill = [] (Sprite& s, const juce::var& m, juce::Image im) {
            s.image = im;
            s.size = (int) std::lround ((double) m.getProperty ("size", im.isValid() ? im.getWidth() : 0));
            s.frames = (int) m.getProperty ("frames", 1); s.cols = (int) m.getProperty ("cols", 1);
            const float w = (float) (double) m.getProperty ("w", (double) s.size), h = (float) (double) m.getProperty ("h", (double) s.size);
            s.anchorX = (float) (double) m.getProperty ("anchorX", w * 0.5); s.anchorY = (float) (double) m.getProperty ("anchorY", h * 0.5);
            if (s.frames <= 1 && im.isValid()) { s.anchorX *= (float) im.getWidth() / std::max (1.0f, w); s.anchorY *= (float) im.getHeight() / std::max (1.0f, h); }
        };
        const auto kn = sp.getProperty ("knobs", {});
        fill (knobBig, kn.getProperty ("big", {}), img ("knob_big_png"));
        fill (knobSmall, kn.getProperty ("small", {}), img ("knob_small_png"));
        const auto bt = sp.getProperty ("buttons", {});
        fill (btnGreyUp, bt.getProperty ("btn_grey_up", {}), img ("btn_grey_up_png"));
        fill (btnGreyDown, bt.getProperty ("btn_grey_down", {}), img ("btn_grey_down_png"));
        fill (btnBlackUp, bt.getProperty ("btn_black_up", {}), img ("btn_black_up_png"));
        fill (btnBlackDown, bt.getProperty ("btn_black_down", {}), img ("btn_black_down_png"));
        fill (ledOn, sp.getProperty ("led_on", {}), img ("led_on_png"));
        ok = background.isValid() && knobBig.image.isValid();
       #endif
    }
};

} // namespace dh::fs::ui
