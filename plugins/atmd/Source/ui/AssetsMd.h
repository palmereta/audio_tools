#pragma once
// Imágenes renderizadas en Blender (assets/ui, embebidas como BinaryData) y su contrato, ui.json
// (docs/ui-assets.md): el fondo con todo lo fijo, las hojas de cuadros de perillas y ruedas, los rockers y
// lámparas, la imagen de "todas las teclas apretadas" y, por control, dónde va y qué zona responde al mouse.
// Todo en píxeles del fondo (2x: 3004 × 1788); la vista lo escala.

#include <JuceHeader.h>

namespace dh::md::ui {

struct Sprite
{
    juce::Image image;
    int w = 0, h = 0, frames = 1, cols = 1;
    juce::Point<float> anchor;
    juce::Rectangle<int> frame (int f) const
    {
        f = ((f % frames) + frames) % frames;
        return { (f % cols) * w, (f / cols) * h, w, h };
    }
};

struct Control
{
    juce::String id, type;                     // knob, rotary, rocker, lamp, wheel
    juce::Point<float> px;                     // ancla en el fondo
    juce::String sprite;                       // knob / rotary / wheel
    juce::StringArray sprites;                 // rocker / lamp: [apagado, encendido]
    juce::Path hit;                            // zona que responde al mouse
    float a0 = -150.0f, a1 = 150.0f;           // ángulos de la perilla
    std::vector<float> detents;                // rotary: ángulo de cada posición
    float dragPx = 200.0f;                     // rueda: recorrido completo en px del fondo
    bool centerDetent = false;
    juce::Rectangle<float> bounds;             // lo que hay que repintar
};

struct Key { int note; bool black; juce::Path hull; };

class Assets
{
public:
    static Assets& get() { static Assets a; return a; }

    juce::Image background, keysDown;
    juce::Rectangle<float> keysCrop;
    std::map<juce::String, Sprite> sprites;
    std::vector<Control> controls;
    std::vector<Key> keys;                      // las negras primero (para el clic)
    int width = 3004, height = 1788;
    bool ok = false;

    const Control* find (const juce::String& id) const
    {
        for (auto& c : controls) if (c.id == id) return &c;
        return nullptr;
    }

    void draw (juce::Graphics& g, const juce::String& spriteId, juce::Point<float> at, int frame = 0) const
    {
        auto it = sprites.find (spriteId);
        if (it == sprites.end() || ! it->second.image.isValid()) return;
        const auto& s = it->second;
        g.drawImageAt (s.image.getClippedImage (s.frame (frame)), juce::roundToInt (at.x - s.anchor.x), juce::roundToInt (at.y - s.anchor.y));
    }

private:
    static juce::Image image (const char* name)
    {
        int size = 0;
        if (const char* data = BinaryData::getNamedResource (name, size)) return juce::ImageFileFormat::loadFrom (data, (size_t) size);
        return {};
    }
    static juce::String resourceName (const juce::String& file) { return file.replaceCharacter ('.', '_').replaceCharacter ('-', '_'); }

    Assets()
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource ("ui_json", size);
        if (data == nullptr) return;
        const auto j = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        background = image ("bg_jpg");
        width = j["background"]["width"]; height = j["background"]["height"];
        const auto& kj = j["keys"];
        keysDown = image (resourceName (kj["file"].toString()).toRawUTF8());
        if (auto* c = kj["crop"].getArray(); c != nullptr && c->size() == 4)
            keysCrop = { (float) (*c)[0], (float) (*c)[1], (float) (*c)[2] - (float) (*c)[0], (float) (*c)[3] - (float) (*c)[1] };
        auto point = [] (const juce::var& v) { return juce::Point<float> ((float) v[0], (float) v[1]); };
        auto poly = [&] (const juce::var& pts) {
            juce::Path p;
            if (auto* a = pts.getArray())
                for (int i = 0; i < a->size(); ++i) { const auto q = point ((*a)[i]); if (i == 0) p.startNewSubPath (q); else p.lineTo (q); }
            p.closeSubPath(); return p;
        };
        if (auto* ks = kj["keys"].getArray())
        {
            for (auto& k : *ks) keys.push_back ({ (int) k["note"], (bool) k["black"], poly (k["hull"]) });
            std::stable_sort (keys.begin(), keys.end(), [] (const Key& a, const Key& b) { return a.black > b.black; });
        }
        if (auto* sp = j["sprites"].getDynamicObject())
            for (auto& prop : sp->getProperties())
            {
                const auto& v = prop.value;
                Sprite s;
                s.image = image (resourceName (v["file"].toString()).toRawUTF8());
                s.w = v["w"]; s.h = v["h"];
                s.frames = std::max (1, (int) v.getProperty ("frames", 1));
                s.cols = std::max (1, (int) v.getProperty ("cols", 1));
                s.anchor = point (v["anchor"]);
                sprites[prop.name.toString()] = s;
            }
        if (auto* cs = j["controls"].getArray())
            for (auto& c : *cs)
            {
                Control k;
                k.id = c["id"]; k.type = c["type"];
                k.px = point (c["px"]);
                k.sprite = c.getProperty ("sprite", "").toString();
                if (auto* a = c["sprites"].getArray()) for (auto& s : *a) k.sprites.add (s.toString());
                if (c.hasProperty ("hitRadiusPx")) { const auto r = point (c["hitRadiusPx"]); k.hit.addEllipse (k.px.x - r.x, k.px.y - r.y, 2 * r.x, 2 * r.y); }
                else if (c.hasProperty ("hit")) k.hit = poly (c["hit"]);
                k.a0 = (float) c.getProperty ("a0", -150.0); k.a1 = (float) c.getProperty ("a1", 150.0);
                if (auto* d = c["detentsDeg"].getArray()) for (auto& x : *d) k.detents.push_back ((float) x);
                k.dragPx = (float) c.getProperty ("dragPxPerFullTravel", 200.0);
                k.centerDetent = (bool) c.getProperty ("centerDetent", false);
                // Lo que ocupa en pantalla: el cuadro del sprite alrededor del ancla, o la zona de clic.
                const juce::String sid = k.sprite.isNotEmpty() ? k.sprite : (k.sprites.isEmpty() ? juce::String() : k.sprites[0]);
                auto it = sprites.find (sid);
                k.bounds = it != sprites.end() ? juce::Rectangle<float> (k.px.x - it->second.anchor.x, k.px.y - it->second.anchor.y, (float) it->second.w, (float) it->second.h)
                                               : k.hit.getBounds();
                k.bounds = k.bounds.getUnion (k.hit.getBounds()).expanded (2.0f);
                controls.push_back (k);
            }
        ok = background.isValid() && ! controls.empty();
    }
};

} // namespace dh::md::ui
