#pragma once
// Las páginas de la pantalla salvo PATTERN: VOICE (la voz elegida, con la vista de su forma de onda y la zona para
// soltar muestras), MIXER (16 canales con solo, mute, envíos, bus y medidor), FX (envío A Space, envío B Echo y el
// maestro), SONG (la cadena como fichas), SAMPLES (navegador de archivos), PRESETS (por género, y los propios) y SETUP.

#include "Ctx.h"
#include "ThemePl.h"

namespace dh::pal::ui {

// ── Vista de la voz: la forma de onda del golpe con los ajustes de ahora ──
class Scope : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget, public juce::SettableTooltipClient
{
public:
    explicit Scope (Ctx& c) : ctx (c) { setTooltip ("The hit with the current settings. Drop an audio file here (or a file from SAMPLES) to load it into this voice."); }
    void update()
    {
        const int v = ctx.voice;
        std::vector<float> x;
        title.clear();
        if (ctx.vvalue ("engine") > 0.5f)
        {
            const int kit = (int) ctx.vvalue ("kit");
            const SampleData* s = nullptr; std::shared_ptr<SampleData> keep;
            if (kit >= kitgen::numKits()) { keep = ctx.proc.userSampleData (v); s = keep.get(); title = keep ? "USER SAMPLE  " + juce::String (keep->name).toUpperCase() : "NO USER SAMPLE: DROP ONE HERE"; }
            else { s = kitgen::Library::get().sample (kit, v); title = juce::String (kitgen::kits()[(size_t) kit].name).toUpperCase() + "  /  " + juce::String (params::voiceName (v)).toUpperCase(); }
            if (s != nullptr)
            {
                x = s->data;
                if (ctx.vvalue ("reverse") > 0.5f) std::reverse (x.begin(), x.end());
                secs = (float) ((double) x.size() / std::max (1.0, s->rate));
            }
        }
        else
        {
            AnalogVoice av; av.prepare (24000.0f, v);
            Shape sh; sh.tune = ctx.vvalue ("tune"); sh.decay = ctx.vvalue ("decay") * 0.01f; sh.tone = ctx.vvalue ("tone") * 0.01f; sh.a = ctx.vvalue ("char1") * 0.01f; sh.b = ctx.vvalue ("char2") * 0.01f;
            const int model = (int) ctx.vvalue ("model");
            av.trigger (model, sh, 1.0f, 0.0f, 1);
            x.resize (24000 * 2);
            size_t n = 0; for (; n < x.size() && av.isActive(); ++n) x[n] = av.process();
            x.resize (std::max<size_t> (n, 2400));
            secs = (float) x.size() / 24000.0f;
            title = "ANALOG  /  " + juce::String (analogModels (familyOf (v))[(size_t) juce::jlimit (0, numAnalogModels (v) - 1, model)].name).toUpperCase();
        }
        // Picos por columna (positivo y negativo).
        const int cols = 360; peaks.assign ((size_t) cols * 2, 0.0f);
        float mx = 1e-6f;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const size_t c = std::min<size_t> ((size_t) cols - 1, i * (size_t) cols / std::max<size_t> (1, x.size()));
            peaks[c * 2] = std::max (peaks[c * 2], x[i]); peaks[c * 2 + 1] = std::min (peaks[c * 2 + 1], x[i]); mx = std::max (mx, std::abs (x[i]));
        }
        for (auto& p : peaks) p /= mx;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::glass2); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (hot ? col::rose : col::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, hot ? 2.0f : 1.0f);
        auto head = r.removeFromTop (22).reduced (8, 0);
        g.setColour (col::boneDim); text (g, title, head, juce::Justification::centredLeft, 12.5f, true, 0.08f);
        text (g, juce::String (juce::roundToInt (secs * 1000.0f)) + " MS", head, juce::Justification::centredRight, 12.0f, false, 0.08f);
        auto w = r.reduced (8, 8);
        g.setColour (col::line); g.drawHorizontalLine ((int) w.getCentreY(), w.getX(), w.getRight());
        const int cols = (int) peaks.size() / 2; if (cols == 0) return;
        juce::Path p;
        for (int c = 0; c < cols; ++c)
        {
            const float xx = w.getX() + w.getWidth() * (float) c / (float) cols;
            p.addRectangle (xx, w.getCentreY() - peaks[(size_t) c * 2] * w.getHeight() * 0.48f, std::max (1.0f, w.getWidth() / (float) cols), std::max (1.0f, (peaks[(size_t) c * 2] - peaks[(size_t) c * 2 + 1]) * w.getHeight() * 0.48f));
        }
        g.setColour (col::rose.withAlpha (0.85f)); g.fillPath (p);
    }
    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() == 1; }
    void fileDragEnter (const juce::StringArray&, int, int) override { hot = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { hot = false; repaint(); }
    void filesDropped (const juce::StringArray& f, int, int) override { hot = false; if (ctx.loadSample) ctx.loadSample (ctx.voice, juce::File (f[0])); }
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("file:"); }
    void itemDragEnter (const SourceDetails&) override { hot = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { hot = false; repaint(); }
    void itemDropped (const SourceDetails& d) override { hot = false; if (ctx.loadSample) ctx.loadSample (ctx.voice, juce::File (d.description.toString().substring (5))); }
    void mouseDown (const juce::MouseEvent&) override { ctx.proc.audition (ctx.voice, 0.85f); }
private:
    Ctx& ctx; std::vector<float> peaks; juce::String title; float secs = 0; bool hot = false;
};

class VoicePage : public juce::Component, private juce::Timer
{
public:
    explicit VoicePage (Ctx& c) : ctx (c), scope (c)
    {
        engine.items = { "ANALOG", "DIGITAL" }; engine.fontH = 16.0f;
        engine.setTooltip ("Analog: a circuit model, synthesised. Digital: a sample through the 80s playback chain (built-in kits or your own).");
        for (auto* ch : { &engine, &model, &kit, &chain, &filter, &choke }) addAndMakeVisible (*ch);
        chain.items = params::chainNames(); chain.columns = 5;
        chain.setTooltip ("The playback chain: the kit's own, 8-bit companded, 12-bit at 26 kHz, 12-bit at 40 kHz, or clean.");
        kit.columns = 3; model.columns = 3;
        filter.items = { "OFF", "LP", "HP", "BP" };
        choke.items = { "NONE", "1", "2", "3", "4" }; choke.setTooltip ("Choke group: a hit cuts the other voices of its group (the open hat by the closed one).");
        reverse.label = "Reverse"; addAndMakeVisible (reverse);
        for (auto& k : knobs) addAndMakeVisible (k);
        addAndMakeVisible (scope);
        for (auto* a : { &loadBtn, &clearBtn, &playBtn, &busBtn }) addAndMakeVisible (*a);
        loadBtn.label = "LOAD SAMPLE..."; clearBtn.label = "CLEAR"; playBtn.label = "PLAY"; playBtn.emphasis = true;
        loadBtn.onClick = [this] { chooser = std::make_unique<juce::FileChooser> ("Load a sample for " + juce::String (params::voiceName (ctx.voice)), juce::File(), "*.wav;*.aif;*.aiff;*.flac");
            juce::Component::SafePointer<VoicePage> safe (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc) { if (safe != nullptr && fc.getResult() != juce::File() && safe->ctx.loadSample) safe->ctx.loadSample (safe->ctx.voice, fc.getResult()); }); };
        clearBtn.onClick = [this] { ctx.proc.clearUserSample (ctx.voice); if (ctx.vvalue ("kit") >= (float) kitgen::numKits()) ctx.setParam (params::vid (ctx.voice, "kit"), 0.0f); scope.update(); };
        playBtn.onClick = [this] { ctx.proc.audition (ctx.voice, 0.85f); };
        busBtn.onClick = [this] { busMenu(); };
        busBtn.setTooltip ("Which output this voice goes to when Outputs (SETUP) is Per voice.");
        ctx.onVoice.push_back ([this] { bind(); });
        bind();
        startTimerHz (8);
    }
    void bind()
    {
        const int v = ctx.voice;
        engine.bind (ctx.vparam ("engine"));
        model.items = params::modelNames (v); model.bind (ctx.vparam ("model"));
        kit.items = params::kitNames(); kit.bind (ctx.vparam ("kit"));
        chain.bind (ctx.vparam ("chain")); filter.bind (ctx.vparam ("filter")); choke.bind (ctx.vparam ("choke")); reverse.bind (ctx.vparam ("reverse"));
        static const char* ids[] = { "tune", "decay", "tone", "char1", "char2", "accent", "velsens", "drive", "bits", "rate", "cutoff", "reso", "level", "pan", "senda", "sendb" };
        static const char* names[] = { "Tune", "Decay", "Tone", "", "", "Accent", "Velocity", "Drive", "Bits", "Rate", "Cutoff", "Reso", "Level", "Pan", "Send A", "Send B" };
        for (int i = 0; i < 16; ++i) { knobs[(size_t) i].bind (ctx.vparam (ids[i])); knobs[(size_t) i].label = names[i]; knobs[(size_t) i].bipolar = i == 0 || i == 13; }
        engineAtt = std::make_unique<juce::ParameterAttachment> (*ctx.vparam ("engine"), [this] (float) { layoutSource(); sig = -1; });
        modelAtt = std::make_unique<juce::ParameterAttachment> (*ctx.vparam ("model"), [this] (float) { sig = -1; });
        busAtt = std::make_unique<juce::ParameterAttachment> (*ctx.vparam ("out"), [this] (float) { busBtn.label = "BUS  " + ctx.vparam ("out")->getCurrentValueAsText().toUpperCase(); busBtn.repaint(); });
        busBtn.label = "BUS  " + ctx.vparam ("out")->getCurrentValueAsText().toUpperCase();
        layoutSource(); sig = -1; resized(); repaint();
    }
    void layoutSource()
    {
        const bool dig = ctx.vvalue ("engine") > 0.5f;
        model.setVisible (! dig); kit.setVisible (dig); chain.setVisible (dig); reverse.setVisible (dig);
        const auto cl = ctx.charLabels(); knobs[3].label = cl.first; knobs[4].label = cl.second; knobs[3].repaint(); knobs[4].repaint();
        repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto left = r.removeFromLeft (330);
        left.removeFromTop (46);
        engine.setBounds (left.removeFromTop (32));
        left.removeFromTop (24);
        auto src = left.removeFromTop (128);
        const int mrows = (model.items.size() + 2) / 3;
        model.setBounds (src.withHeight (32 * mrows)); kit.setBounds (src.removeFromTop (92));
        src.removeFromTop (4); chain.setBounds (src.removeFromTop (30));
        left.removeFromTop (8);
        reverse.setBounds (left.removeFromTop (28).removeFromLeft (120));
        left.removeFromTop (14);
        auto b = left.removeFromTop (28);
        choke.setBounds (b.removeFromRight (220));
        left.removeFromTop (12);
        auto bb = left.removeFromTop (28);
        busBtn.setBounds (bb.removeFromLeft (160).reduced (0, 0));
        r.removeFromLeft (18);
        auto right = r.removeFromRight (380);
        right.removeFromTop (22);
        scope.setBounds (right.removeFromTop (196));
        right.removeFromTop (8);
        auto sr = right.removeFromTop (30);
        loadBtn.setBounds (sr.removeFromLeft (150).reduced (1, 0)); clearBtn.setBounds (sr.removeFromLeft (90).reduced (1, 0)); playBtn.setBounds (sr.removeFromRight (100).reduced (1, 0));
        // Perillas: forma, proceso, salida.
        auto mid = r;
        const int kw = 76, kh = 92;
        auto row = [&] (int y, std::initializer_list<int> idx) { int x = mid.getX(); for (int i : idx) { knobs[(size_t) i].setBounds (x, y, kw, kh); x += kw + 4; } };
        row (mid.getY() + 22, { 0, 1, 2, 3, 4, 5, 6 });
        row (mid.getY() + 22 + kh + 34, { 7, 8, 9, 10, 11 });
        filter.setBounds (mid.getX() + 5 * (kw + 4), mid.getY() + 22 + kh + 34 + 30, 2 * kw, 30);
        row (mid.getY() + 22 + 2 * (kh + 34), { 12, 13, 14, 15 });
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (10, 6).toFloat();
        auto left = r.removeFromLeft (330);
        g.setColour (col::bone); text (g, juce::String (params::voiceName (ctx.voice)).toUpperCase(), left.removeFromTop (40), juce::Justification::centredLeft, 30.0f, true, 0.12f);
        auto keyR = juce::Rectangle<float> (left.getX() + 190, left.getY() - 34, 140, 22);
        g.setColour (col::boneDim); text (g, "PAD " + juce::String (ctx.voice + 1) + "   NOTE " + juce::String (slotToNote (ctx.voice, (int) ctx.proc.value ("note_map"))), keyR, juce::Justification::centredRight, 13.0f, false, 0.08f);
        const bool dig = ctx.vvalue ("engine") > 0.5f;
        section (g, { left.getX(), (float) engine.getBottom() + 4, left.getWidth(), 16 }, dig ? "Kit  /  chain" : "Model");
        section (g, { left.getX(), (float) choke.getY() + 6, 100, 16 }, "Choke");
        r.removeFromLeft (18);
        auto right = r.removeFromRight (380);
        section (g, right.removeFromTop (18), "The hit");
        auto mid = r;
        section (g, { mid.getX(), mid.getY(), mid.getWidth(), 16 }, "Shape");
        section (g, { mid.getX(), (float) knobs[7].getY() - 20, mid.getWidth(), 16 }, "Drive  /  crush  /  filter");
        section (g, { mid.getX(), (float) knobs[12].getY() - 20, mid.getWidth(), 16 }, "Output");
        section (g, { (float) filter.getX(), (float) filter.getY() - 18, (float) filter.getWidth(), 16 }, "Filter");
    }
private:
    Ctx& ctx; Scope scope;
    Chips engine, model, kit, chain, filter, choke;
    Toggle reverse;
    std::array<Knob, 16> knobs;
    Action loadBtn, clearBtn, playBtn, busBtn;
    std::unique_ptr<juce::ParameterAttachment> engineAtt, modelAtt, busAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    int sig = -1;
    void timerCallback() override
    {
        // La vista se recalcula cuando cambia algo que la forma (no en cada cuadro).
        int s = ctx.voice;
        for (auto id : { "engine", "model", "kit", "tune", "decay", "tone", "char1", "char2", "reverse" }) s = s * 31 + juce::roundToInt (ctx.vvalue (id) * 10.0f);
        s = s * 31 + ctx.proc.userSampleName (ctx.voice).hashCode();
        if (s != sig && isShowing()) { sig = s; scope.update(); layoutSource(); }
    }
    void busMenu()
    {
        juce::PopupMenu m; auto* p = dynamic_cast<juce::AudioParameterChoice*> (ctx.vparam ("out"));
        for (int b = 0; b < kBuses; ++b) m.addItem (1 + b, busName (b), true, p && p->getIndex() == b);
        juce::Component::SafePointer<VoicePage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&busBtn), [safe] (int r) { if (safe != nullptr && r > 0) safe->ctx.setParam (params::vid (safe->ctx.voice, "out"), (float) (r - 1)); });
    }
};

// ── Fader vertical atado a un parámetro, con el medidor de la voz al lado ──
class Fader : public Bound
{
public:
    float meter = 0;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto m = r.removeFromRight (7.0f);
        auto track = r.withSizeKeepingCentre (4.0f, r.getHeight() - 14.0f);
        g.setColour (col::line); g.fillRoundedRectangle (track, 2.0f);
        const float v = norm();
        const float y = track.getBottom() - track.getHeight() * v;
        g.setColour (col::rose.withAlpha (0.55f)); g.fillRoundedRectangle (track.withTop (y), 2.0f);
        // 0 dB.
        if (param != nullptr) { const float z = track.getBottom() - track.getHeight() * param->convertTo0to1 (0.0f); g.setColour (col::boneFaint); g.drawHorizontalLine ((int) z, r.getX() + 4, r.getRight() - 4); }
        auto cap = juce::Rectangle<float> (r.getWidth() - 10.0f, 14.0f).withCentre ({ r.getCentreX(), y });
        g.setColour (col::bone); g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (col::ink); g.drawHorizontalLine ((int) cap.getCentreY(), cap.getX() + 3, cap.getRight() - 3);
        // Medidor: verde hueso hasta −6 dB, rosa arriba.
        auto mm = m.withTrimmedTop (7).withTrimmedBottom (7);
        g.setColour (col::cell); g.fillRect (mm);
        const float db = juce::Decibels::gainToDecibels (meter, -60.0f), mh = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        g.setColour (db > -6.0f ? col::rose : col::bone.withAlpha (0.8f)); g.fillRect (mm.withTop (mm.getBottom() - mm.getHeight() * mh));
    }
    void mouseDown (const juce::MouseEvent&) override { if (param) { att->beginGesture(); start = norm(); } }
    void mouseDrag (const juce::MouseEvent& e) override { if (param) setNorm (start - (float) e.getDistanceFromDragStartY() / ((float) getHeight() * (e.mods.isShiftDown() ? 4.0f : 1.0f)), true); }
    void mouseUp (const juce::MouseEvent&) override { if (param) att->endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (param) setNorm (param->convertTo0to1 (0.0f), false); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { if (param) setNorm (norm() + w.deltaY * 0.04f, false); }
private:
    float start = 0;
};

class MixerPage : public juce::Component
{
public:
    explicit MixerPage (Ctx& c) : ctx (c)
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            auto& s = strips[(size_t) i];
            s.mute.label = "M"; s.solo.label = "S";
            s.mute.bind (ctx.vparam ("mute", i)); s.solo.bind (ctx.vparam ("solo", i));
            s.pan.bind (ctx.vparam ("pan", i)); s.pan.label = "Pan"; s.pan.bipolar = true;
            s.a.bind (ctx.vparam ("senda", i)); s.a.label = "Send A";
            s.b.bind (ctx.vparam ("sendb", i)); s.b.label = "Send B";
            s.fader.bind (ctx.vparam ("level", i));
            s.fader.setTooltip (juce::String (params::voiceName (i)) + " level. Double-click: 0 dB.");
            s.bus.onClick = [this, i] { busMenu (i); };
            for (auto* comp : std::initializer_list<juce::Component*> { &s.mute, &s.solo, &s.pan, &s.a, &s.b, &s.fader, &s.bus }) addAndMakeVisible (comp);
            s.busAtt = std::make_unique<juce::ParameterAttachment> (*ctx.vparam ("out", i), [this, i] (float) { updateBus (i); });
            updateBus (i);
        }
        for (auto& r : retKnobs) addAndMakeVisible (r);
        retKnobs[0].bind (ctx.param ("a_return")); retKnobs[0].label = "Space";
        retKnobs[1].bind (ctx.param ("b_return")); retKnobs[1].label = "Echo";
        retKnobs[2].bind (ctx.param ("m_volume")); retKnobs[2].label = "Master";
    }
    void updateBus (int i) { auto& s = strips[(size_t) i]; s.bus.label = juce::String (busName ((int) ctx.vvalue ("out", i))).toUpperCase().replace ("/", " "); s.bus.fontH = 11.5f; s.bus.repaint(); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 4);
        auto ret = r.removeFromRight (86);
        const float w = (float) r.getWidth() / (float) kNumSlots;
        for (int i = 0; i < kNumSlots; ++i)
        {
            auto& s = strips[(size_t) i];
            auto c = juce::Rectangle<int> (r.getX() + (int) (w * (float) i), r.getY(), (int) w, r.getHeight()).reduced (3, 0);
            c.removeFromTop (24);
            auto ms = c.removeFromTop (24); s.mute.setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (1, 0)); s.solo.setBounds (ms.reduced (1, 0));
            c.removeFromTop (4);
            s.pan.setBounds (c.removeFromTop (64)); s.a.setBounds (c.removeFromTop (64)); s.b.setBounds (c.removeFromTop (64));
            s.bus.setBounds (c.removeFromBottom (24));
            c.removeFromBottom (16);
            s.fader.setBounds (c.withSizeKeepingCentre (std::min (c.getWidth(), 40), c.getHeight()));
        }
        ret.removeFromTop (24);
        for (auto& k : retKnobs) { k.setBounds (ret.removeFromTop (78)); ret.removeFromTop (6); }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (8, 4);
        auto ret = r.removeFromRight (86).toFloat();
        const float w = (float) r.getWidth() / (float) kNumSlots;
        for (int i = 0; i < kNumSlots; ++i)
        {
            auto c = juce::Rectangle<float> ((float) r.getX() + w * (float) i, (float) r.getY(), w, (float) r.getHeight()).reduced (3, 0);
            if (i == ctx.voice) { g.setColour (col::cell); g.fillRoundedRectangle (c.expanded (2, 2), 4.0f); }
            g.setColour (i == ctx.voice ? col::rose : col::boneDim);
            text (g, juce::String (params::voiceName (i)).toUpperCase(), c.removeFromTop (22), juce::Justification::centred, 12.5f, i == ctx.voice, 0.04f);
            const auto& s = strips[(size_t) i];
            g.setColour (col::boneDim);
            text (g, ctx.vparam ("level", i)->getCurrentValueAsText(), { c.getX(), (float) s.fader.getBottom(), c.getWidth(), 15 }, juce::Justification::centred, 12.0f);
        }
        g.setColour (col::boneDim); text (g, "RETURNS", ret.removeFromTop (22), juce::Justification::centred, 12.5f, true, 0.12f);
    }
    void tick()
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            auto& s = strips[(size_t) i];
            const float lv = ctx.level[(size_t) i];
            if (std::abs (lv - s.fader.meter) > 0.003f) { s.fader.meter = lv; s.fader.repaint(); }
        }
        if (ctx.voice != shownVoice) { shownVoice = ctx.voice; repaint(); }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        auto r = getLocalBounds().reduced (8, 4); r.removeFromRight (86);
        const int i = (int) ((float) (e.x - r.getX()) / ((float) r.getWidth() / (float) kNumSlots));
        if (i >= 0 && i < kNumSlots) ctx.selectVoice (i);
    }
private:
    struct Strip { Toggle mute, solo; Knob pan, a, b; Fader fader; Action bus; std::unique_ptr<juce::ParameterAttachment> busAtt; };
    Ctx& ctx; std::array<Strip, kNumSlots> strips; std::array<Knob, 3> retKnobs; int shownVoice = -1;
    void busMenu (int i)
    {
        juce::PopupMenu m;
        m.addSectionHeader (juce::String (params::voiceName (i)) + ": output when Outputs is Per voice");
        for (int b = 0; b < kBuses; ++b) m.addItem (1 + b, busName (b), true, (int) ctx.vvalue ("out", i) == b);
        juce::Component::SafePointer<MixerPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&strips[(size_t) i].bus), [safe, i] (int r) { if (safe != nullptr && r > 0) safe->ctx.setParam (params::vid (i, "out"), (float) (r - 1)); });
    }
};

// ── Efectos ──
class FxPage : public juce::Component
{
public:
    explicit FxPage (Ctx& c) : ctx (c)
    {
        auto chips = [&] (Chips& ch, const char* id, int cols) { ch.bind (ctx.param (id)); if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (ch.param)) ch.items = p->choices; ch.columns = cols; addAndMakeVisible (ch); };
        chips (aType, "a_type", 4); chips (bType, "b_type", 4); chips (bTime, "b_time", 7); chips (lofi, "m_lofi", 5);
        chips (ratio, "m_ratio", 3); chips (attack, "m_attack", 6); chips (release, "m_release", 5);
        bTime.fontH = 13.0f; attack.fontH = 13.0f; release.fontH = 13.0f; lofi.fontH = 13.0f;
        aType.setTooltip ("Space types: Gated (the 80s gated room), Nonlinear (flat program, sharp cut), Reverse, Plate, Spring, Hall, Room.");
        bType.setTooltip ("Echo types: 12-bit digital delay, Tape echo, BBD delay, BBD chorus, Ensemble, Jet flanger, Phaser. The modulation types return wet only.");
        auto k = [&] (Knob& kn, const char* id, const char* label, bool bip = false) { kn.bind (ctx.param (id)); kn.label = label; kn.bipolar = bip; addAndMakeVisible (kn); };
        k (aK[0], "a_size", "Size"); k (aK[1], "a_predelay", "Pre-delay"); k (aK[2], "a_gate", "Gate"); k (aK[3], "a_shape", "Shape", true);
        k (aK[4], "a_tone", "Tone"); k (aK[5], "a_lowcut", "Low cut"); k (aK[6], "a_width", "Width"); k (aK[7], "a_return", "Return");
        k (bK[0], "b_fb", "Feedback"); k (bK[1], "b_tone", "Tone"); k (bK[2], "b_rate", "Rate"); k (bK[3], "b_depth", "Depth"); k (bK[4], "b_width", "Width"); k (bK[5], "b_return", "Return");
        k (mK[0], "m_lofi_mix", "Lo-fi mix"); k (mK[1], "m_tape_drive", "Tape drive"); k (mK[2], "m_comp", "Glue"); k (mK[3], "m_comp_mix", "Glue mix");
        k (mK[4], "m_exciter", "Exciter"); k (mK[5], "m_exciter_freq", "Freq"); k (mK[6], "m_volume", "Volume");
        tape.label = "Tape"; tape.bind (ctx.param ("m_tape")); addAndMakeVisible (tape);
        tape.setTooltip ("The master through tape: hysteresis at 4x, drive-compensated (adds 54 samples of latency).");
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        const int cw = (r.getWidth() - 40) / 3;
        auto colA = r.removeFromLeft (cw); r.removeFromLeft (20);
        auto colB = r.removeFromLeft (cw); r.removeFromLeft (20);
        auto colM = r;
        const int kw = (cw - 6) / 4, kh = 90;
        auto grid = [&] (juce::Rectangle<int> a, Knob* ks, int n) { for (int i = 0; i < n; ++i) ks[i].setBounds (a.getX() + (i % 4) * kw, a.getY() + (i / 4) * (kh + 6), kw, kh); };
        colA.removeFromTop (22); aType.setBounds (colA.removeFromTop (64)); colA.removeFromTop (14); grid (colA, aK.data(), 8);
        colB.removeFromTop (22); bType.setBounds (colB.removeFromTop (64)); colB.removeFromTop (20); bTime.setBounds (colB.removeFromTop (60)); colB.removeFromTop (10); grid (colB, bK.data(), 6);
        colM.removeFromTop (22);
        auto lr = colM.removeFromTop (30); lofi.setBounds (lr);
        colM.removeFromTop (8);
        auto row1 = colM.removeFromTop (kh);
        mK[0].setBounds (row1.removeFromLeft (kw)); tape.setBounds (row1.removeFromLeft (kw).withSizeKeepingCentre (kw - 8, 28)); mK[1].setBounds (row1.removeFromLeft (kw)); mK[6].setBounds (row1.removeFromLeft (kw));
        colM.removeFromTop (22);
        auto gr = colM.removeFromTop (kh); mK[2].setBounds (gr.removeFromLeft (kw)); mK[3].setBounds (gr.removeFromLeft (kw)); grR = gr.reduced (8, 18).toFloat();
        colM.removeFromTop (4);
        ratio.setBounds (colM.removeFromTop (26)); colM.removeFromTop (2); attack.setBounds (colM.removeFromTop (26)); colM.removeFromTop (2); release.setBounds (colM.removeFromTop (26));
        colM.removeFromTop (18);
        auto ex = colM.removeFromTop (kh); mK[4].setBounds (ex.removeFromLeft (kw)); mK[5].setBounds (ex.removeFromLeft (kw));
        colX = { colA.getX(), colB.getX(), colM.getX() };
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (10, 6).toFloat();
        const float cw = (r.getWidth() - 40) / 3;
        section (g, { r.getX(), r.getY(), cw, 16 }, "Send A  /  Space");
        section (g, { r.getX() + cw + 20, r.getY(), cw, 16 }, "Send B  /  Echo");
        section (g, { r.getX() + 2 * (cw + 20), r.getY(), cw, 16 }, "Master");
        section (g, { (float) bTime.getX(), (float) bTime.getY() - 18, (float) bTime.getWidth(), 16 }, "Time (tempo synced)");
        section (g, { (float) mK[2].getX(), (float) mK[2].getY() - 18, cw, 16 }, "Glue");
        // Medidor de reducción de ganancia del Glue.
        g.setColour (col::cell); g.fillRoundedRectangle (grR, 2.0f);
        const float grDb = shownGr, f = juce::jlimit (0.0f, 1.0f, grDb / 12.0f);
        g.setColour (col::rose); g.fillRoundedRectangle (grR.withWidth (grR.getWidth() * f), 2.0f);
        g.setColour (col::boneDim); text (g, "GR  " + juce::String (grDb, 1) + " dB", grR.translated (0, -16).withHeight (14), juce::Justification::centredLeft, 12.0f, false, 0.08f);
    }
    void tick()
    {
        const float gr = std::abs (ctx.proc.view.gr.load());
        if (std::abs (gr - shownGr) > 0.05f) { shownGr = gr; repaint (grR.expanded (2, 20).getSmallestIntegerContainer()); }
    }
private:
    Ctx& ctx;
    Chips aType, bType, bTime, lofi, ratio, attack, release;
    std::array<Knob, 8> aK; std::array<Knob, 6> bK; std::array<Knob, 7> mK;
    Toggle tape;
    juce::Rectangle<float> grR; std::array<int, 3> colX {};
    float shownGr = 0;
};

// ── Canción: la cadena como fichas ──
class SongPage : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SongPage (Ctx& c) : ctx (c)
    {
        mode.bind (ctx.param ("mode")); mode.items = { "A", "B", "A > B", "SONG" }; mode.fontH = 16.0f;
        mode.setTooltip ("What plays: A or B of the current pattern, the two alternating, or the song chain.");
        group.items.clear(); for (int i = 0; i < 16; ++i) group.items.add (juce::String (i + 1));
        group.bind (ctx.param ("group")); group.columns = 16;
        group.setTooltip ("The pattern that plays in modes A, B and A > B (changes land on the next bar).");
        autofill.bind (ctx.param ("autofill")); autofill.items = { "OFF", "EVERY 2", "EVERY 4", "EVERY 8", "EVERY 16" };
        autofill.setTooltip ("Auto fill: the last pass of every N plays the Fill.");
        for (auto* ch : { &mode, &group, &autofill, &eGroup, &eVar }) addAndMakeVisible (*ch);
        for (int i = 0; i < 16; ++i) eGroup.items.add (juce::String (i + 1));
        eGroup.columns = 16;
        eGroup.getIndex = [this] { auto ch = ctx.proc.getChain(); return sel >= 0 && sel < (int) ch.size() ? (int) ch[(size_t) sel].group : -1; };
        eGroup.onPick = [this] (int i) { edit ([i] (ChainEntry& e) { e.group = (uint8_t) i; }); };
        eVar.items = { "A", "B" };
        eVar.getIndex = [this] { auto ch = ctx.proc.getChain(); return sel >= 0 && sel < (int) ch.size() ? (int) ch[(size_t) sel].var : -1; };
        eVar.onPick = [this] (int i) { edit ([i] (ChainEntry& e) { e.var = (uint8_t) i; }); };
        eFill.label = "Ends with fill";
        eFill.getState = [this] { auto ch = ctx.proc.getChain(); return sel >= 0 && sel < (int) ch.size() && ch[(size_t) sel].fill; };
        eFill.setState = [this] (bool b) { edit ([b] (ChainEntry& e) { e.fill = b; }); };
        addAndMakeVisible (eFill);
        for (auto* a : { &repDown, &repUp, &leftBtn, &rightBtn, &dupBtn, &delBtn, &clearBtn, &addBtn }) addAndMakeVisible (*a);
        repDown.label = "-"; repUp.label = "+"; leftBtn.label = "< MOVE"; rightBtn.label = "MOVE >"; dupBtn.label = "DUPLICATE"; delBtn.label = "DELETE"; clearBtn.label = "CLEAR SONG";
        addBtn.label = "+ ADD " + Ctx::patternName (ctx.editPattern()); addBtn.emphasis = true;
        repDown.onClick = [this] { edit ([] (ChainEntry& e) { e.repeats = (uint8_t) std::max (1, e.repeats - 1); }); };
        repUp.onClick = [this] { edit ([] (ChainEntry& e) { e.repeats = (uint8_t) std::min (64, e.repeats + 1); }); };
        leftBtn.onClick = [this] { move (-1); }; rightBtn.onClick = [this] { move (1); };
        dupBtn.onClick = [this] { auto ch = ctx.proc.getChain(); if (sel >= 0 && sel < (int) ch.size() && (int) ch.size() < kMaxChain) { ch.insert (ch.begin() + sel + 1, ch[(size_t) sel]); ++sel; ctx.proc.setChain (ch); } repaint(); };
        delBtn.onClick = [this] { auto ch = ctx.proc.getChain(); if (sel >= 0 && sel < (int) ch.size()) { ch.erase (ch.begin() + sel); sel = std::min (sel, (int) ch.size() - 1); ctx.proc.setChain (ch); } repaint(); };
        clearBtn.onClick = [this] { ctx.proc.setChain ({}); sel = -1; repaint(); };
        addBtn.onClick = [this] {
            auto ch = ctx.proc.getChain(); if ((int) ch.size() >= kMaxChain) return;
            ChainEntry e; e.group = (uint8_t) ctx.group; e.var = (uint8_t) std::min (1, ctx.var); e.repeats = 1; e.fill = ctx.var == 2;
            ch.push_back (e); sel = (int) ch.size() - 1; ctx.proc.setChain (ch); repaint();
        };
        setTooltip ("Click a tile to edit it; drag a tile to move it. The song plays in Song mode.");
        ctx.onTarget.push_back ([this] { addBtn.label = "+ ADD " + Ctx::patternName (patIndex (ctx.group, std::min (1, ctx.var))); addBtn.repaint(); });
        ctx.onPatterns.push_back ([this] { repaint(); });
    }
    juce::Rectangle<float> tilesR() const { return getLocalBounds().reduced (10, 6).toFloat().withTrimmedTop (150).withTrimmedBottom (96); }
    juce::Rectangle<float> tileR (int i) const { auto r = tilesR(); const float w = r.getWidth() / 16.0f, h = std::min (62.0f, r.getHeight() / 4.0f); return { r.getX() + w * (float) (i % 16), r.getY() + h * (float) (i / 16), w, h }; }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        r.removeFromTop (20);
        auto top = r.removeFromTop (32);
        mode.setBounds (top.removeFromLeft (300)); top.removeFromLeft (30); autofill.setBounds (top.removeFromLeft (420));
        r.removeFromTop (24);
        group.setBounds (r.removeFromTop (30));
        auto bottom = r.removeFromBottom (64);
        auto row1 = bottom.removeFromTop (30);
        eGroup.setBounds (row1.removeFromLeft (600)); row1.removeFromLeft (8); eVar.setBounds (row1.removeFromLeft (90)); row1.removeFromLeft (16);
        repDown.setBounds (row1.removeFromLeft (28)); row1.removeFromLeft (64); repUp.setBounds (row1.removeFromLeft (28)); row1.removeFromLeft (16);
        eFill.setBounds (row1.removeFromLeft (150));
        bottom.removeFromTop (6);
        auto row2 = bottom.removeFromTop (28);
        leftBtn.setBounds (row2.removeFromLeft (90).reduced (1, 0)); rightBtn.setBounds (row2.removeFromLeft (90).reduced (1, 0)); dupBtn.setBounds (row2.removeFromLeft (110).reduced (1, 0)); delBtn.setBounds (row2.removeFromLeft (90).reduced (1, 0));
        clearBtn.setBounds (row2.removeFromRight (120).reduced (1, 0)); addBtn.setBounds (row2.removeFromRight (140).reduced (1, 0));
        repR = { (float) repDown.getRight(), (float) repDown.getY(), 64, 30 };
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (10, 6).toFloat();
        section (g, { r.getX(), r.getY(), 300, 16 }, "Play mode");
        section (g, { r.getX() + 330, r.getY(), 420, 16 }, "Auto fill");
        section (g, { r.getX(), (float) group.getY() - 20, r.getWidth(), 16 }, "Pattern (A, B, A > B)");
        const auto ch = ctx.proc.getChain();
        double beats = 0; for (auto& e : ch) beats += (double) e.repeats * ctx.proc.getPattern (patIndex (e.group, e.var)).length * scaleBeats (ctx.proc.getPattern (patIndex (e.group, e.var)).scale);
        section (g, { r.getX(), tilesR().getY() - 22, r.getWidth(), 16 }, "Song  /  " + juce::String ((int) ch.size()) + " entries, " + juce::String (beats / 4.0, 1) + " bars");
        const int playing = ctx.proc.view.running.load() ? ctx.proc.view.entry.load() : -1;
        for (int i = 0; i <= (int) ch.size() && i < kMaxChain; ++i)
        {
            auto t = tileR (i).reduced (3.0f);
            if (i == (int) ch.size()) { g.setColour (col::line); g.drawRoundedRectangle (t, 4.0f, 1.0f); g.setColour (col::boneFaint); text (g, "+", t, juce::Justification::centred, 22.0f); break; }
            const auto& e = ch[(size_t) i];
            const bool isSel = i == sel, isPlay = i == playing;
            g.setColour (isPlay ? col::roseDim : (isSel ? col::cell2.brighter (0.08f) : col::cell2)); g.fillRoundedRectangle (t, 4.0f);
            g.setColour (isSel ? col::rose : col::line); g.drawRoundedRectangle (t, 4.0f, isSel ? 2.0f : 1.0f);
            g.setColour (col::bone); text (g, juce::String (e.group + 1) + (e.var ? "B" : "A"), t.withTrimmedBottom (t.getHeight() * 0.4f), juce::Justification::centred, 22.0f, true);
            g.setColour (col::boneDim); text (g, "x" + juce::String ((int) e.repeats) + (e.fill ? "  FILL" : ""), t.withTrimmedTop (t.getHeight() * 0.58f), juce::Justification::centred, 12.0f, e.fill, 0.06f);
            if (isPlay)
            {
                const double s0 = ctx.proc.view.segStart.load(), s1 = ctx.proc.view.segEnd.load(), ppq = ctx.proc.view.ppq.load();
                const float f = (float) juce::jlimit (0.0, 1.0, (ppq - s0) / std::max (1e-6, s1 - s0));
                const float rf = ((float) ctx.proc.view.rep.load() + f) / (float) std::max<int> (1, e.repeats);
                g.setColour (col::rose); g.fillRect (t.getX() + 4, t.getBottom() - 5, (t.getWidth() - 8) * rf, 2.5f);
            }
            if (i == dropAt) { g.setColour (col::rose); g.fillRect (t.getX() - 4, t.getY(), 2.5f, t.getHeight()); }
        }
        if (sel >= 0 && sel < (int) ch.size())
        {
            g.setColour (col::bone); text (g, juce::String ((int) ch[(size_t) sel].repeats) + "x", repR, juce::Justification::centred, 18.0f, true);
            section (g, { (float) eGroup.getX(), (float) eGroup.getY() - 20, (float) getWidth() - 40, 16 }, "Entry " + juce::String (sel + 1) + ": pattern, variation, repeats");
        }
        else { g.setColour (col::boneFaint); text (g, "Select a tile to edit it, or add the pattern being edited with + ADD.", { (float) eGroup.getX(), (float) eGroup.getY() - 20, 700, 16 }, juce::Justification::centredLeft, 13.0f); }
        const bool en = sel >= 0 && sel < (int) ch.size();
        for (auto* c : std::initializer_list<juce::Component*> { &eGroup, &eVar, &eFill, &repDown, &repUp, &leftBtn, &rightBtn, &dupBtn, &delBtn }) c->setAlpha (en ? 1.0f : 0.35f);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto ch = ctx.proc.getChain();
        for (int i = 0; i <= (int) ch.size() && i < kMaxChain; ++i)
            if (tileR (i).contains (e.position))
            {
                if (i == (int) ch.size()) { addBtn.onClick(); return; }
                sel = i; dragFrom = i; repaint(); eGroup.repaint(); eVar.repaint(); eFill.repaint();
                if (e.mods.isPopupMenu()) { delBtn.onClick(); }
                return;
            }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragFrom < 0) return;
        int at = -1; const auto ch = ctx.proc.getChain();
        for (int i = 0; i <= (int) ch.size(); ++i) if (tileR (i).contains (e.position)) at = i;
        if (at != dropAt) { dropAt = at; repaint(); }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragFrom >= 0 && dropAt >= 0 && dropAt != dragFrom && dropAt != dragFrom + 1)
        {
            auto ch = ctx.proc.getChain();
            const auto e = ch[(size_t) dragFrom];
            ch.erase (ch.begin() + dragFrom);
            const int to = dropAt > dragFrom ? dropAt - 1 : dropAt;
            ch.insert (ch.begin() + std::min (to, (int) ch.size()), e);
            sel = to; ctx.proc.setChain (ch);
        }
        dragFrom = -1; dropAt = -1; repaint();
    }
    void tick()
    {
        const int e = ctx.proc.view.running.load() ? ctx.proc.view.entry.load() : -1;
        if (e >= 0 || e != shownEntry) { shownEntry = e; repaint (tilesR().expanded (4).getSmallestIntegerContainer()); }
    }
private:
    Ctx& ctx;
    Chips mode, group, autofill, eGroup, eVar; Toggle eFill;
    Action repDown, repUp, leftBtn, rightBtn, dupBtn, delBtn, clearBtn, addBtn;
    int sel = -1, dragFrom = -1, dropAt = -1, shownEntry = -2;
    juce::Rectangle<float> repR;
    void edit (std::function<void (ChainEntry&)> f)
    {
        auto ch = ctx.proc.getChain();
        if (sel < 0 || sel >= (int) ch.size()) return;
        f (ch[(size_t) sel]); ctx.proc.setChain (ch); repaint(); eGroup.repaint(); eVar.repaint(); eFill.repaint();
    }
    void move (int d)
    {
        auto ch = ctx.proc.getChain();
        if (sel < 0 || sel >= (int) ch.size() || sel + d < 0 || sel + d >= (int) ch.size()) return;
        std::swap (ch[(size_t) sel], ch[(size_t) (sel + d)]); sel += d; ctx.proc.setChain (ch); repaint();
    }
};

// ── Muestras: navegador de archivos ──
class SamplesPage : public juce::Component, private juce::ListBoxModel
{
public:
    explicit SamplesPage (Ctx& c) : ctx (c)
    {
        list.setModel (this); list.setRowHeight (22); list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.getViewport()->setScrollBarThickness (8);
        addAndMakeVisible (list);
        for (auto* a : { &upBtn, &chooseBtn }) addAndMakeVisible (*a);
        upBtn.label = "UP"; chooseBtn.label = "CHOOSE FOLDER...";
        upBtn.onClick = [this] { setDir (dir.getParentDirectory()); };
        chooseBtn.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser> ("Choose a folder of samples", dir);
            juce::Component::SafePointer<SamplesPage> safe (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe] (const juce::FileChooser& fc) { if (safe != nullptr && fc.getResult().isDirectory()) safe->setDir (fc.getResult()); });
        };
        places.items = { "HOME", "DESKTOP", "MUSIC", "DOWNLOADS", "DOCUMENTS" };
        places.getIndex = [] { return -1; };
        places.onPick = [this] (int i) {
            static const juce::File::SpecialLocationType t[] = { juce::File::userHomeDirectory, juce::File::userDesktopDirectory, juce::File::userMusicDirectory, juce::File::userHomeDirectory, juce::File::userDocumentsDirectory };
            auto f = juce::File::getSpecialLocation (t[i]); if (i == 3) f = f.getChildFile ("Downloads");
            setDir (f);
        };
        addAndMakeVisible (places);
        const auto forced = juce::SystemStats::getEnvironmentVariable ("PALLOR_SAMPLES_DIR", {});
        if (forced.isNotEmpty() && juce::File (forced).isDirectory()) { remember = false; setDir (juce::File (forced)); return; }
        auto props = settingsFile();
        const juce::File last (props.isNotEmpty() ? props : juce::String());
        setDir (last.isDirectory() ? last : juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    }
    void setDir (const juce::File& d)
    {
        if (! d.isDirectory()) return;
        dir = d; items.clear();
        for (const auto& f : dir.findChildFiles (juce::File::findDirectories, false)) if (! f.isHidden()) items.add (f);
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.WAV;*.AIF;*.AIFF;*.FLAC");
        files.sort(); items.sort(); items.addArray (files);
        list.updateContent(); list.deselectAllRows(); list.scrollToEnsureRowIsOnscreen (0);
        if (remember) saveSettings (dir.getFullPathName());
        repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        r.removeFromTop (20);
        auto top = r.removeFromTop (28);
        places.setBounds (top.removeFromLeft (440)); top.removeFromLeft (10);
        chooseBtn.setBounds (top.removeFromRight (160)); upBtn.setBounds (top.removeFromRight (60).withTrimmedRight (6));
        r.removeFromTop (30);
        list.setBounds (r.removeFromLeft (r.getWidth() * 2 / 3));
        infoR = r.reduced (16, 0).toFloat();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (10, 6).toFloat();
        section (g, r.removeFromTop (18), "Samples");
        g.setColour (col::boneDim); text (g, dir.getFullPathName(), { r.getX(), (float) places.getBottom() + 6, (float) list.getWidth(), 20 }, juce::Justification::centredLeft, 13.5f);
        auto i = infoR;
        g.setColour (col::boneDim); text (g, "LOADS INTO", i.removeFromTop (18), juce::Justification::centredLeft, 12.5f, true, 0.14f);
        g.setColour (col::rose); text (g, juce::String (params::voiceName (ctx.voice)).toUpperCase(), i.removeFromTop (36), juce::Justification::centredLeft, 28.0f, true, 0.1f);
        const auto nm = ctx.proc.userSampleName (ctx.voice);
        g.setColour (col::bone); text (g, nm.isEmpty() ? juce::String ("No user sample yet") : nm, i.removeFromTop (22), juce::Justification::centredLeft, 15.0f);
        i.removeFromTop (12);
        g.setColour (col::boneFaint);
        g.setFont (font (14.0f));
        g.drawFittedText ("Click a file: it loads into the selected voice, switches it to the digital engine with the User sample kit, and plays it. "
                          "Click a pad first to load into another voice, or drag a file onto a pad. Files longer than 20 s are cut. "
                          "The sample travels inside the project (up to 8 MB for all voices together; beyond that, only its path).",
                          i.removeFromTop (150).toNearestInt(), juce::Justification::topLeft, 8);
    }
    void tick() { if (ctx.voice != shownVoice || ctx.proc.userSampleName (ctx.voice) != shownName) { shownVoice = ctx.voice; shownName = ctx.proc.userSampleName (ctx.voice); repaint (infoR.expanded (4).getSmallestIntegerContainer()); } }
private:
    Ctx& ctx; juce::ListBox list; Action upBtn, chooseBtn; Chips places;
    juce::File dir; juce::Array<juce::File> items; juce::Rectangle<float> infoR;
    std::unique_ptr<juce::FileChooser> chooser; int shownVoice = -1; juce::String shownName; bool remember = true;

    static juce::File settingsPath() { return PallorProcessor::userPresetDir().getParentDirectory().getChildFile ("samples-folder.txt"); }
    static juce::String settingsFile() { return settingsPath().loadFileAsString().trim(); }
    static void saveSettings (const juce::String& s) { settingsPath().getParentDirectory().createDirectory(); settingsPath().replaceWithText (s); }

    int getNumRows() override { return items.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= items.size()) return;
        const auto& f = items[row];
        if (selected) { g.setColour (col::roseDim.withAlpha (0.6f)); g.fillRect (0, 0, w, h); }
        else if (row % 2) { g.setColour (col::cell.withAlpha (0.5f)); g.fillRect (0, 0, w, h); }
        g.setColour (f.isDirectory() ? col::boneDim : col::bone);
        text (g, (f.isDirectory() ? "/ " : "") + f.getFileName(), { 8.0f, 0, (float) w - 100, (float) h }, juce::Justification::centredLeft, 14.5f, f.isDirectory());
        if (! f.isDirectory()) { g.setColour (col::boneFaint); text (g, juce::File::descriptionOfSizeInBytes (f.getSize()), { (float) w - 96, 0, 88, (float) h }, juce::Justification::centredRight, 12.5f); }
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row < 0 || row >= items.size()) return;
        const auto f = items[row];
        if (f.isDirectory()) { setDir (f); return; }
        if (ctx.loadSample) ctx.loadSample (ctx.voice, f);
    }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { if (row >= 0 && row < items.size() && items[row].isDirectory()) setDir (items[row]); }
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        if (rows.size() != 1) return {};
        const auto f = items[rows[0]];
        return f.existsAsFile() ? juce::var ("file:" + f.getFullPathName()) : juce::var();
    }
};

// ── Presets por género ──
class PresetsPage : public juce::Component
{
public:
    explicit PresetsPage (Ctx& c) : ctx (c)
    {
        what.items = { "SOUND + PATTERNS", "SOUND ONLY", "PATTERNS ONLY" };
        what.getIndex = [this] { return loadWhat; }; what.onPick = [this] (int i) { loadWhat = i; };
        what.setTooltip ("What a click loads: the whole preset, only the kit and effects, or only the patterns and song.");
        addAndMakeVisible (what);
        for (auto* a : { &saveBtn, &delBtn, &folderBtn }) addAndMakeVisible (*a);
        saveBtn.label = "SAVE AS..."; delBtn.label = "DELETE"; folderBtn.label = "OPEN FOLDER";
        saveBtn.onClick = [this] { save(); };
        delBtn.onClick = [this] { const int cp = ctx.proc.getCurrentProgram(); if (cp >= 0 && cp < (int) ctx.proc.getPresets().size() && ! ctx.proc.getPresets()[(size_t) cp].factory) ctx.proc.deleteUserPreset (cp); rebuild(); };
        folderBtn.onClick = [] { PallorProcessor::userPresetDir().createDirectory(); PallorProcessor::userPresetDir().revealToUser(); };
        ctx.onPreset.push_back ([this] { rebuild(); });
        rebuild();
    }
    void rebuild()
    {
        genres.clearQuick();
        for (auto& p : ctx.proc.getPresets()) if (! genres.contains (p.category)) genres.add (p.category);
        const int cp = ctx.proc.getCurrentProgram();
        if (cp >= 0 && cp < (int) ctx.proc.getPresets().size()) genre = genres.indexOf (ctx.proc.getPresets()[(size_t) cp].category);
        genre = juce::jlimit (0, std::max (0, genres.size() - 1), genre);
        repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto bottom = r.removeFromBottom (30);
        what.setBounds (bottom.removeFromLeft (480));
        folderBtn.setBounds (bottom.removeFromRight (130).reduced (1, 0)); delBtn.setBounds (bottom.removeFromRight (90).reduced (1, 0)); saveBtn.setBounds (bottom.removeFromRight (120).reduced (1, 0));
    }
    juce::Rectangle<float> genreR (int i) const { return { 10.0f, 30.0f + 25.0f * (float) i, 200.0f, 24.0f }; }
    std::vector<int> inGenre() const { std::vector<int> v; const auto& ps = ctx.proc.getPresets(); for (int i = 0; i < (int) ps.size(); ++i) if (genre >= 0 && ps[(size_t) i].category == genres[genre]) v.push_back (i); return v; }
    // Filas de 50 px; si el género tiene muchos (los propios), dos columnas y filas más bajas para que entren todos.
    juce::Rectangle<float> presetR (int k) const
    {
        const int n = std::max (1, (int) inGenre().size());
        const float avail = (float) getHeight() - 30.0f - 44.0f, x0 = 230.0f, w = (float) getWidth() - 250.0f;
        const int cols = n * 50.0f > avail ? 2 : 1, rows = (n + cols - 1) / cols;
        const float h = std::min (50.0f, avail / (float) rows), cw = w / (float) cols;
        return { x0 + cw * (float) (k / rows), 30.0f + h * (float) (k % rows), cw - (cols > 1 ? 8.0f : 0.0f), h - 4.0f };
    }
    void paint (juce::Graphics& g) override
    {
        section (g, { 10, 6, 200, 16 }, "Genre");
        section (g, { 230, 6, (float) getWidth() - 250, 16 }, genre >= 0 && genre < genres.size() ? genres[genre] : juce::String());
        for (int i = 0; i < genres.size(); ++i)
        {
            auto r = genreR (i).reduced (0, 1);
            const bool on = i == genre;
            g.setColour (on ? col::cell2 : juce::Colours::transparentBlack); g.fillRoundedRectangle (r, 3.0f);
            if (on) { g.setColour (col::rose); g.fillRect (r.getX(), r.getY() + 3, 2.5f, r.getHeight() - 6); }
            g.setColour (on ? col::bone : col::boneDim); text (g, genres[i], r.withTrimmedLeft (10), juce::Justification::centredLeft, 15.5f, on);
        }
        const auto v = inGenre(); const auto& ps = ctx.proc.getPresets(); const int cp = ctx.proc.getCurrentProgram();
        for (int k = 0; k < (int) v.size(); ++k)
        {
            auto r = presetR (k).reduced (0, 2);
            const auto& p = ps[(size_t) v[(size_t) k]];
            const bool on = v[(size_t) k] == cp;
            g.setColour (on ? col::roseDim.withAlpha (0.55f) : col::cell); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (on ? col::rose : col::line); g.drawRoundedRectangle (r, 4.0f, 1.0f);
            auto tr = r.reduced (12, 4);
            g.setColour (col::bone); text (g, p.name, tr.removeFromTop (20), juce::Justification::centredLeft, 17.0f, true);
            const auto tempo = p.data.getProperty ("params", {}).getProperty ("tempo", 0);
            g.setColour (col::boneDim); text (g, p.description.isNotEmpty() ? p.description : juce::String ("Your preset"), tr, juce::Justification::centredLeft, 13.5f);
            if ((float) tempo > 0) { g.setColour (col::boneFaint); text (g, juce::String ((int) tempo) + " BPM", r.reduced (12, 4).removeFromTop (20), juce::Justification::centredRight, 13.0f); }
        }
        if (v.empty()) { g.setColour (col::boneFaint); text (g, "No presets of your own yet: SAVE AS... keeps the kit, effects, patterns and song.", presetR (0), juce::Justification::centredLeft, 14.0f); }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < genres.size(); ++i) if (genreR (i).contains (e.position)) { genre = i; repaint(); return; }
        const auto v = inGenre();
        for (int k = 0; k < (int) v.size(); ++k)
            if (presetR (k).contains (e.position)) { ctx.proc.loadPreset (v[(size_t) k], loadWhat != 2, loadWhat != 1); repaint(); return; }
    }
private:
    Ctx& ctx; juce::StringArray genres; int genre = 0, loadWhat = 0;
    Chips what; Action saveBtn, delBtn, folderBtn;
    void save()
    {
        auto* w = new juce::AlertWindow ("Save preset", "Name of the new preset (kit, effects, patterns and song):", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", ctx.proc.currentPresetName.get());
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetsPage> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r) {
            if (safe != nullptr && r == 1 && w->getTextEditorContents ("name").trim().isNotEmpty()) { safe->ctx.proc.saveUserPreset (w->getTextEditorContents ("name").trim()); safe->rebuild(); safe->genre = safe->genres.indexOf ("User"); safe->repaint(); }
        }), true);
    }
};

// ── Ajustes: salidas, MIDI, reloj ──
class SetupPage : public juce::Component
{
public:
    explicit SetupPage (Ctx& c) : ctx (c)
    {
        auto chips = [&] (Chips& ch, const char* id, juce::StringArray items) { ch.bind (ctx.param (id)); ch.items = items; addAndMakeVisible (ch); };
        chips (routing, "routing", { "MAIN ONLY", "PER VOICE" });
        chips (noteMap, "note_map", { "GM DRUMS", "CHROMATIC 36-51" });
        chips (swingRes, "swing_res", { "8THS", "16THS" });
        sync.label = "Follow host transport and tempo"; sync.bind (ctx.param ("sync"));
        midiOut.label = "Send the sequencer as MIDI"; midiOut.bind (ctx.param ("midi_out"));
        for (auto* t : { &sync, &midiOut }) addAndMakeVisible (*t);
        auto k = [&] (Knob& kn, const char* id, const char* label) { kn.bind (ctx.param (id)); kn.label = label; addAndMakeVisible (kn); };
        k (midiCh, "midi_ch", "Channel"); k (gate, "midi_gate", "Gate"); k (accVel, "accent_vel", "Accent at"); k (seed, "seed", "Chance seed"); k (accent, "accent", "Accent");
        gate.setTooltip ("MIDI out gate: how long each note lasts (2-1000 ms), or until the same note plays again.");
        seed.setTooltip ("The seed of the probability: the same seed plays the same chances every time.");
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto left = r.removeFromLeft (r.getWidth() / 2 - 20); r.removeFromLeft (40);
        left.removeFromTop (24); sync.setBounds (left.removeFromTop (30).removeFromLeft (330));
        left.removeFromTop (34); swingRes.setBounds (left.removeFromTop (30).removeFromLeft (240));
        left.removeFromTop (30);
        auto kr = left.removeFromTop (92); accent.setBounds (kr.removeFromLeft (90)); seed.setBounds (kr.removeFromLeft (90));
        left.removeFromTop (34); routing.setBounds (left.removeFromTop (30).removeFromLeft (300));
        r.removeFromTop (24); midiOut.setBounds (r.removeFromTop (30).removeFromLeft (300));
        r.removeFromTop (14);
        auto mr = r.removeFromTop (92); midiCh.setBounds (mr.removeFromLeft (90)); gate.setBounds (mr.removeFromLeft (90)); accVel.setBounds (mr.removeFromLeft (90));
        r.removeFromTop (34); noteMap.setBounds (r.removeFromTop (30).removeFromLeft (340));
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (10, 6).toFloat();
        auto left = r.removeFromLeft (r.getWidth() / 2 - 20); r.removeFromLeft (40);
        section (g, left.withHeight (16), "Clock");
        section (g, { left.getX(), (float) swingRes.getY() - 20, left.getWidth(), 16 }, "Swing on");
        section (g, { left.getX(), (float) accent.getY() - 20, left.getWidth(), 16 }, "Accent and chance");
        section (g, { left.getX(), (float) routing.getY() - 20, left.getWidth(), 16 }, "Outputs");
        g.setColour (col::boneFaint); g.setFont (font (13.5f));
        g.drawFittedText ("Per voice: each voice goes to its bus (Kick, Snare/Clap, Hats/Cymbals, Toms, Perc, FX; change it in MIXER) when the host enables those outputs. "
                          "The sends and the master stay on Main. A bus the host did not enable folds into Main.",
                          juce::Rectangle<float> (left.getX(), (float) routing.getBottom() + 8, left.getWidth(), 60).toNearestInt(), juce::Justification::topLeft, 4);
        section (g, r.withHeight (16), "MIDI");
        section (g, { r.getX(), (float) noteMap.getY() - 20, r.getWidth(), 16 }, "Note map (in and out)");
        g.setColour (col::boneFaint);
        g.drawFittedText ("GM: 36 kick, 38 snare, 39 clap, 37 rim, 42/46 hats, 49 crash, 51 ride, 41/45/48/50 toms, 56 cowbell, 75 clave, 54 tambourine, 52 FX. "
                          "Chromatic: notes 36 to 51, one per pad.", juce::Rectangle<float> (r.getX(), (float) noteMap.getBottom() + 8, r.getWidth(), 60).toNearestInt(), juce::Justification::topLeft, 4);
        g.setColour (col::boneFaint); text (g, "Pallor " PALLOR_VERSION "  /  Audio Tools  /  free and open source (AGPL-3.0)", { r.getX(), (float) getHeight() - 26, r.getWidth(), 18 }, juce::Justification::centredRight, 12.5f, false, 0.06f);
    }
private:
    Ctx& ctx; Chips routing, noteMap, swingRes; Toggle sync, midiOut; Knob midiCh, gate, accVel, seed, accent;
};

} // namespace dh::pal::ui
