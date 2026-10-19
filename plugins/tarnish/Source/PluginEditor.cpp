#include "PluginEditor.h"

using namespace dh::tn;
using namespace dh::tn::ui;

namespace {
struct DetailDef { const char* id; const char* title; int kind; };
// Los ajustes de cada pestaña, en el orden en que aparecen. "machine" depende de la máquina elegida.
const std::vector<DetailDef>& tabDefs (const juce::String& tab, int machine)
{
    static const std::vector<DetailDef> damage { { "dropouts", "Dropouts", 0 }, { "crackle", "Crackle", 0 }, { "scratches", "Scratches", 1 }, { "scrape", "Scrape", 0 },
                                                 { "hum", "Hum", 0 }, { "azimuth", "Azimuth", 0 }, { "print", "Print-through", 0 } };
    static const std::vector<DetailDef> tone { { "lowcut", "Low cut", 0 }, { "highcut", "High cut", 0 }, { "nr", "Noise reduction", 1 }, { "mistrack", "Mistrack", 0 } };
    static const std::vector<DetailDef> crush { { "bits", "Bits", 0 }, { "rate", "Sample rate", 0 }, { "crush_mode", "Mode", 1 }, { "jitter", "Jitter", 0 }, { "dither", "Dither", 2 } };
    static const std::vector<DetailDef> speed { { "varispeed", "Varispeed", 0 }, { "stop_time", "Stop time", 0 }, { "stop_sync", "Stop sync", 1 }, { "stop_curve", "Stop curve", 0 },
                                                { "start_time", "Start time", 0 }, { "stop_midi", "MIDI stops", 2 }, { "stop_transport", "Host stops", 2 } };
    static const std::vector<DetailDef> vinyl { { "groove", "Groove position", 0 }, { "groove_follow", "Follow song", 2 } };
    static const std::vector<DetailDef> radio { { "compress", "Station", 0 }, { "fading", "Fading", 0 }, { "tuning", "Tuning", 0 }, { "whistle", "Whistle", 0 }, { "detector", "Detector", 0 } };
    static const std::vector<DetailDef> phone { { "frameloss", "Frame loss", 0 }, { "codec", "Codec", 0 } };
    static const std::vector<DetailDef> tape { { "dropouts", "Dropouts", 0 }, { "scrape", "Scrape", 0 }, { "azimuth", "Azimuth", 0 }, { "print", "Print-through", 0 } };
    static const std::vector<DetailDef> more { { "out", "Output", 0 }, { "auto_gain", "Auto gain", 2 }, { "wobble", "Stereo wobble", 2 }, { "timeline", "Lock to timeline", 2 }, { "quality", "Quality", 1 } };
    if (tab == "damage") return damage;
    if (tab == "tone") return tone;
    if (tab == "crush") return crush;
    if (tab == "speed") return speed;
    if (tab == "more") return more;
    return machine == kVinyl ? vinyl : machine == kRadio ? radio : machine == kPhone ? phone : tape;
}
const juce::StringArray tabIds { "damage", "tone", "crush", "speed", "machine", "more" };
const juce::StringArray tabTitles { "Damage", "Tone", "Crush", "Speed", "Machine", "More" };
}

juce::String TarnishEditor::tip (const juce::String& id)
{
    static const std::map<juce::String, juce::String> t {
        { "drive", "Drive: how hard the signal hits the tape. Up to the reference it stays clean; past it, the tape saturates and the highs close first." },
        { "saturation", "Saturation: how early the tape reaches its ceiling. Low is a hot, roomy tape; high saturates sooner and more softly." },
        { "bias", "Bias: the tape's alignment. Centre is clean. Under-bias opens the magnetic loop (grit on quiet notes); over-bias is cleaner and duller." },
        { "wow", "Wow: slow speed wobble from the reels and the pinch roller, as a weighted percentage (0.03 % is a studio machine, 1 % a worn portable)." },
        { "flutter", "Flutter: fast speed wobble from the capstan and the motor, weighted." },
        { "wear", "Wear: the tape lifts off the head (highs fade), oxide sheds, damaged stretches appear. On vinyl, a dirtier record." },
        { "noise", "Noise: the machine's own hiss, surface noise or line noise. Half is its usual level; full is 20 dB more." },
        { "mix", "Mix: how much of the machine you hear against the dry signal (kept in time)." },
        { "dropouts", "Dropouts: moments when dust lifts the tape off the head. The highs go first, as on a real tape. Watch them reach the play head." },
        { "crackle", "Crackle: on vinyl, crackle, clicks and pops; on the radio, static." },
        { "scratches", "Scratches: pops at a fixed place on the record, once per turn." },
        { "scrape", "Scrape: modulation noise, the fuzz tape adds around sustained notes." },
        { "hum", "Hum: mains hum from the machine (60 Hz, 50 Hz on radio and phone, the field rate on video)." },
        { "azimuth", "Azimuth: the play head is tilted. Highs drop and the right channel lags, so the mono sum gets dull and phasey." },
        { "print", "Print-through: a faint post-echo from the layer of tape one turn away on the reel." },
        { "lowcut", "Low cut: removes lows after the machine." },
        { "highcut", "High cut: removes highs after the machine." },
        { "nr", "Noise reduction: a generic sliding-shelf compander. Mild is about 10 dB, strong about 20 dB at the top." },
        { "mistrack", "Mistrack: the decoder hears a different level than the encoder. The highs breathe, like an old deck." },
        { "bits", "Bits: quantisation, from 24 (off) down to 1." },
        { "rate", "Sample rate: hold the signal at a lower rate. All the way up is off." },
        { "crush_mode", "Mode: Clean filters before and after and smooths the steps; Raw keeps the aliasing." },
        { "jitter", "Jitter: the sample clock wavers." },
        { "dither", "Dither: turns quantisation distortion into hiss." },
        { "varispeed", "Varispeed: the machine runs slower or faster, in semitones." },
        { "stop_time", "Stop time: how long the tape takes to stop." },
        { "stop_sync", "Stop sync: the stop time as a note value of the host tempo." },
        { "stop_curve", "Stop curve: from a straight stop (friction) to a long glide (inertia)." },
        { "start_time", "Start time: how long it takes to get back up to speed." },
        { "stop_midi", "MIDI stops: any held note stops the tape; release to start again." },
        { "stop_transport", "Host stops: the tape stops when the host stops playing (in hosts that keep processing)." },
        { "groove", "Groove position: outer edge to the label. Inner grooves lose highs and distort more." },
        { "groove_follow", "Follow song: the stylus moves inward as the song plays, like a side of a record." },
        { "compress", "Station: how hard the station's compressor squeezes the audio." },
        { "fading", "Fading: the signal comes and goes, the lows and highs on their own." },
        { "tuning", "Tuning: off centre adds a whistle and a lopsided, distorted sound." },
        { "whistle", "Whistle: two stations too close together." },
        { "detector", "Detector: a slow detector can't follow fast drops (diagonal clipping)." },
        { "frameloss", "Frame loss: 20 ms frames go missing in bursts, as on a weak mobile call." },
        { "codec", "Codec: fewer bits on the line, a harsher, grittier voice." },
        { "out", "Output: level after the machine." },
        { "auto_gain", "Auto gain: undo the level change of Drive, so you judge the sound and not the volume." },
        { "wobble", "Stereo wobble: each channel wobbles on its own. Not physical, but wide." },
        { "timeline", "Lock to timeline: dropouts, crackle and wow follow the song position, so a bounce sounds like playback." },
        { "quality", "Quality: Eco halves the CPU of the magnetic model, with a less exact loop at high drive." },
        { "machine", "Machine: what the sound goes through." },
    };
    auto it = t.find (id); return it != t.end() ? it->second : juce::String();
}

TarnishEditor::TarnishEditor (TarnishProcessor& p) : AudioProcessorEditor (&p), proc (p), deck (p)
{
    setLookAndFeel (&lnf);
    content.painter = [this] (juce::Graphics& g) { paintBackdrop (g); };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    content.addAndMakeVisible (deck);
    content.addMouseListener (this, true);

    const char* ids[] = { "drive", "saturation", "bias", "wow", "flutter", "wear", "noise", "mix" };
    const char* titles[] = { "Drive", "Saturation", "Bias", "Wow", "Flutter", "Wear", "Noise", "Mix" };
    for (int i = 0; i < 8; ++i) setupKnob (knobs[(size_t) i], ids[i], titles[i], tip (ids[i]));
    knobs[2].s.getProperties().set ("bipolar", true);

    for (int i = 0; i < 6; ++i)
    {
        auto* b = machineBtns.add (new juce::TextButton (params::machines()[i]));
        b->onClick = [this, i] { proc.setValue ("machine", (float) i); };
        b->setTooltip (tip ("machine"));
        content.addAndMakeVisible (b);
    }
    content.addAndMakeVisible (variant);
    eq.addItemList (params::eqs(), 1);
    eqAtt = std::make_unique<CA> (proc.apvts, "eq", eq);
    eq.setTooltip ("Studio EQ: IEC (CCIR) or NAB, which lifts the lows on playback and cuts them when recording.");
    content.addAndMakeVisible (eq);
    for (int i = 0; i < tabIds.size(); ++i)
    {
        auto* b = tabBtns.add (new juce::TextButton (tabTitles[i]));
        b->onClick = [this, i] { tab = tabIds[i]; buildDetail(); };
        content.addAndMakeVisible (b);
    }

    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &stopBtn, &manualBtn }) content.addAndMakeVisible (b);
    prevBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.loadPreset ((proc.getCurrentProgram() - 1 + n) % n); };
    nextBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n) proc.loadPreset ((proc.getCurrentProgram() + 1) % n); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    stopBtn.setClickingTogglesState (true);
    stopBtn.getProperties().set ("stop", true);
    stopAtt = std::make_unique<BA> (proc.apvts, "stop", stopBtn);
    stopBtn.setTooltip ("Stop: the tape slows to a halt (Stop time and curve in the Speed tab). Click again to start. A held MIDI note does the same.");
    manualBtn.onClick = [this] { showManual (manual == nullptr); };
   #if ! DH_HAS_MANUAL
    manualBtn.setVisible (false);
   #endif
    proc.onPresetChanged = [this] { presetBtn.setButtonText (proc.currentPresetName); buildDetail(); content.repaint(); };
    presetBtn.setButtonText (proc.currentPresetName);

    bindVariant();
    buildDetail();
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.6), (int) (baseH * 1.6));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.9), (int) (baseH * 0.9));
    startTimerHz (30);

    // Capturas sin pantalla (manual, web): TARNISH_SNAPSHOT=/ruta.png, TARNISH_PRESET=n, TARNISH_WAIT=s,
    // TARNISH_TAB=pestaña, TARNISH_MANUAL=1, TARNISH_LATE=id=valor (a mitad de la espera). La entrada de prueba la
    // toma el procesador de TARNISH_DEMO_INPUT.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("TARNISH_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
        const int preset = env ("TARNISH_PRESET", "-1").getIntValue(), wait = env ("TARNISH_WAIT", "6").getIntValue();
        const auto tb = env ("TARNISH_TAB", ""); const bool man = env ("TARNISH_MANUAL", "0").getIntValue() != 0;
        juce::Component::SafePointer<TarnishEditor> safe (this);
        juce::Timer::callAfterDelay (300, [safe, preset, tb, man] {
            if (safe == nullptr) return;
            if (preset >= 0) safe->proc.loadPreset (preset);
            if (tb.isNotEmpty()) { safe->tab = tb; safe->buildDetail(); }
            if (man) safe->showManual (true);
        });
        if (const auto late = env ("TARNISH_LATE", ""); late.containsChar ('='))
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

TarnishEditor::~TarnishEditor() { proc.onPresetChanged = nullptr; manual.reset(); setLookAndFeel (nullptr); }

void TarnishEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& title, const juce::String& t)
{
    k.id = id; k.title = title;
    k.s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.att = std::make_unique<SA> (proc.apvts, id, k.s);
    k.s.setTooltip (t);
    k.s.onValueChange = [this] { content.repaint (knobsR); };
    content.addAndMakeVisible (k.s);
}

// La lista de variante cambia con la máquina (velocidad, cinta, modo de video, revoluciones, banda, línea).
void TarnishEditor::bindVariant()
{
    const int m = juce::jlimit (0, 5, (int) proc.value ("machine"));
    if (m == variantFor) return;
    variantFor = m;
    variantAtt.reset();
    variant.clear (juce::dontSendNotification);
    variant.addItemList (params::variantNames (m), 1);
    variantAtt = std::make_unique<CA> (proc.apvts, params::variantId (m), variant);
    static const char* vt[] = { "Tape speed: faster is cleaner and brighter, with the head bump higher up.",
                                "Cassette tape: ferric is warm and hissy, chrome and metal brighter and cleaner; half speed is a dictation machine.",
                                "Video tape speed: SP, or EP for the long-play rental copy.",
                                "Record speed: 33, 45 or a 78 (mono, narrow, with shellac roar).",
                                "Receiver band: a narrow old set or a wide hi-fi tuner.",
                                "Line: a landline or a mobile call." };
    variant.setTooltip (vt[m]);
    eq.setVisible (m == kStudio);
}

juce::String TarnishEditor::valueText (const juce::String& id) const
{
    const float v = proc.value (id);
    if (id == "drive" || id == "out" || id == "mistrack") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    if (id == "wow" || id == "flutter") return v <= 0.0005f ? juce::String ("Off") : juce::String (v, v < 0.1f ? 3 : 2) + " %";
    if (id == "bias") return std::abs (v) < 2 ? juce::String ("Nominal") : (v < 0 ? "Under " : "Over ") + juce::String (juce::roundToInt (std::abs (v))) + " %";
    if (id == "noise") return v < 0.5f ? juce::String ("Off") : (v < 50 ? juce::String (juce::roundToInt ((v - 50) * 0.4f)) : "+" + juce::String (juce::roundToInt ((v - 50) * 0.4f))) + " dB";
    if (id == "print") return v < 0.5f ? juce::String ("Off") : juce::String (juce::roundToInt (-70.0f + 0.3f * v)) + " dB";
    if (id == "bits") return v >= 23.9f ? juce::String ("Off") : juce::String (v, 1);
    if (id == "rate") return v >= 47999.0f ? juce::String ("Off") : (v >= 1000 ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz");
    if (id == "lowcut") return v <= 10.5f ? juce::String ("Off") : juce::String (juce::roundToInt (v)) + " Hz";
    if (id == "highcut") return v >= 21900.0f ? juce::String ("Off") : juce::String (v / 1000.0f, 1) + " kHz";
    if (id == "varispeed") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " st";
    if (id == "stop_time" || id == "start_time") return juce::String (v, 2) + " s";
    if (id == "tuning") return std::abs (v) < 1 ? juce::String ("Centre") : juce::String (juce::roundToInt (v)) + " %";
    if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (proc.apvts.getParameter (id)))
        return juce::String (juce::roundToInt (v)) + (pf->getLabel().isNotEmpty() ? " " + pf->getLabel() : juce::String());
    if (auto* pr = proc.apvts.getParameter (id)) return pr->getCurrentValueAsText();
    return {};
}

void TarnishEditor::buildDetail()
{
    detail.clear();
    for (int i = 0; i < tabBtns.size(); ++i) tabBtns[i]->setToggleState (tabIds[i] == tab, juce::dontSendNotification);
    const int m = (int) proc.value ("machine");
    for (auto& d : tabDefs (tab, m))
    {
        auto c = std::make_unique<Control>();
        c->id = d.id; c->title = d.title; c->kind = d.kind;
        if (d.kind == 0)
        {
            c->s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            c->s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            if (juce::String (d.id) == "varispeed" || juce::String (d.id) == "tuning" || juce::String (d.id) == "mistrack") c->s->getProperties().set ("bipolar", true);
            c->sa = std::make_unique<SA> (proc.apvts, d.id, *c->s);
            c->s->onValueChange = [this] { content.repaint (detailR); };
            c->s->setTooltip (tip (d.id));
            content.addAndMakeVisible (*c->s);
        }
        else if (d.kind == 1)
        {
            c->c = std::make_unique<juce::ComboBox>();
            if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (d.id))) c->c->addItemList (pc->choices, 1);
            c->ca = std::make_unique<CA> (proc.apvts, d.id, *c->c);
            c->c->setTooltip (tip (d.id));
            content.addAndMakeVisible (*c->c);
        }
        else
        {
            c->b = std::make_unique<juce::TextButton> (d.title);
            c->b->setClickingTogglesState (true);
            c->ba = std::make_unique<BA> (proc.apvts, d.id, *c->b);
            c->b->setTooltip (tip (d.id));
            content.addAndMakeVisible (*c->b);
        }
        detail.push_back (std::move (c));
    }
    layoutDetail();
    content.repaint();
}

void TarnishEditor::layoutDetail()
{
    const int n = (int) detail.size();
    if (n == 0) return;
    const int w = std::min (140, (detailR.getWidth() - 20) / std::max (1, n));
    const int x0 = detailR.getX() + 10;
    for (int i = 0; i < n; ++i)
    {
        auto& c = *detail[(size_t) i];
        const auto cell = juce::Rectangle<int> (x0 + i * w, detailR.getY() + 4, w, detailR.getHeight() - 8);
        if (c.s) c.s->setBounds (cell.withSizeKeepingCentre (50, 50).translated (0, -12));
        if (c.c) c.c->setBounds (cell.withSizeKeepingCentre (w - 16, 30).translated (0, -12));
        if (c.b) c.b->setBounds (cell.withSizeKeepingCentre (w - 18, 30).translated (0, -12));
    }
}

void TarnishEditor::showPresetMenu()
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

void TarnishEditor::showManual (bool on)
{
   #if DH_HAS_MANUAL
    if (! on) { manual.reset(); manualBtn.setToggleState (false, juce::dontSendNotification); return; }
    dh::manual::Theme t;
    t.bg = col::bg; t.panel = col::panel; t.text = col::cream; t.muted = col::creamDim; t.accent = col::tealHi; t.line = col::line;
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TarnishEditor> (this)] { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*m);
    m->setBounds (getLocalBounds());
    m->grabKeyboardFocus();
    manual = std::move (m);
    manualBtn.setToggleState (true, juce::dontSendNotification);
   #else
    juce::ignoreUnused (on);
   #endif
}

void TarnishEditor::timerCallback()
{
    const int m = (int) proc.value ("machine");
    for (int i = 0; i < machineBtns.size(); ++i) machineBtns[i]->setToggleState (i == m, juce::dontSendNotification);
    if (m != lastMachine) { lastMachine = m; bindVariant(); if (tab == "machine") buildDetail(); content.repaint (machineR); }
    deck.tick();
    content.repaint (knobsR); content.repaint (detailR);
}

void TarnishEditor::mouseMove (const juce::MouseEvent& e)
{
    juce::String t;
    for (auto* c = e.eventComponent; c != nullptr && c != &content; c = c->getParentComponent())
        if (auto* tc = dynamic_cast<juce::SettableTooltipClient*> (c)) { t = tc->getTooltip(); if (t.isNotEmpty()) break; }
    if (t != help) { help = t; content.repaint (helpR); }
}

void TarnishEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void TarnishEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    const int y = (barH - 32) / 2;
    prevBtn.setBounds (250, y, 32, 32); presetBtn.setBounds (286, y, 250, 32); nextBtn.setBounds (540, y, 32, 32);
    stopBtn.setBounds (760, y, 110, 32);
    manualBtn.setBounds (baseW - 14 - 96, y, 96, 32);

    deck.setBounds (0, barH, baseW, 390);
    machineR = { 0, barH + 392, baseW, 50 };
    const int my = machineR.getY() + 8, mw = 112;
    for (int i = 0; i < machineBtns.size(); ++i) machineBtns[i]->setBounds (24 + i * (mw + 6), my, mw, 34);
    variant.setBounds (24 + 6 * (mw + 6) + 14, my + 2, 150, 30);
    eq.setBounds (variant.getRight() + 10, my + 2, 90, 30);

    knobsR = { 0, machineR.getBottom() + 2, baseW, 120 };
    const int kw = (baseW - 48) / 8;
    for (int i = 0; i < 8; ++i) knobs[(size_t) i].s.setBounds (24 + i * kw + (kw - 72) / 2, knobsR.getY() + 6, 72, 72);

    const int ty = knobsR.getBottom() + 4, tw = 116;
    for (int i = 0; i < tabBtns.size(); ++i) tabBtns[i]->setBounds (24 + i * (tw + 6), ty, tw, 30);
    detailR = { 24, ty + 36, baseW - 48, baseH - 30 - 8 - (ty + 36) };
    layoutDetail();
    helpR = { 0, baseH - 30, baseW, 30 };
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void TarnishEditor::paintBackdrop (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // Barra superior.
    g.setColour (col::panel); g.fillRect (0, 0, baseW, barH);
    g.setColour (col::line); g.fillRect (0, barH - 1, baseW, 1);
    g.setColour (col::cream); g.setFont (bold (29.0f));
    g.drawText ("Tarnish", 20, 0, 150, barH, juce::Justification::centredLeft);
    g.setColour (col::creamFaint); g.setFont (regular (13.0f));
    g.drawText ("Audio Tools", 124, 3, 100, barH, juce::Justification::centredLeft);
    // Etiquetas y valores de las perillas.
    for (auto& k : knobs)
    {
        const auto r = k.s.getBounds();
        g.setColour (col::creamDim); g.setFont (regular (14.5f));
        g.drawText (k.title, r.getCentreX() - 70, r.getBottom() + 4, 140, 18, juce::Justification::centred);
        g.setColour (col::cream); g.setFont (bold (15.0f));
        g.drawText (valueText (k.id), r.getCentreX() - 70, r.getBottom() + 22, 140, 18, juce::Justification::centred);
    }
    // Panel de detalle.
    g.setColour (col::panel); g.fillRoundedRectangle (detailR.toFloat(), 8.0f);
    g.setColour (col::line); g.drawRoundedRectangle (detailR.toFloat(), 8.0f, 1.0f);
    for (auto& c : detail)
    {
        juce::Rectangle<int> r;
        if (c->s) r = c->s->getBounds(); else if (c->c) r = c->c->getBounds(); else if (c->b) r = c->b->getBounds();
        if (c->kind != 2)
        {
            g.setColour (col::creamDim); g.setFont (regular (13.0f));
            g.drawText (c->title, r.getCentreX() - 70, r.getBottom() + 2, 140, 15, juce::Justification::centred);
            if (c->kind == 0) { g.setColour (col::cream); g.setFont (bold (13.5f)); g.drawText (valueText (c->id), r.getCentreX() - 70, r.getBottom() + 17, 140, 16, juce::Justification::centred); }
        }
    }
    // Línea de ayuda.
    g.setColour (col::panel); g.fillRect (helpR);
    g.setColour (col::line); g.fillRect (helpR.getX(), helpR.getY(), helpR.getWidth(), 1);
    g.setColour (help.isNotEmpty() ? col::cream : col::creamFaint); g.setFont (regular (14.5f));
    g.drawText (help.isNotEmpty() ? help : juce::String ("Point at any control to see what it does. Turn up Wear and Dropouts and watch the tape reach the head."),
                helpR.reduced (16, 0), juce::Justification::centredLeft);
}
