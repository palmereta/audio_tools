#pragma once
// El manual dentro del plugin: lee docs/manual/en.md y es.md (embebidos con dh_add_manual, ver manual.cmake)
// con el subconjunto de Markdown de FORMAT.md y lo muestra encima del editor: índice a la izquierda, texto con
// las capturas a la derecha, inglés o español. Esc o la × lo cierran.

#include <JuceHeader.h>
#if DH_HAS_MANUAL
 #include "ManualData.h"
#endif

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wswitch-enum")

namespace dh::manual {

struct Theme
{
    juce::Colour bg { 0xff15161a }, panel { 0xff1d1f24 }, text { 0xffe9e6df }, muted { 0xff9a968e }, accent { 0xfff2c878 }, line { 0x22ffffff };
};

// ───────── Markdown ─────────
struct Span { juce::String text; bool bold = false, code = false; };
struct Block
{
    enum Kind { H1, H2, H3, Para, Bullet, Step, Image, Table, Note } kind = Para;
    std::vector<Span> spans;                       // texto con formato (párrafos, listas, notas, títulos)
    int number = 0;                                // pasos numerados
    juce::String image, caption;
    std::vector<std::vector<juce::String>> rows;   // tabla (la primera es el encabezado)
};

inline std::vector<Span> parseInline (const juce::String& s)
{
    std::vector<Span> out;
    juce::String cur; bool bold = false, code = false;
    auto flush = [&] { if (cur.isNotEmpty()) out.push_back ({ cur, bold, code }); cur.clear(); };
    for (int i = 0; i < s.length(); ++i)
    {
        if (! code && s[i] == '*' && i + 1 < s.length() && s[i + 1] == '*') { flush(); bold = ! bold; ++i; continue; }
        if (s[i] == '`') { flush(); code = ! code; continue; }
        cur += s[i];
    }
    flush();
    return out;
}

inline std::vector<Block> parse (const juce::String& md)
{
    std::vector<Block> blocks;
    juce::StringArray lines; lines.addLines (md);
    juce::String para;
    auto flushPara = [&] { if (para.trim().isNotEmpty()) { Block b; b.kind = Block::Para; b.spans = parseInline (para.trim()); blocks.push_back (b); } para.clear(); };
    for (int i = 0; i < lines.size(); ++i)
    {
        const auto l = lines[i].trimEnd();
        const auto t = l.trimStart();
        if (t.isEmpty()) { flushPara(); continue; }
        if (t.startsWith ("### ")) { flushPara(); Block b; b.kind = Block::H3; b.spans = parseInline (t.substring (4)); blocks.push_back (b); continue; }
        if (t.startsWith ("## ")) { flushPara(); Block b; b.kind = Block::H2; b.spans = parseInline (t.substring (3)); blocks.push_back (b); continue; }
        if (t.startsWith ("# ")) { flushPara(); Block b; b.kind = Block::H1; b.spans = parseInline (t.substring (2)); blocks.push_back (b); continue; }
        if (t.startsWith ("![")) {
            flushPara(); Block b; b.kind = Block::Image;
            b.caption = t.fromFirstOccurrenceOf ("![", false, false).upToFirstOccurrenceOf ("](", false, false);
            b.image = t.fromFirstOccurrenceOf ("](", false, false).upToLastOccurrenceOf (")", false, false).fromLastOccurrenceOf ("/", false, false);
            blocks.push_back (b); continue;
        }
        if (t.startsWith ("> ")) { flushPara(); Block b; b.kind = Block::Note; b.spans = parseInline (t.substring (2)); blocks.push_back (b); continue; }
        if (t.startsWith ("- ")) { flushPara(); Block b; b.kind = Block::Bullet; b.spans = parseInline (t.substring (2)); blocks.push_back (b); continue; }
        if (t.length() > 2 && juce::CharacterFunctions::isDigit (t[0]) && t.containsChar ('.') && t.upToFirstOccurrenceOf (".", false, false).containsOnly ("0123456789")
            && t.fromFirstOccurrenceOf (".", false, false).startsWithChar (' '))
        { flushPara(); Block b; b.kind = Block::Step; b.number = t.getIntValue(); b.spans = parseInline (t.fromFirstOccurrenceOf (". ", false, false)); blocks.push_back (b); continue; }
        if (t.startsWithChar ('|'))
        {
            flushPara(); Block b; b.kind = Block::Table;
            for (; i < lines.size() && lines[i].trim().startsWithChar ('|'); ++i)
            {
                const auto row = lines[i].trim();
                if (row.removeCharacters ("|-: ").isEmpty()) continue;      // la línea |---|---|
                auto cells = juce::StringArray::fromTokens (row.substring (1, row.endsWithChar ('|') ? row.length() - 1 : row.length()), "|", "");
                std::vector<juce::String> r; for (auto& c : cells) r.push_back (c.trim());
                b.rows.push_back (r);
            }
            --i;
            blocks.push_back (b); continue;
        }
        para << (para.isEmpty() ? "" : " ") << t;
    }
    flushPara();
    return blocks;
}

// ───────── Vista ─────────
class Page : public juce::Component
{
public:
    Page (const Theme& t, std::function<juce::Image (const juce::String&)> img) : theme (t), images (std::move (img)) {}

    void setBlocks (std::vector<Block> b) { blocks = std::move (b); layoutFor (getWidth()); }
    int layoutFor (int width)
    {
        if (width <= 0) return 0;
        const float W = std::min ((float) (width - 2 * pad), maxText);
        float y = (float) pad;
        tops.clear();
        for (auto& b : blocks)
        {
            tops.push_back (y);
            y += blockHeight (b, W) + gapAfter (b);
        }
        total = (int) y + pad;
        setSize (width, total);
        return total;
    }
    float topOf (size_t i) const { return i < tops.size() ? tops[i] : 0.0f; }
    const std::vector<Block>& getBlocks() const { return blocks; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme.bg);
        const float W = std::min ((float) (getWidth() - 2 * pad), maxText), x0 = (float) pad;
        const auto clip = g.getClipBounds().toFloat();
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            const auto& b = blocks[i];
            const float h = blockHeight (b, W), y = tops[i];
            if (y > clip.getBottom() || y + h < clip.getY()) continue;
            juce::Rectangle<float> r (x0, y, W, h);
            switch (b.kind)
            {
                case Block::Image:
                {
                    auto im = images (b.image);
                    if (im.isValid())
                    {
                        const float iw = std::min (W, (float) im.getWidth()), ih = iw * (float) im.getHeight() / (float) im.getWidth();
                        g.drawImage (im, juce::Rectangle<float> (x0, y, iw, ih), juce::RectanglePlacement::stretchToFit);
                        g.setColour (theme.line); g.drawRect (juce::Rectangle<float> (x0, y, iw, ih), 1.0f);
                        layout (b.caption, 13.0f, theme.muted, W).draw (g, { x0, y + ih + 6.0f, W, 40.0f });
                    }
                    break;
                }
                case Block::Table: drawTable (g, b, x0, y, W); break;
                case Block::Note:
                    g.setColour (theme.panel); g.fillRoundedRectangle (r, 6.0f);
                    g.setColour (theme.accent); g.fillRect (r.withWidth (3.0f));
                    rich (b, 15.0f, theme.text, W - 28.0f).draw (g, r.reduced (14.0f, 10.0f).withTrimmedLeft (4.0f));
                    break;
                case Block::Bullet:
                    g.setColour (theme.accent); g.fillEllipse (x0 + 4.0f, y + 9.0f, 5.0f, 5.0f);
                    rich (b, 16.0f, theme.text, W - 22.0f).draw (g, r.withTrimmedLeft (22.0f));
                    break;
                case Block::Step:
                    g.setColour (theme.accent); g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
                    g.drawText (juce::String (b.number) + ".", juce::Rectangle<float> (x0, y, 24.0f, 22.0f), juce::Justification::topLeft);
                    rich (b, 16.0f, theme.text, W - 28.0f).draw (g, r.withTrimmedLeft (28.0f));
                    break;
                default:
                    rich (b, fontSize (b), b.kind == Block::Para ? theme.text : (b.kind == Block::H3 ? theme.accent : theme.text), W).draw (g, r);
                    if (b.kind == Block::H2) { g.setColour (theme.line); g.fillRect (x0, y - 14.0f, W, 1.0f); }
                    break;
            }
        }
    }

private:
    static constexpr int pad = 36;
    static constexpr float maxText = 760.0f;
    Theme theme;
    std::function<juce::Image (const juce::String&)> images;
    std::vector<Block> blocks;
    std::vector<float> tops;
    int total = 0;

    static float fontSize (const Block& b) { return b.kind == Block::H1 ? 34.0f : b.kind == Block::H2 ? 25.0f : b.kind == Block::H3 ? 18.0f : 16.0f; }
    static float gapAfter (const Block& b)
    {
        switch (b.kind) { case Block::H1: return 18; case Block::H2: return 12; case Block::H3: return 6; case Block::Bullet: case Block::Step: return 6; default: return 16; }
    }
    juce::TextLayout rich (const Block& b, float size, juce::Colour col, float w) const
    {
        juce::AttributedString as;
        const bool head = b.kind == Block::H1 || b.kind == Block::H2 || b.kind == Block::H3;
        for (auto& s : b.spans)
        {
            juce::Font f = s.code ? juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size * 0.92f, juce::Font::plain))
                                  : juce::Font (juce::FontOptions (size, (s.bold || head) ? juce::Font::bold : juce::Font::plain));
            as.append (s.text, f, s.code ? theme.accent : col);
        }
        as.setLineSpacing (head ? 2.0f : size * 0.45f);
        juce::TextLayout tl; tl.createLayout (as, w);
        return tl;
    }
    juce::TextLayout layout (const juce::String& text, float size, juce::Colour col, float w) const
    {
        juce::AttributedString as; as.append (text, juce::Font (juce::FontOptions (size)), col); as.setLineSpacing (3.0f);
        juce::TextLayout tl; tl.createLayout (as, w); return tl;
    }
    std::vector<float> columnWidths (const Block& b, float W) const
    {
        size_t nc = 0; for (auto& r : b.rows) nc = std::max (nc, r.size());
        std::vector<float> len (nc, 1.0f);
        for (auto& r : b.rows) for (size_t c = 0; c < r.size(); ++c) len[c] = std::max (len[c], std::min (60.0f, (float) r[c].length()));
        float sum = 0; for (float v : len) sum += v;
        for (auto& v : len) v = W * v / sum;
        return len;
    }
    float rowHeight (const std::vector<juce::String>& r, const std::vector<float>& cw, bool head) const
    {
        float h = 0;
        for (size_t c = 0; c < r.size() && c < cw.size(); ++c)
        {
            Block tb; tb.spans = parseInline (r[c]); if (head) for (auto& s : tb.spans) s.bold = true;
            h = std::max (h, rich (tb, 14.0f, theme.text, cw[c] - 16.0f).getHeight());
        }
        return h + 14.0f;
    }
    void drawTable (juce::Graphics& g, const Block& b, float x0, float y, float W) const
    {
        const auto cw = columnWidths (b, W);
        float yy = y;
        for (size_t ri = 0; ri < b.rows.size(); ++ri)
        {
            const auto& r = b.rows[ri];
            const float rh = rowHeight (r, cw, ri == 0);
            if (ri == 0) { g.setColour (theme.panel); g.fillRect (x0, yy, W, rh); }
            float xx = x0;
            for (size_t c = 0; c < r.size() && c < cw.size(); ++c)
            {
                Block tb; tb.spans = parseInline (r[c]); if (ri == 0) for (auto& s : tb.spans) s.bold = true;
                rich (tb, 14.0f, ri == 0 ? theme.text : theme.text.withAlpha (0.92f), cw[c] - 16.0f).draw (g, { xx + 8.0f, yy + 7.0f, cw[c] - 16.0f, rh });
                xx += cw[c];
            }
            g.setColour (theme.line); g.fillRect (x0, yy + rh - 1.0f, W, 1.0f);
            yy += rh;
        }
    }
    float blockHeight (const Block& b, float W) const
    {
        switch (b.kind)
        {
            case Block::Image:
            {
                auto im = images (b.image);
                if (! im.isValid()) return 0.0f;
                const float iw = std::min (W, (float) im.getWidth());
                return iw * (float) im.getHeight() / (float) im.getWidth() + 10.0f + layout (b.caption, 13.0f, theme.muted, W).getHeight();
            }
            case Block::Table: { const auto cw = columnWidths (b, W); float h = 0; for (size_t i = 0; i < b.rows.size(); ++i) h += rowHeight (b.rows[i], cw, i == 0); return h; }
            case Block::Note: return rich (b, 15.0f, theme.text, W - 28.0f).getHeight() + 20.0f;
            case Block::Bullet: return rich (b, 16.0f, theme.text, W - 22.0f).getHeight();
            case Block::Step: return rich (b, 16.0f, theme.text, W - 28.0f).getHeight();
            case Block::H2: return rich (b, fontSize (b), theme.text, W).getHeight() + 14.0f;
            default: return rich (b, fontSize (b), theme.text, W).getHeight();
        }
    }
};

class ManualView : public juce::Component
{
public:
    // texts: {"en": markdown, "es": markdown}; images: nombre de archivo → imagen.
    ManualView (std::map<juce::String, juce::String> texts, std::function<juce::Image (const juce::String&)> images, Theme t = {})
        : theme (t), md (std::move (texts)), page (t, std::move (images))
    {
        viewport.setViewedComponent (&page, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
        for (auto* b : { &enBtn, &esBtn, &closeBtn })
        {
            addAndMakeVisible (*b);
            b->setColour (juce::TextButton::buttonColourId, theme.panel);
            b->setColour (juce::TextButton::buttonOnColourId, theme.accent);
            b->setColour (juce::TextButton::textColourOffId, theme.text);
            b->setColour (juce::TextButton::textColourOnId, theme.bg);
        }
        enBtn.onClick = [this] { setLanguage ("en"); };
        esBtn.onClick = [this] { setLanguage ("es"); };
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        setWantsKeyboardFocus (true);
        setLanguage (juce::SystemStats::getUserLanguage().startsWithIgnoreCase ("es") && md.count ("es") ? "es" : "en");
    }

    std::function<void()> onClose;

    void setLanguage (const juce::String& l)
    {
        lang = md.count (l) ? l : "en";
        enBtn.setToggleState (lang == "en", juce::dontSendNotification);
        esBtn.setToggleState (lang == "es", juce::dontSendNotification);
        page.setBlocks (parse (md[lang]));
        toc.clear();
        const auto& bl = page.getBlocks();
        for (size_t i = 0; i < bl.size(); ++i)
            if (bl[i].kind == Block::H2 || bl[i].kind == Block::H1)
            {
                juce::String s; for (auto& sp : bl[i].spans) s << sp.text;
                toc.push_back ({ s, i, bl[i].kind == Block::H1 });
            }
        resized(); viewport.setViewPosition (0, 0); repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme.bg.withAlpha (0.985f));
        auto side = getLocalBounds().removeFromLeft (sideW);
        g.setColour (theme.panel); g.fillRect (side);
        const int cur = currentSection();
        int y = 64;
        for (size_t i = 0; i < toc.size(); ++i)
        {
            const bool on = (int) i == cur;
            g.setColour (on ? theme.accent : (toc[i].title ? theme.text : theme.muted));
            g.setFont (juce::FontOptions (toc[i].title ? 16.0f : 14.0f, toc[i].title || on ? juce::Font::bold : juce::Font::plain));
            g.drawFittedText (toc[i].text, juce::Rectangle<int> (18, y, sideW - 30, 26), juce::Justification::centredLeft, 1);
            tocHits[i] = { 0, y - 2, sideW, 28 };
            y += toc[i].title ? 36 : 28;
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto side = r.removeFromLeft (sideW);
        closeBtn.setBounds (r.getRight() - 46, 12, 34, 30);
        esBtn.setBounds (closeBtn.getX() - 52, 12, 46, 30);
        enBtn.setBounds (esBtn.getX() - 50, 12, 46, 30);
        viewport.setBounds (r.withTrimmedTop (0));
        page.layoutFor (viewport.getMaximumVisibleWidth());
        tocHits.assign (toc.size(), {});
        (void) side;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (size_t i = 0; i < tocHits.size(); ++i)
            if (tocHits[i].contains (e.getPosition()))
            {
                viewport.setViewPosition (0, std::max (0, (int) page.topOf (toc[i].block) - 24));
                repaint(); return;
            }
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
        if (k == juce::KeyPress::pageDownKey || k == juce::KeyPress::spaceKey) { viewport.setViewPosition (0, viewport.getViewPositionY() + viewport.getHeight() - 60); return true; }
        if (k == juce::KeyPress::pageUpKey) { viewport.setViewPosition (0, viewport.getViewPositionY() - viewport.getHeight() + 60); return true; }
        return false;
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { viewport.mouseWheelMove (e.getEventRelativeTo (&viewport), w); repaint(); }

private:
    struct TocEntry { juce::String text; size_t block; bool title; };
    static constexpr int sideW = 230;
    Theme theme;
    std::map<juce::String, juce::String> md;
    juce::String lang = "en";
    juce::Viewport viewport;
    Page page;
    juce::TextButton enBtn { "EN" }, esBtn { "ES" }, closeBtn { juce::String::fromUTF8 ("×") };
    std::vector<TocEntry> toc;
    std::vector<juce::Rectangle<int>> tocHits;

    int currentSection() const
    {
        const int y = viewport.getViewPositionY() + 40;
        int cur = 0;
        for (size_t i = 0; i < toc.size(); ++i) if ((int) page.topOf (toc[i].block) <= y) cur = (int) i;
        return cur;
    }
};

#if DH_HAS_MANUAL
// El manual embebido en el plugin (dh_add_manual): textos y capturas desde ManualData.
inline std::unique_ptr<ManualView> makeEmbedded (Theme t = {})
{
    std::map<juce::String, juce::String> texts;
    auto find = [] (const juce::String& file) -> std::pair<const char*, int> {
        for (int i = 0; i < ManualData::namedResourceListSize; ++i)
            if (juce::String (ManualData::originalFilenames[i]) == file)
            {
                int size = 0;
                const char* d = ManualData::getNamedResource (ManualData::namedResourceList[i], size);
                return { d, size };
            }
        return { nullptr, 0 };
    };
    for (auto l : { "en", "es" })
        if (auto r = find (juce::String (l) + ".md"); r.first != nullptr) texts[l] = juce::String::fromUTF8 (r.first, r.second);
    auto cache = std::make_shared<std::map<juce::String, juce::Image>>();
    auto images = [find, cache] (const juce::String& name) -> juce::Image {
        if (auto it = cache->find (name); it != cache->end()) return it->second;
        juce::Image im;
        if (auto r = find (name); r.first != nullptr) im = juce::ImageCache::getFromMemory (r.first, r.second);
        (*cache)[name] = im;
        return im;
    };
    return std::make_unique<ManualView> (texts, images, t);
}
#endif

// Abre y cierra el manual encima de un editor: el editor llama a toggle() desde su botón Manual y a layout()
// desde resized(). Sin manual embebido (DH_HAS_MANUAL=0) no hace nada.
class ManualHost
{
public:
    bool isOpen() const { return view != nullptr; }
    void toggle (juce::Component& parent, const Theme& t) { if (view != nullptr) close(); else open (parent, t); }
    void open (juce::Component& parent, const Theme& t)
    {
       #if DH_HAS_MANUAL
        if (view != nullptr) return;
        auto v = makeEmbedded (t);
        v->onClose = [this] { juce::MessageManager::callAsync ([this, alive = std::weak_ptr<bool> (token)] { if (! alive.expired()) close(); }); };
        parent.addAndMakeVisible (*v);
        v->setBounds (parent.getLocalBounds());
        v->grabKeyboardFocus();
        view = std::move (v);
        if (onChange) onChange (true);
       #else
        juce::ignoreUnused (parent, t);
       #endif
    }
    void close() { view.reset(); if (onChange) onChange (false); }
    void layout (juce::Rectangle<int> r) { if (view != nullptr) view->setBounds (r); }
    std::function<void (bool)> onChange;
    juce::Component* component() const { return view.get(); }
    static constexpr bool available() { return DH_HAS_MANUAL != 0; }

private:
    std::unique_ptr<ManualView> view;
    std::shared_ptr<bool> token = std::make_shared<bool> (true);
};

} // namespace dh::manual

JUCE_END_IGNORE_WARNINGS_GCC_LIKE
