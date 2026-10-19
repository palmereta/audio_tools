#pragma once
// Páginas del editor. Cada una arma sus controles en resized() y responde a refresh() cuando cambia la
// voz (preset nuevo, sysex importado, edición desde otra página o desde el frente).

#include "Graphs.h"
#include "Fields.h"

namespace dh::fs::ui {

class Page : public juce::Component
{
public:
    explicit Page (ATFS1Processor& p) : proc (p) {}
    virtual void refresh() { for (auto& f : fields) f->repaint(); repaint(); }
    virtual void selectOp (int) {}
    std::vector<NumField*> cursorFields() { std::vector<NumField*> v; for (auto& f : fields) if (f->isVisible() && f->isActive()) v.push_back (f.get()); return v; }
protected:
    ATFS1Processor& proc;
    std::vector<std::unique_ptr<NumField>> fields;
    NumField* addField (VField f, bool compact = false) { fields.push_back (std::make_unique<NumField> (proc, std::move (f), compact)); addAndMakeVisible (*fields.back()); return fields.back().get(); }
    std::vector<std::unique_ptr<juce::Component>> owned;
    template <typename T, typename... A> T* make (A&&... a) { auto c = std::make_unique<T> (std::forward<A> (a)...); auto* p = c.get(); owned.push_back (std::move (c)); addAndMakeVisible (p); return p; }
    void paintSections (juce::Graphics& g, std::initializer_list<std::pair<juce::Rectangle<int>, juce::String>> s) { for (auto& [r, t] : s) section (g, r, t); }
};

// ───────── PLAY ─────────
class PlayPage : public Page, private juce::ListBoxModel, private juce::Timer
{
public:
    std::function<void()> onOpenFseq;
    explicit PlayPage (ATFS1Processor& p) : Page (p), algo (p), scope (p)
    {
        addAndMakeVisible (algo); addAndMakeVisible (scope);
        algo.onSelect = [] (int) {};
        addAndMakeVisible (list); list.setModel (this); list.setRowHeight (26);
        list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (search); search.setTextToShowWhenEmpty ("Search presets", col::faint);
        search.onTextChange = [this] { rebuild(); };
        for (auto* b : { &saveBtn, &prevBtn, &nextBtn }) addAndMakeVisible (b);
        saveBtn.onClick = [this] { save(); };
        prevBtn.onClick = [this] { step (-1); };
        nextBtn.onClick = [this] { step (1); };
        int i = 0;
        for (auto c : { "All", "Vo", "Pd", "Ld", "Ky", "Bs", "Fx", "Sq", "User" })
        {
            auto* b = make<juce::TextButton> (c); b->setClickingTogglesState (false);
            b->onClick = [this, c] { filter = c; for (auto* x : chips) x->setToggleState (x->getButtonText() == filter, juce::dontSendNotification); rebuild(); };
            b->setToggleState (i++ == 0, juce::dontSendNotification);
            chips.push_back (b);
        }
        fseqView = make<FseqView> (p);
        for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                 { "vn_balance", "V/N" }, { "formant", "Formant" }, { "fm", "FM" }, { "cutoff", "Cutoff" },
                 { "resonance", "Resonance" }, { "attack", "Attack" }, { "release", "Release" }, { "fseq_speed", "Fseq speed" } })
            knobs.push_back (make<ParamKnob> (p, id, lab));
        fseqOn = make<ParamBox> (p, "fseq", "Formant Sequence");
        fseqLib = make<juce::ComboBox>();
        fseqLib->onChange = [this] { const int li = fseqLib->getSelectedId() - 1; if (li >= 0 && li < (int) proc.fseqLibrary().size()) { proc.setFseqFromLibrary (li); proc.setValue ("fseq", 1.0f); } };
        rebuild();
        startTimerHz (10);
    }
    void refresh() override
    {
        Page::refresh(); algo.repaint(); fseqView->repaint();
        fseqLib->clear (juce::dontSendNotification);
        int id = 1;
        for (auto& f : proc.fseqLibrary()) fseqLib->addItem (f.name, id++);
        const int li = proc.libraryIndexOf (proc.fseqRef());
        if (li >= 0) fseqLib->setSelectedId (li + 1, juce::dontSendNotification);
        else fseqLib->setText (proc.fseq().empty() ? "No Fseq" : juce::String (proc.fseq().name) + " (own)", juce::dontSendNotification);
        rebuild();
    }
    void paint (juce::Graphics& g) override
    {
        paintSections (g, { { leftR, "Presets" }, { midR, {} }, { rightR, "Perform" } });
        // Tarjetas de operadores bajo el algoritmo.
        const auto& v = proc.voice();
        for (int k = 0; k < 8; ++k)
        {
            const auto r = cards[(size_t) k].toFloat();
            const auto& o = v.op[(size_t) k];
            const bool used = o.level > 0 || o.fseq;
            g.setColour (col::raised.withAlpha (used ? 1.0f : 0.5f)); g.fillRoundedRectangle (r, 5.0f);
            g.setColour (col::border); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
            g.setColour (used ? col::text : col::faint); g.setFont (font (13.0f, true));
            g.drawText (juce::String (k + 1) + "  " + formNames()[o.form & 7], r.reduced (8, 4).withHeight (18).toNearestInt(), juce::Justification::centredLeft);
            g.setColour (used ? col::green : col::faint); g.setFont (font (12.0f));
            g.drawText (o.fseq ? "Fseq Tr" + juce::String (o.fseqTrack + 1) : ratioText (o), r.reduced (8, 4).withTrimmedTop (20).withHeight (16).toNearestInt(), juce::Justification::centredLeft);
            // Nivel en vivo con voz / sin voz.
            const float mv = meters[(size_t) k], mn = meters[(size_t) k + 8];
            auto bar = [&] (float m, float y, juce::Colour c) {
                const float w = (r.getWidth() - 16) * juce::jlimit (0.0f, 1.0f, 1.0f + 20.0f * std::log10 (std::max (1e-6f, m)) / 60.0f);
                g.setColour (col::bg); g.fillRect (r.getX() + 8, y, r.getWidth() - 16, 4.0f);
                g.setColour (c); g.fillRect (r.getX() + 8, y, w, 4.0f);
            };
            bar (mv, r.getBottom() - 18, col::green);
            bar (mn, r.getBottom() - 10, col::dim);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        leftR = r.removeFromLeft (360); r.removeFromLeft (12);
        rightR = r.removeFromRight (420); r.removeFromRight (12);
        midR = r;
        auto l = leftR.withTrimmedTop (28).reduced (10, 6);
        search.setBounds (l.removeFromTop (28)); l.removeFromTop (8);
        auto chipsRow = l.removeFromTop (54);
        gridLayout (chipsRow, { chips.begin(), chips.end() }, 5, 23, 4);
        l.removeFromTop (6);
        auto bottom = l.removeFromBottom (32);
        prevBtn.setBounds (bottom.removeFromLeft (44)); bottom.removeFromLeft (6);
        nextBtn.setBounds (bottom.removeFromLeft (44)); bottom.removeFromLeft (6);
        saveBtn.setBounds (bottom);
        l.removeFromBottom (8);
        list.setBounds (l);
        auto m = midR;
        auto cardRow = m.removeFromBottom (74);
        m.removeFromBottom (10);
        algo.setBounds (m.removeFromTop (m.getHeight() * 54 / 100)); m.removeFromTop (10);
        scope.setBounds (m);
        const int cw = (cardRow.getWidth() - 7 * 8) / 8;
        for (int k = 0; k < 8; ++k) cards[(size_t) k] = { cardRow.getX() + k * (cw + 8), cardRow.getY(), cw, cardRow.getHeight() };
        auto rr = rightR.withTrimmedTop (28).reduced (10, 6);
        fseqView->setBounds (rr.removeFromTop (240)); rr.removeFromTop (8);
        auto row = rr.removeFromTop (44);
        fseqOn->setBounds (row.removeFromLeft (150)); row.removeFromLeft (8);
        fseqLib->setBounds (row.withTrimmedTop (16));
        rr.removeFromTop (12);
        gridLayout (rr, { knobs.begin(), knobs.end() }, 4, 92, 6);
    }
    void selectedPreset (int i) { list.selectRow (rowOf (i), true, true); }

private:
    AlgoView algo;
    ScopeView scope;
    juce::ListBox list;
    juce::TextEditor search;
    juce::TextButton saveBtn { "Save as..." }, prevBtn { "<" }, nextBtn { ">" };
    std::vector<juce::TextButton*> chips;
    std::vector<ParamKnob*> knobs;
    FseqView* fseqView = nullptr;
    ParamBox* fseqOn = nullptr;
    juce::ComboBox* fseqLib = nullptr;
    juce::Rectangle<int> leftR, midR, rightR;
    std::array<juce::Rectangle<int>, 8> cards {};
    std::array<float, 16> meters {};
    juce::String filter { "All" };
    std::vector<int> rows;

    void rebuild()
    {
        rows.clear();
        const auto& ps = proc.getPresets();
        const auto q = search.getText().trim();
        for (int i = 0; i < (int) ps.size(); ++i)
        {
            const auto& e = ps[(size_t) i];
            if (filter == "User" ? e.factory : (filter != "All" && e.category != filter)) continue;
            if (q.isNotEmpty() && ! e.name.containsIgnoreCase (q) && ! e.category.containsIgnoreCase (q)) continue;
            rows.push_back (i);
        }
        list.updateContent();
        const int r = rowOf (proc.getCurrentProgram());
        if (r >= 0) list.selectRow (r, true, true); else list.deselectAllRows();
        list.repaint();
    }
    int rowOf (int preset) const { for (size_t i = 0; i < rows.size(); ++i) if (rows[i] == preset) return (int) i; return -1; }
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (row < 0 || row >= (int) rows.size()) return;
        const auto& e = proc.getPresets()[(size_t) rows[(size_t) row]];
        if (sel) { g.setColour (col::blue.withAlpha (0.55f)); g.fillRoundedRectangle (0.0f, 1.0f, (float) w, (float) h - 2.0f, 4.0f); }
        g.setColour (col::dim); g.setFont (font (11.5f));
        g.drawText (juce::String (rows[(size_t) row] + 1).paddedLeft ('0', 3), 6, 0, 30, h, juce::Justification::centredLeft);
        g.setColour (sel ? juce::Colours::white : col::text); g.setFont (font (14.0f));
        g.drawText (e.name, 40, 0, w - 90, h, juce::Justification::centredLeft);
        g.setColour (col::faint); g.setFont (font (11.5f));
        g.drawText (e.category, w - 48, 0, 42, h, juce::Justification::centredRight);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override { if (row >= 0 && row < (int) rows.size()) proc.loadPreset (rows[(size_t) row]); }
    void step (int d)
    {
        if (rows.empty()) return;
        int r = rowOf (proc.getCurrentProgram());
        r = r < 0 ? 0 : (r + d + (int) rows.size()) % (int) rows.size();
        proc.loadPreset (rows[(size_t) r]);
    }
    void save()
    {
        auto* w = new juce::AlertWindow ("Save preset", "Name of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.currentPresetName);
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) {
            if (r == 1 && w->getTextEditorContents ("name").trim().isNotEmpty()) proc.saveUserPreset (w->getTextEditorContents ("name").trim());
            rebuild();
        }), true);
    }
    void timerCallback() override
    {
        bool ch = false;
        for (int k = 0; k < 16; ++k) { const float m = proc.live.opMeter[(size_t) k].load(); if (std::abs (m - meters[(size_t) k]) > 0.003f) { meters[(size_t) k] = m; ch = true; } }
        if (ch) for (auto& c : cards) repaint (c);
    }
};

// ───────── VOICE: algoritmo y matriz de operadores ─────────
class VoicePage : public Page
{
public:
    std::function<void (int, bool)> onOpenOp;
    explicit VoicePage (ATFS1Processor& p) : Page (p), algo (p)
    {
        addAndMakeVisible (algo);
        algo.onSelect = [this] (int k) { selectOp (k); };
        addAndMakeVisible (pickBtn); pickBtn.onClick = [this] { pick(); };
        addAndMakeVisible (name); name.setInputRestrictions (10);
        name.onTextChange = [this] { const auto s = name.getText(); proc.editVoiceWith ([&] (VoiceData& v) { v.name = s.toStdString(); }); };
        algF = addField (withFmt (VField { "Algorithm", 0, 87, 0, [] (const VoiceData& v) { return v.algorithm; }, [] (VoiceData& v, int x) { v.algorithm = x; }, {}, nullptr,
                                           "one of the 88 ways the 8 voiced operators modulate each other (the unvoiced ones always go straight to the output)", nullptr },
                                  [] (int x, const VoiceData&) { return juce::String (x + 1); }));
        addField (VField { "Feedback", 0, 7, 0, [] (const VoiceData& v) { return v.feedback; }, [] (VoiceData& v, int x) { v.feedback = x; }, {}, nullptr, "the algorithm's feedback loop, 0.5 x 2^(fb - 7) of the output back into the phase", nullptr });
        addField (VField { "Note shift", -24, 24, 0, [] (const VoiceData& v) { return v.noteShift; }, [] (VoiceData& v, int x) { v.noteShift = x; }, {}, nullptr, "semitones", nullptr });
        addField (VField { "Category", 0, 22, 0, [] (const VoiceData& v) { return v.category; }, [] (VoiceData& v, int x) { v.category = x; }, categories(), nullptr, "", nullptr });
        nCommon = (int) fields.size();
        static const char* VH[] = { "Form", "Mode", "Coarse", "Fine", "Detune", "Transp", "Skirt", "BW/Res", "Level", "Fseq", "Track" };
        static const char* NH[] = { "Mode", "Coarse", "Fine", "Transp", "Band", "Reso", "Skirt", "Level", "Fseq" };
        juce::ignoreUnused (VH, NH);
        for (int k = 0; k < 8; ++k)
        {
            const auto osc = voicedOscFields (k);
            for (int idx : { 0, 1, 2, 3, 4, 5, 8, 9 }) addField (osc[(size_t) idx], true);
            addField (voicedLevelFields (k)[0], true);
            addField (osc[11], true); addField (osc[12], true);
        }
        nVoiced = (int) fields.size();
        for (int k = 0; k < 8; ++k)
        {
            const auto u = unvoicedFields (k);
            for (int idx : { 0, 1, 2, 3, 5, 7, 8, 9, 16 }) addField (u[(size_t) idx], true);
        }
    }
    void refresh() override { Page::refresh(); algo.repaint(); if (! name.hasKeyboardFocus (true)) name.setText (juce::String (proc.voice().name), false); }
    void selectOp (int k) override { algo.selected = k; repaint(); algo.repaint(); }
    void paint (juce::Graphics& g) override
    {
        paintSections (g, { { leftR, "Voice" }, { vR, "Voiced operators" }, { nR, "Unvoiced operators (noise formants)" } });
        static const char* VH[] = { "Form", "Mode", "Coarse", "Fine", "Detune", "Transp", "Skirt", "BW/Res", "Level", "Fseq", "Track" };
        static const char* NH[] = { "Mode", "Freq", "Fine", "Transp", "Band", "Reso", "Skirt", "Level", "Fseq" };
        g.setFont (font (11.5f)); g.setColour (col::dim);
        for (int c = 0; c < 11; ++c) g.drawText (VH[c], vCols[(size_t) c].withY (vHeadY).withHeight (16), juce::Justification::centred);
        for (int c = 0; c < 9; ++c) g.drawText (NH[c], nCols[(size_t) c].withY (nHeadY).withHeight (16), juce::Justification::centred);
        for (int k = 0; k < 8; ++k)
        {
            const bool sel = k == algo.selected;
            g.setColour (sel ? col::green : col::text); g.setFont (font (14.0f, true));
            g.drawText (juce::String (k + 1), opLabelV[(size_t) k], juce::Justification::centred);
            g.drawText (juce::String (k + 1), opLabelN[(size_t) k], juce::Justification::centred);
        }
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int k = 0; k < 8; ++k)
        {
            if (opLabelV[(size_t) k].contains (e.getPosition())) { if (onOpenOp) onOpenOp (k, false); return; }
            if (opLabelN[(size_t) k].contains (e.getPosition())) { if (onOpenOp) onOpenOp (k, true); return; }
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        leftR = r.removeFromLeft (520); r.removeFromLeft (12);
        vR = r.removeFromTop (r.getHeight() * 52 / 100); r.removeFromTop (12); nR = r;
        auto l = leftR.withTrimmedTop (28).reduced (10, 6);
        auto top = l.removeFromTop (40);
        name.setBounds (top.removeFromLeft (180).withTrimmedTop (12)); top.removeFromLeft (10);
        pickBtn.setBounds (top.removeFromRight (150).withTrimmedTop (10));
        l.removeFromTop (8);
        auto fr = l.removeFromBottom (40);
        gridLayout (fr, { fields[0].get(), fields[1].get(), fields[2].get(), fields[3].get() }, 4, 40, 8);
        l.removeFromBottom (10);
        algo.setBounds (l);
        // Matriz con voz.
        auto v = vR.withTrimmedTop (28).reduced (10, 2);
        vHeadY = v.getY(); v.removeFromTop (18);
        const int rowH = (v.getHeight() - 7 * 3) / 8;
        const int labW = 30;
        const int cw = (v.getWidth() - labW - 10 * 4) / 11;
        for (int c = 0; c < 11; ++c) vCols[(size_t) c] = { v.getX() + labW + c * (cw + 4), 0, cw, 0 };
        for (int k = 0; k < 8; ++k)
        {
            const int y = v.getY() + k * (rowH + 3);
            opLabelV[(size_t) k] = { v.getX(), y, labW, rowH };
            for (int c = 0; c < 11; ++c) fields[(size_t) (nCommon + k * 11 + c)]->setBounds (vCols[(size_t) c].withY (y).withHeight (rowH));
        }
        auto n = nR.withTrimmedTop (28).reduced (10, 2);
        nHeadY = n.getY(); n.removeFromTop (18);
        const int rowN = (n.getHeight() - 7 * 3) / 8;
        const int cn = (n.getWidth() - labW - 8 * 4) / 9;
        for (int c = 0; c < 9; ++c) nCols[(size_t) c] = { n.getX() + labW + c * (cn + 4), 0, cn, 0 };
        for (int k = 0; k < 8; ++k)
        {
            const int y = n.getY() + k * (rowN + 3);
            opLabelN[(size_t) k] = { n.getX(), y, labW, rowN };
            for (int c = 0; c < 9; ++c) fields[(size_t) (nVoiced + k * 9 + c)]->setBounds (nCols[(size_t) c].withY (y).withHeight (rowN));
        }
    }
private:
    AlgoView algo;
    juce::TextButton pickBtn { "Choose algorithm..." };
    juce::TextEditor name;
    NumField* algF = nullptr;
    int nCommon = 0, nVoiced = 0, vHeadY = 0, nHeadY = 0;
    juce::Rectangle<int> leftR, vR, nR;
    std::array<juce::Rectangle<int>, 11> vCols {};
    std::array<juce::Rectangle<int>, 9> nCols {};
    std::array<juce::Rectangle<int>, 8> opLabelV {}, opLabelN {};
    void pick()
    {
        auto picker = std::make_unique<AlgoPicker> (proc.voice().algorithm);
        auto* raw = picker.get();
        auto& box = juce::CallOutBox::launchAsynchronously (std::move (picker), pickBtn.getScreenBounds(), nullptr);
        juce::Component::SafePointer<juce::CallOutBox> sb (&box);
        raw->onPick = [this, sb] (int a) { proc.editVoiceWith ([a] (VoiceData& v) { v.algorithm = a; }); showLcd ("Algorithm", juce::String (a + 1)); if (sb != nullptr) sb->dismiss(); };
    }
};

// ───────── OPERATOR: detalle de un operador ─────────
class OperatorPage : public Page
{
public:
    explicit OperatorPage (ATFS1Processor& p) : Page (p), spectrum (p), amp (p, ampEgModel (0, false)), freq (p, freqEgModel (0, false))
    {
        addAndMakeVisible (spectrum); addAndMakeVisible (amp); addAndMakeVisible (freq);
        for (int k = 0; k < 8; ++k)
        {
            auto* b = make<juce::TextButton> (juce::String (k + 1));
            b->onClick = [this, k] { selectOp (k); };
            opBtns.push_back (b);
        }
        vBtn = make<juce::TextButton> ("Voiced"); nBtn = make<juce::TextButton> ("Unvoiced");
        vBtn->onClick = [this] { setUnvoiced (false); };
        nBtn->onClick = [this] { setUnvoiced (true); };
        copyBtn = make<juce::TextButton> ("Copy"); pasteBtn = make<juce::TextButton> ("Paste");
        copyBtn->onClick = [this] { clipV = proc.voice().op[(size_t) op]; clipN = proc.voice().nop[(size_t) op]; hasClip = true; showLcd ("Copied", "OP" + juce::String (op + 1)); };
        pasteBtn->onClick = [this] { if (! hasClip) return; proc.editVoiceWith ([this] (VoiceData& v) { if (unvoiced) v.nop[(size_t) op] = clipN; else v.op[(size_t) op] = clipV; }); showLcd ("Pasted", "OP" + juce::String (op + 1)); };
        rebuildFields();
    }
    void selectOp (int k) override { op = juce::jlimit (0, 7, k); rebuildFields(); }
    void setUnvoiced (bool n) { unvoiced = n; rebuildFields(); }
    int currentOp() const { return op; }
    bool isUnvoiced() const { return unvoiced; }
    void refresh() override { Page::refresh(); spectrum.refresh(); amp.repaint(); freq.repaint(); }
    void paint (juce::Graphics& g) override
    {
        paintSections (g, { { leftR, (unvoiced ? "Unvoiced operator " : "Voiced operator ") + juce::String (op + 1) } });
        if (! unvoiced)
        {
            g.setColour (col::dim); g.setFont (font (12.0f, true));
            g.drawText ("Oscillator and shape", oscTitle, juce::Justification::centredLeft);
            g.drawText ("Level, scaling and sensitivity", lvlTitle, juce::Justification::centredLeft);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto bar = r.removeFromTop (34);
        for (auto* b : opBtns) { b->setBounds (bar.removeFromLeft (40)); bar.removeFromLeft (4); }
        bar.removeFromLeft (16);
        vBtn->setBounds (bar.removeFromLeft (90)); bar.removeFromLeft (4); nBtn->setBounds (bar.removeFromLeft (90));
        pasteBtn->setBounds (bar.removeFromRight (70)); bar.removeFromRight (6); copyBtn->setBounds (bar.removeFromRight (70));
        r.removeFromTop (10);
        leftR = r.removeFromLeft (560); r.removeFromLeft (12);
        spectrum.setBounds (r.removeFromTop (r.getHeight() * 40 / 100)); r.removeFromTop (10);
        amp.setBounds (r.removeFromTop (r.getHeight() * 60 / 100)); r.removeFromTop (10);
        freq.setBounds (r);
        layoutFields();
    }
private:
    SpectrumView spectrum;
    EgView amp, freq;
    std::vector<juce::TextButton*> opBtns;
    juce::TextButton *vBtn = nullptr, *nBtn = nullptr, *copyBtn = nullptr, *pasteBtn = nullptr;
    int op = 0; bool unvoiced = false, hasClip = false;
    VoicedOp clipV; UnvoicedOp clipN;
    juce::Rectangle<int> leftR, oscTitle, lvlTitle;
    int nOsc = 0;
    void rebuildFields()
    {
        for (auto& f : fields) removeChildComponent (f.get());
        fields.clear();
        if (! unvoiced) { for (auto& f : voicedOscFields (op)) addField (f); nOsc = (int) fields.size(); for (auto& f : voicedLevelFields (op)) addField (f); }
        else { for (auto& f : unvoicedFields (op)) addField (f); nOsc = (int) fields.size(); }
        for (int k = 0; k < 8; ++k) opBtns[(size_t) k]->setToggleState (k == op, juce::dontSendNotification);
        vBtn->setToggleState (! unvoiced, juce::dontSendNotification); nBtn->setToggleState (unvoiced, juce::dontSendNotification);
        spectrum.op = op; spectrum.unvoiced = unvoiced; spectrum.refresh();
        amp.setModel (ampEgModel (op, unvoiced)); freq.setModel (freqEgModel (op, unvoiced));
        layoutFields();
        repaint();
    }
    void layoutFields()
    {
        if (leftR.isEmpty()) return;
        auto l = leftR.withTrimmedTop (28).reduced (10, 6);
        std::vector<juce::Component*> a, b;
        for (int i = 0; i < (int) fields.size(); ++i) (i < nOsc ? a : b).push_back (fields[(size_t) i].get());
        if (unvoiced) { gridLayout (l, a, 3, 40, 10); return; }
        oscTitle = l.removeFromTop (18);
        const int rowsA = ((int) a.size() + 2) / 3;
        gridLayout (l.removeFromTop (rowsA * 50), a, 3, 40, 10);
        l.removeFromTop (10);
        lvlTitle = l.removeFromTop (18);
        gridLayout (l, b, 3, 40, 10);
    }
};

// ───────── COMMON: altura, LFO, filtro, perillas ─────────
class CommonPage : public Page
{
public:
    explicit CommonPage (ATFS1Processor& p) : Page (p), peg (p, pitchEgModel()), feg (p, filterEgModel())
    {
        addAndMakeVisible (peg); addAndMakeVisible (feg);
        for (auto& f : pitchEgFields()) addField (f);
        nPeg = (int) fields.size();
        for (auto& f : lfo1Fields()) addField (f);
        nL1 = (int) fields.size();
        for (auto& f : lfo2Fields()) addField (f);
        nL2 = (int) fields.size();
        for (auto& f : filterFields()) addField (f);
        nFlt = (int) fields.size();
        for (int s = 0; s < 5; ++s) for (auto& f : knobSlotFields (false, s)) addField (f, true);
        for (int s = 0; s < 5; ++s) for (auto& f : knobSlotFields (true, s)) addField (f, true);
        nKnob = (int) fields.size();
        for (int k = 0; k < 8; ++k)
            addField (VField { "Op" + juce::String (k + 1), 0, 15, 0, [k] (const VoiceData& v) { return v.carrierAtt[(size_t) k]; }, [k] (VoiceData& v, int x) { v.carrierAtt[(size_t) k] = x; }, {},
                               [] (int x, const VoiceData&) { return x == 0 ? juce::String ("0 dB") : "-" + juce::String (x * 1.5f, 1); }, "carrier level correction: lowers an operator when it is a carrier", nullptr }, true);
    }
    void refresh() override { Page::refresh(); peg.repaint(); feg.repaint(); }
    void paint (juce::Graphics& g) override
    {
        paintSections (g, { { pegR, "Pitch envelope" }, { lfoR, "LFO 1 and LFO 2" }, { fltR, "Filter (switch it on in Perform)" }, { knobR, "Formant and FM knobs" } });
        g.setColour (col::dim); g.setFont (font (12.0f, true));
        g.drawText ("LFO 2", lfo2T, juce::Justification::centredLeft);
        g.drawText ("Formant knob", fmtT, juce::Justification::centredLeft);
        g.drawText ("FM knob", fmT, juce::Justification::centredLeft);
        g.drawText ("Carrier level correction", carT, juce::Justification::centredLeft);
        g.setFont (font (11.0f)); g.setColour (col::faint);
        for (int s = 0; s < 5; ++s) { g.drawText (juce::String (s + 1), slotLabF[(size_t) s], juce::Justification::centred); g.drawText (juce::String (s + 1), slotLabM[(size_t) s], juce::Justification::centred); }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto left = r.removeFromLeft (r.getWidth() * 36 / 100); r.removeFromLeft (12);
        auto mid = r.removeFromLeft (r.getWidth() * 50 / 100); r.removeFromLeft (12);
        pegR = left.removeFromTop (left.getHeight() * 46 / 100); left.removeFromTop (12); lfoR = left;
        fltR = mid; knobR = r;
        auto p = pegR.withTrimmedTop (28).reduced (10, 6);
        auto pf = p.removeFromBottom (40); p.removeFromBottom (8);
        peg.setBounds (p);
        gridLayout (pf, sub (0, nPeg), 3, 40, 10);
        auto lf = lfoR.withTrimmedTop (28).reduced (10, 6);
        gridLayout (lf.removeFromTop (2 * 48), sub (nPeg, nL1), 4, 40, 8);
        lf.removeFromTop (6);
        lfo2T = lf.removeFromTop (18);
        gridLayout (lf, sub (nL1, nL2), 4, 40, 8);
        auto fl = fltR.withTrimmedTop (28).reduced (10, 6);
        gridLayout (fl.removeFromTop (3 * 50), sub (nL2, nFlt), 4, 40, 10);
        fl.removeFromTop (6);
        feg.setBounds (fl);
        auto kn = knobR.withTrimmedTop (28).reduced (10, 6);
        auto slots = [&] (int base, juce::Rectangle<int>& title, std::array<juce::Rectangle<int>, 5>& labs) {
            title = kn.removeFromTop (18);
            for (int s = 0; s < 5; ++s)
            {
                auto row = kn.removeFromTop (26); kn.removeFromTop (4);
                labs[(size_t) s] = row.removeFromLeft (18);
                std::vector<juce::Component*> c;
                for (int i = 0; i < 4; ++i) c.push_back (fields[(size_t) (base + s * 4 + i)].get());
                gridLayout (row, c, 4, 26, 4);
            }
            kn.removeFromTop (8);
        };
        slots (nFlt, fmtT, slotLabF);
        slots (nFlt + 20, fmT, slotLabM);
        carT = kn.removeFromTop (18);
        gridLayout (kn.removeFromTop (60), sub (nKnob, (int) fields.size()), 4, 26, 4);
    }
private:
    EgView peg, feg;
    int nPeg = 0, nL1 = 0, nL2 = 0, nFlt = 0, nKnob = 0;
    juce::Rectangle<int> pegR, lfoR, fltR, knobR, lfo2T, fmtT, fmT, carT;
    std::array<juce::Rectangle<int>, 5> slotLabF {}, slotLabM {};
    std::vector<juce::Component*> sub (int a, int b) { std::vector<juce::Component*> v; for (int i = a; i < b; ++i) v.push_back (fields[(size_t) i].get()); return v; }
};

// ───────── FSEQ ─────────
class FseqPage : public Page
{
public:
    explicit FseqPage (ATFS1Processor& p) : Page (p)
    {
        view = make<FseqView> (p);
        lib = make<juce::ComboBox>();
        lib->onChange = [this] { const int i = lib->getSelectedId() - 1; if (i >= 0 && i < (int) proc.fseqLibrary().size()) { proc.setFseqFromLibrary (i); proc.setValue ("fseq", 1.0f); } };
        analyzeBtn = make<juce::TextButton> ("Analyze audio...");
        analyzeBtn->onClick = [this] { chooseAudio(); };
        importBtn = make<juce::TextButton> ("Import .syx...");
        importBtn->onClick = [this] { chooseSyx(); };
        exportBtn = make<juce::TextButton> ("Export .syx...");
        exportBtn->onClick = [this] { exportSyx(); };
        vowels = make<juce::TextEditor>(); vowels->setText ("aeiou", false);
        vowels->setInputRestrictions (24, "aeiouym-");
        for (auto* t : { &pitch, &secs }) { *t = make<juce::TextEditor>(); }
        pitch->setText ("130", false); pitch->setInputRestrictions (4, "0123456789");
        secs->setText ("0.4", false); secs->setInputRestrictions (4, "0123456789.");
        voiceBox = make<juce::ComboBox>(); voiceBox->addItemList ({ "Low voice", "High voice" }, 1); voiceBox->setSelectedId (1);
        breath = make<juce::ComboBox>(); breath->addItemList ({ "No breath", "Breathy", "Whisper" }, 1); breath->setSelectedId (1);
        genBtn = make<juce::TextButton> ("Create Fseq");
        genBtn->onClick = [this] { generate(); };
        for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                 { "fseq", "Fseq" }, { "fseq_smooth", "Interpolation" }, { "fseq_pitch", "Pitch" }, { "fseq_loop", "Loop" },
                 { "fseq_start", "Start frame" }, { "fseq_lp_start", "Loop start" }, { "fseq_lp_end", "Loop end" },
                 { "fseq_vel_speed", "Speed velocity" }, { "fseq_vel_level", "Level velocity" }, { "fseq_delay", "Delay" } })
            boxes.push_back (make<ParamBox> (p, id, lab));
        speed = make<ParamKnob> (p, "fseq_speed", "Speed", "100 % = 126 frames per second, as on the FS1R; up to 1000 % here");
    }
    void refresh() override
    {
        Page::refresh(); view->repaint();
        lib->clear (juce::dontSendNotification);
        int id = 1; for (auto& f : proc.fseqLibrary()) lib->addItem (f.name, id++);
        const int li = proc.libraryIndexOf (proc.fseqRef());
        if (li >= 0) lib->setSelectedId (li + 1, juce::dontSendNotification);
        else lib->setText (proc.fseq().empty() ? "No Fseq" : juce::String (proc.fseq().name) + " (own)", juce::dontSendNotification);
    }
    void paint (juce::Graphics& g) override
    {
        paintSections (g, { { srcR, "Source" }, { genR, "Vowel generator" }, { playR, "Playback" } });
        g.setColour (col::dim); g.setFont (font (11.5f));
        g.drawText ("Vowels (a e i o u y m, - for silence)", vowLab, juce::Justification::centredLeft);
        g.drawText ("Pitch Hz", pitchLab, juce::Justification::centredLeft);
        g.drawText ("Seconds each", secsLab, juce::Justification::centredLeft);
        g.setColour (col::faint);
        g.drawFittedText ("Drop a WAV or AIFF anywhere on this page to turn a recording into an Fseq (pitch by autocorrelation, formants by LPC). The FS1R could not create Fseqs; Yamaha made them in-house.",
                          note, juce::Justification::topLeft, 3);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        view->setBounds (r.removeFromTop (r.getHeight() * 56 / 100)); r.removeFromTop (12);
        srcR = r.removeFromLeft (r.getWidth() * 30 / 100); r.removeFromLeft (12);
        genR = r.removeFromLeft (r.getWidth() * 42 / 100); r.removeFromLeft (12);
        playR = r;
        auto s = srcR.withTrimmedTop (28).reduced (10, 6);
        lib->setBounds (s.removeFromTop (30)); s.removeFromTop (10);
        analyzeBtn->setBounds (s.removeFromTop (30)); s.removeFromTop (8);
        auto row = s.removeFromTop (30);
        importBtn->setBounds (row.removeFromLeft (row.getWidth() / 2 - 4)); row.removeFromLeft (8); exportBtn->setBounds (row);
        s.removeFromTop (10); note = s;
        auto gnr = genR.withTrimmedTop (28).reduced (10, 6);
        vowLab = gnr.removeFromTop (16); vowels->setBounds (gnr.removeFromTop (28)); gnr.removeFromTop (8);
        auto r2 = gnr.removeFromTop (16);
        pitchLab = r2.removeFromLeft (r2.getWidth() / 2); secsLab = r2;
        auto r3 = gnr.removeFromTop (28);
        pitch->setBounds (r3.removeFromLeft (r3.getWidth() / 2 - 4)); r3.removeFromLeft (8); secs->setBounds (r3);
        gnr.removeFromTop (8);
        auto r4 = gnr.removeFromTop (28);
        voiceBox->setBounds (r4.removeFromLeft (r4.getWidth() / 2 - 4)); r4.removeFromLeft (8); breath->setBounds (r4);
        gnr.removeFromTop (10);
        genBtn->setBounds (gnr.removeFromTop (32));
        auto pl = playR.withTrimmedTop (28).reduced (10, 6);
        speed->setBounds (pl.removeFromLeft (90).withHeight (100));
        pl.removeFromLeft (10);
        gridLayout (pl, { boxes.begin(), boxes.end() }, 3, 40, 8);
    }
private:
    FseqView* view = nullptr;
    juce::ComboBox *lib = nullptr, *voiceBox = nullptr, *breath = nullptr;
    juce::TextButton *analyzeBtn = nullptr, *importBtn = nullptr, *exportBtn = nullptr, *genBtn = nullptr;
    juce::TextEditor *vowels = nullptr, *pitch = nullptr, *secs = nullptr;
    std::vector<ParamBox*> boxes;
    ParamKnob* speed = nullptr;
    juce::Rectangle<int> srcR, genR, playR, vowLab, pitchLab, secsLab, note;
    std::unique_ptr<juce::FileChooser> chooser;
    void generate()
    {
        const auto txt = vowels->getText().trim();
        if (txt.isEmpty()) return;
        const float hz = juce::jlimit (50.0f, 600.0f, (float) pitch->getText().getIntValue());
        const float s = juce::jlimit (0.05f, 2.0f, (float) secs->getText().getDoubleValue());
        const bool high = voiceBox->getSelectedId() == 2;
        const float br = breath->getSelectedId() == 1 ? 0.0f : (breath->getSelectedId() == 2 ? 0.3f : 1.0f);
        auto f = vowelSequence (txt.substring (0, 8).toStdString(), txt.toStdString(), s, 0.35f, high ? hz * 1.8f : hz, high ? 1.17f : 1.0f, 0.25f, br);
        if ((int) f.frames.size() > 512) f.frames.resize (512);
        f.loopEnd = std::min (f.loopEnd, (int) f.frames.size() - 1);
        f.loopStart = std::min (f.loopStart, f.loopEnd);
        proc.setFseq (f);
        proc.setValue ("fseq", 1.0f); proc.setValue ("fseq_loop", 0.0f);
        showLcd ("Fseq", juce::String (f.name));
    }
    void chooseAudio()
    {
        chooser = std::make_unique<juce::FileChooser> ("Recording to analyze", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& c) {
            if (c.getResult().existsAsFile()) proc.analyzeAudioAsync (c.getResult());
        });
    }
    void chooseSyx()
    {
        chooser = std::make_unique<juce::FileChooser> ("FS1R sysex (voice or Fseq)", juce::File(), "*.syx;*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& c) {
            if (c.getResult().existsAsFile()) proc.postMessage (proc.importSyx (c.getResult()));
        });
    }
    void exportSyx()
    {
        if (proc.fseq().empty()) { proc.postMessage ("There is no Fseq to export"); return; }
        chooser = std::make_unique<juce::FileChooser> ("Save the Fseq as FS1R sysex", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::String (proc.fseq().name) + ".syx"), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& c) {
            const auto f = c.getResult();
            if (f != juce::File()) proc.postMessage (proc.exportFseqSyx (f.withFileExtension ("syx")) ? "Fseq saved: " + f.getFileName() : "Could not save " + f.getFileName());
        });
    }
};

// ───────── PERFORM ─────────
class PerformPage : public Page
{
public:
    explicit PerformPage (ATFS1Processor& p) : Page (p)
    {
        auto knob = [this, &p] (const char* id, const char* lab, const char* help = "") { return make<ParamKnob> (p, id, lab, help); };
        auto box = [this, &p] (const char* id, const char* lab, const char* help = "") { return make<ParamBox> (p, id, lab, help); };
        groups.push_back ({ "Part", { knob ("volume", "Volume"), knob ("pan", "Pan"), knob ("vn_balance", "V/N balance", "voiced against unvoiced operators"),
                                      knob ("note_shift", "Note shift"), knob ("detune", "Detune") }, {} });
        groups.push_back ({ "Envelope offsets", { knob ("attack", "Attack", "added to every operator's T1"), knob ("decay", "Decay", "added to T2 and T3"), knob ("release", "Release", "added to T4"),
                                                  knob ("formant", "Formant", "the voice's Formant knob matrix"), knob ("fm", "FM", "the voice's FM knob matrix") }, {} });
        groups.push_back ({ "Filter and LFO offsets", { knob ("cutoff", "Cutoff"), knob ("resonance", "Resonance"), knob ("feg_depth", "Filter EG"),
                                                        knob ("lfo1_rate", "LFO1 rate"), knob ("lfo1_pmd", "LFO1 pitch"), knob ("lfo1_delay", "LFO1 delay"),
                                                        knob ("lfo2_rate", "LFO2 rate"), knob ("lfo2_depth", "LFO2 depth") }, {} });
        groups.push_back ({ "Pitch EG offsets", { knob ("peg_init", "Initial"), knob ("peg_attack", "Attack time"), knob ("peg_rel_level", "Release level"), knob ("peg_rel_time", "Release time") }, {} });
        groups.push_back ({ "Voice", { box ("filter", "Filter"), box ("mono", "Mono/Poly"), box ("voices", "Voices"), box ("porta", "Portamento"), box ("porta_mode", "Porta mode"),
                                       knob ("porta_time", "Porta time"), box ("bend_up", "Bend up"), box ("bend_down", "Bend down") }, {} });
        groups.push_back ({ "Velocity and controllers", { knob ("vel_depth", "Vel depth"), knob ("vel_offset", "Vel offset"), box ("mw_dest", "Mod wheel"), knob ("mw_depth", "MW depth"),
                                                          box ("at_dest", "Aftertouch"), knob ("at_depth", "AT depth") }, {} });
    }
    void paint (juce::Graphics& g) override { for (auto& gr : groups) section (g, gr.r, gr.title); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        const int colW = (r.getWidth() - 24) / 3;
        auto c1 = r.removeFromLeft (colW); r.removeFromLeft (12);
        auto c2 = r.removeFromLeft (colW); r.removeFromLeft (12);
        auto c3 = r;
        auto place = [&] (Group& gr, juce::Rectangle<int> area) {
            gr.r = area;
            auto in = area.withTrimmedTop (28).reduced (10, 6);
            std::vector<juce::Component*> ks, bs;
            for (auto* c : gr.items) (dynamic_cast<ParamKnob*> (c) != nullptr ? ks : bs).push_back (c);
            const int bc = bs.size() <= 2 ? 2 : 4;
            if (! bs.empty()) { gridLayout (in.removeFromTop (((int) bs.size() + bc - 1) / bc * 48), bs, bc, 40, 8); in.removeFromTop (6); }
            gridLayout (in, ks, 4, 92, 6);
        };
        place (groups[0], c1.removeFromTop (246)); c1.removeFromTop (12);
        place (groups[1], c1.removeFromTop (246)); c1.removeFromTop (12);
        place (groups[3], c1);
        place (groups[2], c2.removeFromTop (246)); c2.removeFromTop (12);
        place (groups[5], c2);
        place (groups[4], c3);
    }
private:
    struct Group { juce::String title; std::vector<juce::Component*> items; juce::Rectangle<int> r; };
    std::vector<Group> groups;
};

// ───────── EFFECT ─────────
class EffectsPage : public Page
{
public:
    explicit EffectsPage (ATFS1Processor& p) : Page (p)
    {
        auto knob = [this, &p] (const char* id, const char* lab, const char* help = "") { return make<ParamKnob> (p, id, lab, help); };
        auto box = [this, &p] (const char* id, const char* lab) { return make<ParamBox> (p, id, lab); };
        cols.push_back ({ "Insertion", box ("ins_type", "Type"), { knob ("ins_amount", "Amount", "drive, depth or rate, by type"), knob ("ins_mix", "Mix") }, {} });
        cols.push_back ({ "Variation", box ("var_type", "Type"), { knob ("var_rate", "Rate", "speed, or delay time for the delays"), knob ("var_depth", "Depth", "depth, or feedback for the delays"),
                                                                   knob ("var_send", "Send"), knob ("var_to_rev", "To reverb") }, {} });
        cols.push_back ({ "Reverb", box ("rev_type", "Type"), { knob ("rev_time", "Time"), knob ("rev_send", "Send") }, {} });
        cols.push_back ({ "EQ and output", nullptr, { knob ("eq_low", "Low"), knob ("eq_high", "High"), knob ("volume", "Volume") }, {} });
    }
    void paint (juce::Graphics& g) override
    {
        for (auto& c : cols) section (g, c.r, c.title);
        // Flujo de la señal, como en el original: parte → inserción → envíos a variación y reverb → EQ.
        const auto f = flow.toFloat();
        lcdPanel (g, f);
        const char* names[] = { "PART", "INSERTION", "VARIATION", "REVERB", "EQ", "OUT" };
        const float bw = 120.0f, bh = 34.0f, y = f.getCentreY();
        std::array<juce::Point<float>, 6> c {
            juce::Point<float> { f.getX() + 90, y }, { f.getX() + 270, y }, { f.getX() + 470, y - 40 }, { f.getX() + 470, y + 40 }, { f.getRight() - 270, y }, { f.getRight() - 90, y } };
        g.setColour (col::ink);
        auto arrow = [&] (juce::Point<float> a, juce::Point<float> b) {
            juce::Path p; p.addArrow ({ a, b }, 1.6f, 9.0f, 8.0f); g.fillPath (p);
        };
        arrow (c[0].translated (bw / 2, 0), c[1].translated (-bw / 2, 0));
        arrow (c[1].translated (bw / 2, 0), c[2].translated (-bw / 2, 0));
        arrow (c[1].translated (bw / 2, 0), c[3].translated (-bw / 2, 0));
        arrow (c[2].translated (0, bh / 2), c[3].translated (0, -bh / 2));
        arrow (c[1].translated (bw / 2, 0), c[4].translated (-bw / 2, 0));
        arrow (c[2].translated (bw / 2, 0), c[4].translated (-bw / 2, -6));
        arrow (c[3].translated (bw / 2, 0), c[4].translated (-bw / 2, 6));
        arrow (c[4].translated (bw / 2, 0), c[5].translated (-bw / 2, 0));
        for (int i = 0; i < 6; ++i)
        {
            const auto r = juce::Rectangle<float> (bw, bh).withCentre (c[(size_t) i]);
            const bool off = (i == 1 && proc.value ("ins_type") < 0.5f) || (i == 2 && proc.value ("var_type") < 0.5f) || (i == 3 && proc.value ("rev_type") < 0.5f);
            g.setColour (col::lcdHi); g.fillRect (r);
            g.setColour (col::ink.withAlpha (off ? 0.35f : 1.0f)); g.drawRect (r, 1.6f);
            dotText (g, names[i], r.getCentreX() - (float) std::strlen (names[i]) * 6.0f, r.getCentreY() - 7.0f, 2.0f, col::ink.withAlpha (off ? 0.4f : 1.0f), false);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        flow = r.removeFromTop (200); r.removeFromTop (12);
        const int w = (r.getWidth() - 36) / 4;
        for (auto& c : cols)
        {
            c.r = r.removeFromLeft (w); r.removeFromLeft (12);
            auto in = c.r.withTrimmedTop (28).reduced (10, 6);
            if (c.type != nullptr) { c.type->setBounds (in.removeFromTop (40)); in.removeFromTop (12); }
            std::vector<juce::Component*> ks (c.knobs.begin(), c.knobs.end());
            gridLayout (in, ks, 2, 100, 10);
        }
    }
    void refresh() override { Page::refresh(); }
private:
    struct Col { juce::String title; ParamBox* type; std::vector<ParamKnob*> knobs; juce::Rectangle<int> r; };
    std::vector<Col> cols;
    juce::Rectangle<int> flow;
};

// ───────── UTIL ─────────
class UtilityPage : public Page, private juce::ListBoxModel
{
public:
    explicit UtilityPage (ATFS1Processor& p) : Page (p)
    {
        importBtn = make<juce::TextButton> ("Import FS1R .syx...");
        importBtn->onClick = [this] { chooseSyx(); };
        exportVBtn = make<juce::TextButton> ("Export voice .syx...");
        exportVBtn->onClick = [this] { exportVoice(); };
        addAndMakeVisible (bank); bank.setModel (this); bank.setRowHeight (24);
        auth = make<ParamBox> (p, "authentic", "Authentic polyphony", "with the filter on, the FS1R plays 16 notes instead of 32");
        out = make<ParamKnob> (p, "output", "Output", "global output level, not stored in presets");
        voices = make<ParamBox> (p, "voices", "Voices");
    }
    void refresh() override { Page::refresh(); bank.updateContent(); bank.repaint(); }
    void paint (juce::Graphics& g) override
    {
        paintSections (g, { { sysR, "FS1R sysex" }, { bankR, "Imported bank" }, { sysParR, "System" }, { aboutR, "About" } });
        g.setColour (col::dim); g.setFont (font (13.0f));
        g.drawFittedText ("Reads voice bulks (608 bytes, from the FS1R or any editor that speaks its format) and Formant Sequence bulks (up to 512 frames). "
                          "Export writes the current voice as a bulk the FS1R accepts. Performances are not imported: the part, effect and controller settings live in this plugin's parameters.",
                          sysTxt, juce::Justification::topLeft, 6);
        g.drawFittedText ("ATFS1 is an independent recreation of the Yamaha FS1R (1998) formant synthesizer, built from the published manuals and from public measurements of the hardware. "
                          "It is not affiliated with or endorsed by Yamaha. FS1R is a trademark of Yamaha Corporation.\n\n"
                          "Default MIDI controllers, as on the FS1R: CC 80 Formant, CC 81 FM, CC 73 Attack, CC 72 Release, CC 74 Cutoff, CC 71 Resonance, CC 1 mod wheel (destination in Perform), CC 64 sustain.\n\n"
                          "Extensions: the full-screen editor, WAV to Fseq analysis, vowel Fseq generator, an Fseq per preset, interpolated Fseq playback, 10 to 1000 % Fseq speed, filter on all 32 voices, inline help.",
                          aboutTxt, juce::Justification::topLeft, 20);
        if (proc.importedVoices.empty())
        {
            g.setColour (col::faint);
            g.drawFittedText ("Import a .syx with several voices to browse them here.", bank.getBounds(), juce::Justification::centred, 2);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto left = r.removeFromLeft (r.getWidth() / 2 - 6); r.removeFromLeft (12);
        sysR = left.removeFromTop (220); left.removeFromTop (12);
        bankR = left;
        sysParR = r.removeFromTop (220); r.removeFromTop (12);
        aboutR = r;
        auto s = sysR.withTrimmedTop (28).reduced (10, 6);
        auto row = s.removeFromTop (32);
        importBtn->setBounds (row.removeFromLeft (row.getWidth() / 2 - 5)); row.removeFromLeft (10); exportVBtn->setBounds (row);
        s.removeFromTop (10); sysTxt = s;
        bank.setBounds (bankR.withTrimmedTop (28).reduced (10, 6));
        auto sp = sysParR.withTrimmedTop (28).reduced (10, 6);
        auth->setBounds (sp.removeFromLeft (260).removeFromTop (40)); sp.removeFromLeft (12);
        voices->setBounds (sp.removeFromLeft (120).removeFromTop (40)); sp.removeFromLeft (12);
        out->setBounds (sp.removeFromLeft (90).withHeight (100));
        aboutTxt = aboutR.withTrimmedTop (28).reduced (10, 6);
    }
private:
    juce::TextButton *importBtn = nullptr, *exportVBtn = nullptr;
    juce::ListBox bank;
    ParamBox *auth = nullptr, *voices = nullptr;
    ParamKnob* out = nullptr;
    juce::Rectangle<int> sysR, bankR, sysParR, aboutR, sysTxt, aboutTxt;
    std::unique_ptr<juce::FileChooser> chooser;
    int getNumRows() override { return (int) proc.importedVoices.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (row < 0 || row >= (int) proc.importedVoices.size()) return;
        if (sel) { g.setColour (col::blue.withAlpha (0.55f)); g.fillRoundedRectangle (0.0f, 1.0f, (float) w, (float) h - 2.0f, 4.0f); }
        const auto& v = proc.importedVoices[(size_t) row];
        g.setColour (col::dim); g.setFont (font (11.5f));
        g.drawText (juce::String (row + 1).paddedLeft ('0', 3), 6, 0, 30, h, juce::Justification::centredLeft);
        g.setColour (col::text); g.setFont (font (14.0f));
        g.drawText (juce::String (v.name), 40, 0, w - 120, h, juce::Justification::centredLeft);
        g.setColour (col::faint); g.setFont (font (11.5f));
        g.drawText ("alg " + juce::String (v.algorithm + 1) + "  " + categories()[juce::jlimit (0, 22, v.category)], w - 110, 0, 104, h, juce::Justification::centredRight);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row < 0 || row >= (int) proc.importedVoices.size()) return;
        proc.setVoice (proc.importedVoices[(size_t) row]);
        proc.currentPresetName = juce::String (proc.importedVoices[(size_t) row].name);
        showLcd ("Voice", proc.currentPresetName);
    }
    void chooseSyx()
    {
        chooser = std::make_unique<juce::FileChooser> ("FS1R sysex (voices or Fseq)", juce::File(), "*.syx;*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& c) {
            if (c.getResult().existsAsFile()) { proc.postMessage (proc.importSyx (c.getResult())); bank.updateContent(); repaint(); }
        });
    }
    void exportVoice()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save the voice as FS1R sysex", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::String (proc.voice().name).trim() + ".syx"), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& c) {
            const auto f = c.getResult();
            if (f != juce::File()) proc.postMessage (proc.exportVoiceSyx (f.withFileExtension ("syx")) ? "Voice saved: " + f.getFileName() : "Could not save " + f.getFileName());
        });
    }
};

} // namespace dh::fs::ui
