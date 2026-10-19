#pragma once
// Imágenes renderizadas en Blender (assets/ui, embebidas como BinaryData) y su geometría.
//
// Proyección (tools/blender/scene.py): un punto del panel (u, v) a altura z cae en el fondo en
//   x = (u − u0)·px,   y = (v − v0)·cosθ·px − z·sinθ·px
// con px = 2 (las imágenes están a 2x). En coordenadas del editor (1x) se divide por 2.

#include <JuceHeader.h>
#include "Layout.generated.h"

namespace dh {

struct SpriteSheet
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

class Assets
{
public:
    static Assets& get() { static Assets a; return a; }

    juce::Image background, backgroundExt, ledOn;
    // Teclado renderizado (todo en px de la imagen a 2x).
    juce::Image keysUp, keysDown, wheelSheet, switchDown;
    struct KeyShape { int note; bool black; juce::Path hull; };
    std::vector<KeyShape> keyShapes;
    juce::Rectangle<int> wheelCrop, switchCrop;
    int wheelFrames = 1, wheelCols = 1, wheelFw = 1, wheelFh = 1;
    float wheelRange = 70.0f;
    SpriteSheet knobSmall, knobBig;
    std::map<juce::String, juce::Image> plugs;
    float plugAnchorX = 0, plugAnchorY = 0, plugSize = 0, plugTopZ = 52;
    float u0 = -46, v0 = -58, cosT = 0.978f, sinT = 0.208f, pxPerUnit = 2;
    int bgW = 3384, bgH = 1187;
    bool ok = false;

    // Coordenadas del editor (1x) de un punto del panel.
    juce::Point<float> toView (float u, float v, float z = 0.0f) const
    {
        const float s = pxPerUnit * 0.5f;
        return { (u - u0) * s, (v - v0) * cosT * s - z * sinT * s };
    }
    float viewWidth() const { return (float) bgW * 0.5f; }
    float viewHeight() const { return (float) bgH * 0.5f; }

private:
    Assets()
    {
       #if AT20V_HAS_ASSETS
        auto img = [] (const char* name) {
            int sz = 0;
            const char* d = BinaryData::getNamedResource (name, sz);
            return d != nullptr ? juce::ImageCache::getFromMemory (d, sz) : juce::Image();
        };
        background = img ("background_png");
        backgroundExt = img ("background_ext_png");
        keysUp = img ("keys_up_png");
        keysDown = img ("keys_down_png");
        wheelSheet = img ("wheel_png");
        switchDown = img ("switch_down_png");
        ledOn = img ("led_on_png");
        int sz = 0;
        const char* metaData = BinaryData::getNamedResource ("ui_json", sz);
        const auto meta = juce::JSON::parse (juce::String::fromUTF8 (metaData, sz));
        const auto bg = meta.getProperty ("background", {});
        u0 = bg.getProperty ("u0", u0); v0 = bg.getProperty ("v0", v0);
        cosT = bg.getProperty ("cosTheta", cosT); sinT = bg.getProperty ("sinTheta", sinT);
        pxPerUnit = bg.getProperty ("pxPerUnit", pxPerUnit);
        bgW = bg.getProperty ("width", bgW); bgH = bg.getProperty ("height", bgH);
        auto sheet = [&] (SpriteSheet& s, const char* imgName, const char* key) {
            s.image = img (imgName);
            const auto m = meta.getProperty (key, {});
            s.size = m.getProperty ("size", 0); s.frames = m.getProperty ("frames", 1); s.cols = m.getProperty ("cols", 1);
            s.anchorX = m.getProperty ("anchorX", 0.0f); s.anchorY = m.getProperty ("anchorY", 0.0f);
        };
        sheet (knobSmall, "knob_small_png", "knob_small");
        sheet (knobBig, "knob_big_png", "knob_big");
        for (auto c : { "black", "red", "blue", "yellow", "green", "white" })
            plugs[c] = img ((juce::String ("plug_") + c + "_png").toRawUTF8());
        const auto pm = meta.getProperty ("plugs", {});
        plugAnchorX = pm.getProperty ("anchorX", 0.0f); plugAnchorY = pm.getProperty ("anchorY", 0.0f);
        plugSize = pm.getProperty ("size", 0.0f); plugTopZ = pm.getProperty ("topZ", 52.0f);
        const auto km = meta.getProperty ("keys", {});
        if (auto* arr = km.getProperty ("keys", {}).getArray())
            for (auto& k : *arr)
            {
                KeyShape ks { (int) k.getProperty ("note", 0), (bool) k.getProperty ("black", false), {} };
                if (auto* pts = k.getProperty ("hull", {}).getArray())
                    for (int i = 0; i < pts->size(); ++i)
                    {
                        const auto& pt = pts->getReference (i);
                        const juce::Point<float> q ((float) pt[0], (float) pt[1]);
                        if (i == 0) ks.hull.startNewSubPath (q); else ks.hull.lineTo (q);
                    }
                ks.hull.closeSubPath();
                keyShapes.push_back (ks);
            }
        auto rect = [] (const juce::var& a) {
            return a.isArray() && a.size() == 4 ? juce::Rectangle<int> ((int) a[0], (int) a[1], (int) a[2], (int) a[3]) : juce::Rectangle<int>();
        };
        wheelCrop = rect (km.getProperty ("wheelCrop", {}));
        switchCrop = rect (km.getProperty ("switchCrop", {}));
        const auto wm = km.getProperty ("wheel", {});
        wheelFrames = wm.getProperty ("frames", 1); wheelCols = wm.getProperty ("cols", 1);
        wheelFw = wm.getProperty ("fw", 1); wheelFh = wm.getProperty ("fh", 1);
        wheelRange = wm.getProperty ("range", 70.0f);
        ok = background.isValid() && knobSmall.image.isValid();
       #endif
    }
};

} // namespace dh
