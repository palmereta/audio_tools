#pragma once
// DeVicenti: the rendered unit (assets/ui, made by tools/blender/devicenti.py and tools/pack_ui.py). The background
// image is the unit in 3/4 perspective; every control comes with the homography of its top plane (local millimetres,
// origin at the control's centre, u right, v towards the front → image pixels), so the editor draws knob pointers
// and screens in perspective and hit-tests through the inverse. Keys and LEDs also have a crop rectangle and their
// sprites (pressed, lit, green, red) in one atlas, rendered at the same pixels as the background.

#include <JuceHeader.h>
#include "../PanelMap.h"
#include <map>

namespace dh::dv::ui {

// A binary resource by its original file name (assets/ui/bg.jpg → "bg.jpg").
inline const char* resource (const char* filename, int& size)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String (BinaryData::originalFilenames[i]) == filename) return BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
    size = 0; return nullptr;
}

struct Homography
{
    double m[9] { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    juce::Point<float> map (float u, float v) const
    {
        const double w = m[6] * u + m[7] * v + m[8];
        return { (float) ((m[0] * u + m[1] * v + m[2]) / w), (float) ((m[3] * u + m[4] * v + m[5]) / w) };
    }
    juce::Point<float> map (juce::Point<float> p) const { return map (p.x, p.y); }
    Homography inverted() const
    {
        const double* a = m; Homography r;
        const double c0 = a[4] * a[8] - a[5] * a[7], c1 = a[5] * a[6] - a[3] * a[8], c2 = a[3] * a[7] - a[4] * a[6];
        const double det = a[0] * c0 + a[1] * c1 + a[2] * c2;
        if (std::abs (det) < 1e-18) return r;
        const double id = 1.0 / det;
        r.m[0] = c0 * id; r.m[1] = (a[2] * a[7] - a[1] * a[8]) * id; r.m[2] = (a[1] * a[5] - a[2] * a[4]) * id;
        r.m[3] = c1 * id; r.m[4] = (a[0] * a[8] - a[2] * a[6]) * id; r.m[5] = (a[2] * a[3] - a[0] * a[5]) * id;
        r.m[6] = c2 * id; r.m[7] = (a[1] * a[6] - a[0] * a[7]) * id; r.m[8] = (a[0] * a[4] - a[1] * a[3]) * id;
        return r;
    }
    static Homography fromVar (const juce::var& v)
    {
        Homography h;
        if (auto* a = v.getArray()) for (int i = 0; i < 9 && i < a->size(); ++i) h.m[i] = (double) (*a)[i];
        return h;
    }
};

struct Ctl
{
    juce::String id;
    panel::Kind kind = panel::Kind::Knob;
    int panelIndex = -1;                       // index into panel::controls()
    float size = 0, r = 0, w = 0, h = 0;      // mm
    Homography top, base, topInv, baseInv;
    juce::Rectangle<int> crop;                 // background pixels covered by the sprites
    std::map<juce::String, juce::Rectangle<int>> sprites;    // "down", "lit", "green", "red" → atlas rectangle
    juce::Rectangle<float> boundsPx;           // approximate screen-space bounds in background pixels (repaints, hit order)
};

class Model
{
public:
    static const Model& get() { static const Model m; return m; }
    bool ok = false;
    int width = 0, height = 0;                 // background pixels
    float scale = 2.0f;                        // background pixels per logical pixel of the default editor
    juce::Image bg, atlas;
    std::vector<Ctl> ctls;
    const Ctl* find (const juce::String& id) const { for (const auto& c : ctls) if (c.id == id) return &c; return nullptr; }

private:
    Model()
    {
        int sz = 0;
        const char* js = resource ("ui.json", sz);
        if (js == nullptr) return;
        const auto v = juce::JSON::parse (juce::String::fromUTF8 (js, sz));
        width = (int) v["width"]; height = (int) v["height"]; scale = (float) (double) v["scale"];
        if (const char* b = resource ("bg.jpg", sz)) bg = juce::ImageCache::getFromMemory (b, sz);
        if (const char* b = resource ("sprites.png", sz)) atlas = juce::ImageCache::getFromMemory (b, sz);
        const auto& pc = panel::controls();
        if (auto* arr = v["controls"].getArray())
            for (const auto& cv : *arr)
            {
                Ctl c; c.id = cv["id"].toString();
                for (int i = 0; i < (int) pc.size(); ++i) if (c.id == pc[(size_t) i].id) { c.panelIndex = i; c.kind = pc[(size_t) i].kind; }
                if (c.panelIndex < 0) continue;
                c.size = (float) (double) cv["size"]; c.r = (float) (double) cv.getProperty ("r", c.size * 0.375f);
                c.w = (float) (double) cv.getProperty ("w", c.size); c.h = (float) (double) cv.getProperty ("h", c.size);
                c.top = Homography::fromVar (cv["top"]); c.topInv = c.top.inverted();
                c.base = cv.hasProperty ("base") ? Homography::fromVar (cv["base"]) : c.top; c.baseInv = c.base.inverted();
                if (auto* cr = cv["crop"].getArray(); cr != nullptr && cr->size() == 4) c.crop = { (int) (*cr)[0], (int) (*cr)[1], (int) (*cr)[2], (int) (*cr)[3] };
                if (auto* so = cv["sprites"].getDynamicObject())
                    for (const auto& kv : so->getProperties())
                        if (auto* a = kv.value.getArray(); a != nullptr && a->size() == 4)
                            c.sprites[kv.name.toString()] = { (int) (*a)[0], (int) (*a)[1], (int) (*a)[2], (int) (*a)[3] };
                // Screen-space bounds: the top plane's extent (and the base for knobs).
                const float hw = c.kind == panel::Kind::Knob ? c.size * 0.5f : c.w * 0.5f + 1.5f, hh = c.kind == panel::Kind::Knob ? c.size * 0.5f : c.h * 0.5f + 1.5f;
                float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
                for (const auto* H : { &c.top, &c.base })
                    for (float su : { -1.0f, 1.0f }) for (float sv : { -1.0f, 1.0f })
                    {
                        const auto p = H->map (su * hw, sv * hh);
                        x0 = std::min (x0, p.x); y0 = std::min (y0, p.y); x1 = std::max (x1, p.x); y1 = std::max (y1, p.y);
                    }
                c.boundsPx = juce::Rectangle<float> (x0, y0, x1 - x0, y1 - y0).expanded (4.0f);
                ctls.push_back (std::move (c));
            }
        ok = bg.isValid() && ! ctls.empty();
    }
};

} // namespace dh::dv::ui
