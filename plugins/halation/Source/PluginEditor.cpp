#include "PluginEditor.h"

using namespace dh::sp;
using namespace dh::sp::ui;

namespace {
struct DetailDef { const char* id; const char* title; int kind; };
// Los ajustes de cada módulo (y del panel More), en el orden en que aparecen.
const std::map<juce::String, std::vector<DetailDef>>& details()
{
    static const std::map<juce::String, std::vector<DetailDef>> d {
        { "freeze",    { { "freeze_mode", "Bands", 1 } } },
        { "shimmer",   { { "shimmer", "Amount", 0 }, { "interval", "Interval", 1 } } },
        { "reverse",   { { "reverse", "Amount", 0 }, { "reverse_div", "Length", 1 } } },
        { "grains",    { { "grains", "Amount", 0 }, { "grain_ms", "Grain size", 0 } } },
        { "spectral",  { { "spectral", "Smear", 0 } } },
        { "resonator", { { "resonance", "Amount", 0 }, { "res_root", "Root", 0 }, { "chord", "Chord", 1 }, { "res_midi", "Follow MIDI", 2 } } },
        { "stutter",   { { "stutter", "Chance", 0 }, { "stutter_div", "Length", 1 } } },
        { "breathe",   { { "breathe_mode", "Mode", 1 }, { "breathe_depth", "Depth", 0 }, { "breathe_rate", "Rate", 0 } } },
        { "duck",      { { "duck", "Amount", 0 } } },
        { "gate",      { { "gate", "Amount", 0 }, { "gate_hold", "Hold", 0 } } },
        { "more",      { { "predelay", "Pre-delay", 0 }, { "diffusion", "Diffusion", 0 }, { "lowcut", "Low cut", 0 }, { "bass", "Bass decay", 0 },
                         { "mod_depth", "Modulation", 0 }, { "mod_rate", "Mod rate", 0 }, { "width", "Width", 0 }, { "wear", "Tape wear", 0 },
                         { "env_amount", "Envelope", 0 }, { "env_target", "Env. target", 1 }, { "elastic", "Tape glide", 2 }, { "out_gain", "Output", 0 } } },
    };
    return d;
}
const char* noteName (int n) { static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return names[((n % 12) + 12) % 12]; }
}

juce::String SpaceEditor::moduleTitle (const juce::String& m)
{
    static const std::map<juce::String, juce::String> t { { "freeze", "Freeze" }, { "shimmer", "Shimmer" }, { "reverse", "Reverse" }, { "grains", "Grains" },
        { "spectral", "Spectral" }, { "resonator", "Resonator" }, { "stutter", "Stutter" }, { "breathe", "Breathe" }, { "duck", "Duck" }, { "gate", "Gate" } };
    auto it = t.find (m); return it != t.end() ? it->second : m;
}
juce::String SpaceEditor::moduleTip (const juce::String& m)
{
    static const std::map<juce::String, juce::String> t {
        { "freeze", "Freeze: the space holds what it has, forever. Choose All, or freeze only the lows, mids or highs and let the rest move." },
        { "shimmer", "Shimmer: every repeat is lifted by an interval (octave, fifth, a chord), so the tail rises like a choir." },
        { "reverse", "Reverse: the tail plays backwards in slices, swelling up into the next beat." },
        { "grains", "Grains: fragments of the tail are scattered back into the space, forward and backward." },
        { "spectral", "Spectral: the tail is smeared in time and its phases scattered, into a still, glassy cloud." },
        { "resonator", "Resonator: the tail rings on a note or chord. With Follow MIDI, it rings on the notes you hold." },
        { "stutter", "Stutter: on the beat, a slice of the output is caught and repeated." },
        { "breathe", "Breathe: an impossible room. Breathe moves the walls in and out; Collapse closes them while you play." },
        { "duck", "Duck: the effect steps back while you play and blooms in the gaps." },
        { "gate", "Gate: the tail stops dead after the sound, like the big drums of the 1980s." },
    };
    auto it = t.find (m); return it != t.end() ? it->second : juce::String();
}

SpaceEditor::SpaceEditor (SpaceProcessor& p) : AudioProcessorEditor (&p), proc (p), room (p)
{
    setLookAndFeel (&lnf);
    content.painter = [this] (juce::Graphics& g) { paintBackdrop (g); };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    content.addAndMakeVisible (room);
    content.addMouseListener (this, true);

    space.setSliderStyle (juce::Slider::LinearHorizontal);
    space.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    space.getProperties().set ("space", true);
    spaceAtt = std::make_unique<SA> (proc.apvts, "space", space);
    space.setTooltip ("Delay to Space: from separate echoes on the beat, through echoes that melt together, to a room. The tail keeps its length; only how it spreads changes.");
    content.addAndMakeVisible (space);

    setupKnob (time, "time_ms", "Time", "Time between echoes, in milliseconds (turn Sync on to follow the tempo).");
    setupKnob (division, "division", "Time", "Time between echoes, as a note value of the host tempo.");
    setupKnob (size, "size", "Size", "How big the room is. The walls move.");
    setupKnob (length, "length", "Length", "How long the sound lasts: repeats in a delay, seconds in a room. All the way up, it never ends.");
    setupKnob (tone, "tone", "Tone", "Dark to bright: how fast the highs fade compared with the rest.");
    setupKnob (mix, "mix", "Mix", "How much of the effect you hear against the dry sound.");
    for (auto* b : { &syncBtn, &pingBtn }) { b->setClickingTogglesState (true); content.addAndMakeVisible (b); }
    syncAtt = std::make_unique<BA> (proc.apvts, "sync", syncBtn);
    pingAtt = std::make_unique<BA> (proc.apvts, "pingpong", pingBtn);
    syncBtn.setTooltip ("Sync: the echo time follows the host tempo.");
    pingBtn.setTooltip ("Ping-pong: the echoes bounce between left and right.");
    pattern.addItemList (params::patternNames(), 1);
    patternAtt = std::make_unique<CA> (proc.apvts, "pattern", pattern);
    pattern.setTooltip ("Pattern: how the echoes are spread over the beat (dotted, triplet, rain, gallop...).");
    content.addAndMakeVisible (pattern);

    auto choiceRow = [this] (juce::OwnedArray<juce::TextButton>& arr, const juce::StringArray& names, const juce::String& id, const juce::String& tip) {
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = arr.add (new juce::TextButton (names[i]));
            b->setClickingTogglesState (false);
            b->onClick = [this, id, i] { proc.setValue (id, (float) i); };
            b->setTooltip (tip);
            content.addAndMakeVisible (b);
        }
    };
    choiceRow (charBtns, params::characters(), "character", "Character of the echoes: clean digital, tape (wobble, warmth, wear), a dark bucket-brigade chip, or a spring tank.");
    choiceRow (roomBtns, params::roomTypes(), "room", "The kind of room the echoes melt into: a small room, a hall, a bright plate or a stone chamber.");

    for (auto& m : params::modules())
    {
        auto* b = modBtns.add (new juce::TextButton (moduleTitle (m)));
        b->setTooltip (moduleTip (m));
        b->onClick = [this, m] {
            // Un clic elige el módulo (y muestra sus ajustes); si ya estaba elegido, lo prende o lo apaga.
            const bool isOn = proc.value ("on_" + m) > 0.5f;
            if (selected == m || ! isOn) proc.setValue ("on_" + m, isOn ? 0.0f : 1.0f);
            selected = m; moreBtn.setToggleState (false, juce::dontSendNotification); buildDetail();
        };
        content.addAndMakeVisible (b);
    }
    moreBtn.setClickingTogglesState (true);
    moreBtn.onClick = [this] { selected = moreBtn.getToggleState() ? "more" : "shimmer"; buildDetail(); };
    moreBtn.setTooltip ("More: pre-delay, diffusion, low cut, bass decay, modulation, width, tape wear, envelope and output.");
    content.addAndMakeVisible (moreBtn);

    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &storeB, &manualBtn }) content.addAndMakeVisible (b);
    prevBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.loadPreset ((proc.getCurrentProgram() - 1 + n) % n); };
    nextBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.loadPreset ((proc.getCurrentProgram() + 1) % n); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    storeB.onClick = [this] { proc.storeSceneB(); };
    storeB.setTooltip ("Store B: remember the current settings as scene B. Then Morph glides from your knobs (A) to B.");
    manualBtn.onClick = [this] { showManual (manual == nullptr); };
   #if ! DH_HAS_MANUAL
    manualBtn.setVisible (false);
   #endif
    morph.setSliderStyle (juce::Slider::LinearHorizontal);
    morph.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    morph.setColour (juce::Slider::trackColourId, col::plaster); morph.setColour (juce::Slider::backgroundColourId, col::panel2);
    morph.setColour (juce::Slider::thumbColourId, col::plaster);
    morphAtt = std::make_unique<SA> (proc.apvts, "morph", morph);
    morph.setTooltip ("Morph: from A (the knobs) to scene B. Automate it for slow changes of space.");
    content.addAndMakeVisible (morph);
    proc.onPresetChanged = [this] { presetBtn.setButtonText (proc.currentPresetName); buildDetail(); content.repaint(); };
    presetBtn.setButtonText (proc.currentPresetName);

    buildDetail();
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.6), (int) (baseH * 1.6));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.9), (int) (baseH * 0.9));
    startTimerHz (15);

    // Capturas sin pantalla (manual, web): HALATION_SNAPSHOT=/ruta.png, HALATION_PRESET=n, HALATION_WAIT=s,
    // HALATION_MODULE=nombre (o "more"), HALATION_MANUAL=1, HALATION_LATE=id=valor (se aplica a mitad de la espera:
    // por ejemplo prender Freeze cuando la sala ya tiene sonido). La entrada de prueba la toma el procesador de
    // HALATION_DEMO_INPUT.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("HALATION_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
        const int preset = env ("HALATION_PRESET", "-1").getIntValue(), wait = env ("HALATION_WAIT", "6").getIntValue();
        const auto module = env ("HALATION_MODULE", ""); const bool man = env ("HALATION_MANUAL", "0").getIntValue() != 0;
        juce::Component::SafePointer<SpaceEditor> safe (this);
        juce::Timer::callAfterDelay (300, [safe, preset, module, man] {
            if (safe == nullptr) return;
            if (preset >= 0) safe->proc.loadPreset (preset);
            if (module.isNotEmpty()) { safe->selected = module; safe->moreBtn.setToggleState (module == "more", juce::dontSendNotification); safe->buildDetail(); }
            if (man) safe->showManual (true);
        });
        if (const auto late = env ("HALATION_LATE", ""); late.containsChar ('='))
            juce::Timer::callAfterDelay (300 + wait * 500, [safe, late] {
                if (safe != nullptr) safe->proc.setValue (late.upToFirstOccurrenceOf ("=", false, false), late.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
            });
        juce::Timer::callAfterDelay (300 + wait * 1000, [safe, snap, man] {
            if (safe == nullptr) return;
            juce::Component& shot = man && safe->manual != nullptr ? (juce::Component&) *safe : (juce::Component&) safe->content;
            const auto img = shot.createComponentSnapshot (shot.getLocalBounds(), true, man ? 2.0f * (float) baseW / (float) safe->getWidth() : 2.0f);
            juce::File f (snap); f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

SpaceEditor::~SpaceEditor() { proc.onPresetChanged = nullptr; manual.reset(); setLookAndFeel (nullptr); }

void SpaceEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& title, const juce::String& tip)
{
    k.id = id; k.title = title;
    k.s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.att = std::make_unique<SA> (proc.apvts, id, k.s);
    k.s.setTooltip (title + ": " + tip);
    k.s.onValueChange = [this] { content.repaint (knobsR); };
    content.addAndMakeVisible (k.s);
}

juce::String SpaceEditor::valueText (const juce::String& id) const
{
    const float v = proc.value (id);
    if (id == "time_ms") return juce::String (juce::roundToInt (v)) + " ms";
    if (id == "division") return params::divisions()[(size_t) juce::jlimit (0, 15, (int) v)].name;
    if (id == "size") return juce::String (juce::roundToInt (v)) + " %";
    if (id == "length")
    {
        const auto ep = proc.currentEngineParams();
        if (ep.net.decayS >= 100.0f) return "Endless";
        if (ep.net.space < 0.5f) return juce::String (std::max (1, (int) std::ceil (ep.net.decayS * 1000.0f / std::max (1.0f, ep.net.timeMs)))) + " repeats";
        return ep.net.decayS < 10.0f ? juce::String (ep.net.decayS, 1) + " s" : juce::String (juce::roundToInt (ep.net.decayS)) + " s";
    }
    if (id == "tone") return v < 33 ? "Dark" : v < 66 ? "Warm" : "Bright";
    if (id == "res_root") return juce::String (noteName (juce::roundToInt (v))) + juce::String (juce::roundToInt (v) / 12 - 1);
    if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (proc.apvts.getParameter (id)))
    {
        const auto r = pf->getNormalisableRange();
        const juce::String num = r.end - r.start > 10.0f ? juce::String (juce::roundToInt (v)) : juce::String (v, 2);
        return num + (pf->getLabel().isNotEmpty() ? " " + pf->getLabel() : juce::String());
    }
    if (auto* pr = proc.apvts.getParameter (id)) return pr->getCurrentValueAsText();
    return {};
}

void SpaceEditor::buildDetail()
{
    detail.clear();
    auto it = details().find (selected);
    if (it == details().end()) return;
    for (auto& d : it->second)
    {
        auto c = std::make_unique<Control>();
        c->id = d.id; c->title = d.title; c->kind = d.kind;
        if (d.kind == 0)
        {
            c->s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            c->s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            if (juce::String (d.id) == "env_amount") c->s->getProperties().set ("bipolar", true);
            c->sa = std::make_unique<SA> (proc.apvts, d.id, *c->s);
            c->s->onValueChange = [this] { content.repaint (detailR); };
            content.addAndMakeVisible (*c->s);
        }
        else if (d.kind == 1)
        {
            c->c = std::make_unique<juce::ComboBox>();
            if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (d.id))) c->c->addItemList (pc->choices, 1);
            c->ca = std::make_unique<CA> (proc.apvts, d.id, *c->c);
            content.addAndMakeVisible (*c->c);
        }
        else
        {
            c->b = std::make_unique<juce::TextButton> (d.title);
            c->b->setClickingTogglesState (true);
            c->ba = std::make_unique<BA> (proc.apvts, d.id, *c->b);
            content.addAndMakeVisible (*c->b);
        }
        detail.push_back (std::move (c));
    }
    layoutDetail();
    content.repaint();
}

void SpaceEditor::layoutDetail()
{
    const int n = (int) detail.size();
    if (n == 0) return;
    const bool more = selected == "more";
    const int x0 = detailR.getX() + (more ? 10 : 260), w = more ? (detailR.getWidth() - 20) / n : 120;
    for (int i = 0; i < n; ++i)
    {
        auto& c = *detail[(size_t) i];
        const auto cell = juce::Rectangle<int> (x0 + i * w, detailR.getY() + 6, w, detailR.getHeight() - 10);
        if (c.s) c.s->setBounds (cell.withSizeKeepingCentre (more ? 46 : 54, more ? 46 : 54).translated (0, -6));
        if (c.c) c.c->setBounds (cell.withSizeKeepingCentre (w - 14, 30).translated (0, -6));
        if (c.b) c.b->setBounds (cell.withSizeKeepingCentre (w - 18, 30).translated (0, -6));
    }
}

void SpaceEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    std::map<juce::String, juce::PopupMenu> sub; juce::StringArray order;
    const auto& ps = proc.getPresets();
    for (int i = 0; i < (int) ps.size(); ++i)
    {
        if (! order.contains (ps[(size_t) i].category)) order.add (ps[(size_t) i].category);
        sub[ps[(size_t) i].category].addItem (i + 1, ps[(size_t) i].name, true, i == proc.getCurrentProgram());
    }
    for (auto& c : order) menu.addSubMenu (c, sub[c]);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [this] (int r) { if (r > 0) proc.loadPreset (r - 1); });
}

void SpaceEditor::showManual (bool on)
{
   #if DH_HAS_MANUAL
    if (! on) { manual.reset(); manualBtn.setToggleState (false, juce::dontSendNotification); return; }
    dh::manual::Theme t;
    t.bg = col::bg; t.panel = col::panel; t.text = col::ink; t.muted = col::inkDim; t.accent = col::warm; t.line = col::line;
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SpaceEditor> (this)] { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*m);
    m->setBounds (getLocalBounds());
    m->grabKeyboardFocus();
    manual = std::move (m);
    manualBtn.setToggleState (true, juce::dontSendNotification);
   #else
    juce::ignoreUnused (on);
   #endif
}

void SpaceEditor::timerCallback()
{
    const bool sync = proc.value ("sync") > 0.5f;
    time.s.setVisible (! sync); division.s.setVisible (sync);
    const int ch = (int) proc.value ("character"), rm = (int) proc.value ("room");
    for (int i = 0; i < charBtns.size(); ++i) charBtns[i]->setToggleState (i == ch, juce::dontSendNotification);
    for (int i = 0; i < roomBtns.size(); ++i) roomBtns[i]->setToggleState (i == rm, juce::dontSendNotification);
    for (int i = 0; i < modBtns.size(); ++i) modBtns[i]->setToggleState (proc.value ("on_" + params::modules()[i]) > 0.5f, juce::dontSendNotification);
    storeB.setButtonText (proc.hasSceneB() ? "Store B again" : "Store B");
    content.repaint (knobsR); content.repaint (detailR);
}

void SpaceEditor::mouseMove (const juce::MouseEvent& e)
{
    juce::String t;
    for (auto* c = e.eventComponent; c != nullptr && c != &content; c = c->getParentComponent())
        if (auto* tc = dynamic_cast<juce::SettableTooltipClient*> (c)) { t = tc->getTooltip(); if (t.isNotEmpty()) break; }
    if (t != help) { help = t; content.repaint (helpR); }
}

void SpaceEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void SpaceEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    const int y = (barH - 32) / 2;
    prevBtn.setBounds (250, y, 32, 32); presetBtn.setBounds (286, y, 230, 32); nextBtn.setBounds (520, y, 32, 32);
    storeB.setBounds (640, y, 112, 32);
    morph.setBounds (790, y, 170, 32);
    manualBtn.setBounds (baseW - 14 - 96, y, 96, 32);

    room.setBounds (0, barH, baseW, 380);
    space.setBounds (110, barH + 392, baseW - 220, 44);

    knobsR = { 0, barH + 444, baseW, 128 };
    const int kw = 96, kx = 24, ky = knobsR.getY() + 4;
    time.s.setBounds (kx, ky, 70, 70); division.s.setBounds (kx, ky, 70, 70);
    int x = kx;
    for (auto* k : { &time, &size, &length, &tone, &mix }) { if (k != &time) k->s.setBounds (x, ky, 70, 70); x += kw; }

    // Patrón y ping-pong (lo del delay), después carácter y sala.
    const int px = 520;
    pattern.setBounds (px, ky + 20, 140, 30); syncBtn.setBounds (px, ky + 76, 60, 30); pingBtn.setBounds (px + 64, ky + 76, 76, 30);
    const int cx = 680, cw = (baseW - cx - 24) / 4;
    for (int i = 0; i < charBtns.size(); ++i) charBtns[i]->setBounds (cx + i * cw, ky + 20, cw - 6, 30);
    for (int i = 0; i < roomBtns.size(); ++i) roomBtns[i]->setBounds (cx + i * cw, ky + 76, cw - 6, 30);

    const int my = barH + 586, mw = (baseW - 48 - 90) / modBtns.size();
    for (int i = 0; i < modBtns.size(); ++i) modBtns[i]->setBounds (24 + i * mw, my, mw - 6, 34);
    moreBtn.setBounds (baseW - 24 - 84, my, 84, 34);
    detailR = { 24, my + 42, baseW - 48, 104 };
    layoutDetail();
    helpR = { 0, baseH - 30, baseW, 30 };
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void SpaceEditor::paintBackdrop (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // Barra superior.
    g.setColour (col::panel); g.fillRect (0, 0, baseW, barH);
    g.setColour (col::line); g.fillRect (0, barH - 1, baseW, 1);
    g.setColour (col::ink); g.setFont (bold (27.0f));
    g.drawText ("Halation", 20, 0, 150, barH, juce::Justification::centredLeft);
    g.setColour (col::inkFaint); g.setFont (regular (13.0f));
    g.drawText ("Audio Tools", 136, 3, 100, barH, juce::Justification::centredLeft);
    g.setColour (col::inkDim); g.setFont (regular (14.0f));
    g.drawText ("A", 772, 0, 14, barH, juce::Justification::centred);
    g.drawText ("B", 964, 0, 14, barH, juce::Justification::centred);

    // Delay ↔ Space.
    g.setColour (col::ink); g.setFont (bold (18.0f));
    g.drawText ("Delay", 24, barH + 392, 80, 44, juce::Justification::centredLeft);
    g.drawText ("Space", baseW - 104, barH + 392, 80, 44, juce::Justification::centredRight);

    // Perillas: título y valor abajo.
    const int kw = 96, kx = 24, ky = knobsR.getY() + 4;
    int x = kx;
    for (auto* k : { &time, &size, &length, &tone, &mix })
    {
        const bool sync = proc.value ("sync") > 0.5f;
        const juce::String id = k == &time && sync ? juce::String ("division") : k->id;
        g.setColour (col::inkDim); g.setFont (regular (13.5f));
        g.drawText (k->title, x - 13, ky + 72, kw, 18, juce::Justification::centred);
        g.setColour (col::ink); g.setFont (bold (15.0f));
        g.drawText (valueText (id), x - 13, ky + 90, kw, 20, juce::Justification::centred);
        x += kw;
    }
    g.setColour (col::inkDim); g.setFont (regular (13.5f));
    g.drawText ("Echo pattern", 520, ky, 160, 18, juce::Justification::centredLeft);
    g.drawText ("Character", 680, ky, 200, 18, juce::Justification::centredLeft);
    g.drawText ("Room", 680, ky + 56, 200, 18, juce::Justification::centredLeft);

    // Panel de detalle.
    g.setColour (col::panel); g.fillRoundedRectangle (detailR.toFloat(), 10.0f);
    g.setColour (col::line); g.drawRoundedRectangle (detailR.toFloat().reduced (0.5f), 10.0f, 1.0f);
    const bool more = selected == "more";
    if (! more)
    {
        g.setColour (col::ink); g.setFont (bold (19.0f));
        g.drawText (moduleTitle (selected), detailR.getX() + 18, detailR.getY() + 12, 230, 24, juce::Justification::centredLeft);
        const bool on = proc.value ("on_" + selected) > 0.5f;
        g.setColour (on ? col::warm : col::inkFaint); g.setFont (regular (13.5f));
        g.drawText (on ? "On. Click the button again to turn it off." : "Off. Click the button again to turn it on.", detailR.getX() + 18, detailR.getY() + 36, 240, 18, juce::Justification::centredLeft);
        g.setColour (col::inkDim); g.setFont (regular (13.5f));
        auto tipText = moduleTip (selected).fromFirstOccurrenceOf (": ", false, false);
        tipText = tipText.substring (0, 1).toUpperCase() + tipText.substring (1);
        g.drawFittedText (tipText, juce::Rectangle<int> (detailR.getX() + 18, detailR.getY() + 56, 230, 44), juce::Justification::topLeft, 3, 0.9f);
    }
    for (auto& c : detail)
    {
        juce::Rectangle<int> r;
        if (c->s) r = c->s->getBounds(); else if (c->c) r = c->c->getBounds(); else if (c->b) r = c->b->getBounds();
        if (c->kind != 2)
        {
            g.setColour (col::inkDim); g.setFont (regular (12.5f));
            g.drawText (c->title, r.getCentreX() - 60, r.getBottom() + 2, 120, 15, juce::Justification::centred);
            if (c->kind == 0) { g.setColour (col::ink); g.setFont (bold (13.0f)); g.drawText (valueText (c->id), r.getCentreX() - 60, r.getBottom() + 16, 120, 16, juce::Justification::centred); }
        }
    }

    // Línea de ayuda.
    g.setColour (col::panel); g.fillRect (helpR);
    g.setColour (col::line); g.fillRect (helpR.getX(), helpR.getY(), helpR.getWidth(), 1);
    g.setColour (help.isNotEmpty() ? col::ink : col::inkFaint); g.setFont (regular (14.0f));
    g.drawText (help.isNotEmpty() ? help : juce::String ("Point at any control to see what it does. Turn Delay to Space slowly and watch the room."),
                helpR.reduced (16, 0), juce::Justification::centredLeft);
}
