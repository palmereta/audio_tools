#include "PluginEditor.h"

// El manual con los colores de la serie (el acento de este producto en la web).
static dh::manual::Theme manualTheme() { dh::manual::Theme t; t.accent = juce::Colour (0xffa9c83c); return t; }

using namespace dh::fs;
using namespace dh::fs::ui;

ATFS1Editor::ATFS1Editor (ATFS1Processor& p)
    : AudioProcessorEditor (&p), proc (p), rack (p), play (p), voicePg (p), opPg (p), commonPg (p), fseqPg (p), perfPg (p), fxPg (p), utilPg (p), keys (p)
{
    setLookAndFeel (&lnf);
    content.painter = [this] (juce::Graphics& g) { paintBackdrop (g); };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    const auto& A = Assets::get();
    rackH = (int) std::lround ((double) baseW * A.viewHeight() / A.viewWidth());
    tabY = rackH; pageY = rackH + 40; statusY = baseH - 82; keysY = baseH - 58;
    content.addAndMakeVisible (rack);
    rack.setBounds (0, 0, baseW, rackH);
    pages = { &play, &voicePg, &opPg, &commonPg, &fseqPg, &perfPg, &fxPg, &utilPg };
    static const char* names[NumPages] = { "Play", "Voice", "Operator", "Common", "Fseq", "Perform", "Effects", "Utility" };
    for (int i = 0; i < NumPages; ++i)
    {
        auto& t = tabs[(size_t) i];
        t.setButtonText (names[i]);
        t.onClick = [this, i] { showPage (i); };
        content.addAndMakeVisible (t);
        content.addChildComponent (pages[(size_t) i]);
        pages[(size_t) i]->setBounds (0, pageY, baseW, statusY - pageY);
    }
    content.addAndMakeVisible (keys);
    content.addAndMakeVisible (manualBtn);
    manualBtn.onClick = [this] { manualHost.toggle (*this, manualTheme()); };
    keys.setBounds (0, keysY, baseW, baseH - keysY);

    rack.onButton = [this] (const juce::String& id) { onRackButton (id); };
    rack.onDataKnob = [this] (int k, int s) { onDataKnob (k, s); };
    rack.line1 = [this] { return lcdLine1(); };
    rack.line2 = [this] { return lcdLine2(); };
    voicePg.onOpenOp = [this] (int k, bool n) { opPg.selectOp (k); opPg.setUnvoiced (n); showPage (POperator); };
    dh::HelpRouter::add (this);

    proc.onPresetChanged = [this] { refreshAll(); };
    proc.onVoiceChanged = [this] { for (auto* pg : pages) if (pg->isVisible()) pg->refresh(); rack.repaint(); };
    proc.onFseqChanged = [this] { play.refresh(); fseqPg.refresh(); };
    proc.onMessage = [this] { message = proc.lastMessage; messageUntil = juce::Time::getMillisecondCounter() + 5000; content.repaint (0, statusY, baseW, 24); rack.flash ("", proc.lastMessage.substring (0, 24)); };

    showPage (PPlay);
    refreshAll();
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.4), (int) (baseH * 1.4));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.8), (int) (baseH * 0.8));
    startTimerHz (4);

    // Verificación sin capturar la pantalla: ATFS1_SNAPSHOT=/ruta.png guarda el editor a 2x;
    // ATFS1_PRESET=n, ATFS1_PAGE=0..7, ATFS1_OP=1..8 (N para el sin voz: ATFS1_UNVOICED=1), ATFS1_KEYS=1 toca un acorde.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("ATFS1_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        const bool manualOpen = juce::SystemStats::getEnvironmentVariable ("ATFS1_MANUAL", "0").getIntValue() != 0;      // con el manual abierto
        const int presetIndex = juce::SystemStats::getEnvironmentVariable ("ATFS1_PRESET", "-1").getIntValue();
        const int pg = juce::SystemStats::getEnvironmentVariable ("ATFS1_PAGE", "0").getIntValue();
        const int opn = juce::SystemStats::getEnvironmentVariable ("ATFS1_OP", "1").getIntValue();
        const bool unv = juce::SystemStats::getEnvironmentVariable ("ATFS1_UNVOICED", "0").getIntValue() != 0;
        const bool k = juce::SystemStats::getEnvironmentVariable ("ATFS1_KEYS", "0").getIntValue() != 0;
        juce::Component::SafePointer<ATFS1Editor> safe (this);
        juce::Timer::callAfterDelay (700, [safe, manualOpen, presetIndex, pg, opn, unv, k] {
            if (safe == nullptr) return;
            if (presetIndex >= 0) safe->proc.loadPreset (presetIndex);
            safe->opPg.selectOp (opn - 1); safe->opPg.setUnvoiced (unv);
            safe->showPage (pg);
            if (manualOpen) safe->manualHost.open (*safe, manualTheme());
            if (k) for (int n : { 48, 55, 64 }) safe->proc.sendCommand (ATFS1Processor::CmdNoteOn, n, 100);
        });
        juce::Timer::callAfterDelay (3200, [safe, snap] {
            if (safe == nullptr) return;
            const bool man = safe->manualHost.isOpen();
            juce::Component& target = man ? (juce::Component&) *safe : (juce::Component&) safe->content;
            const auto img = target.createComponentSnapshot (target.getLocalBounds(), true, man ? 2.0f * (float) baseW / (float) safe->getWidth() : 2.0f);
            juce::File f (snap);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

ATFS1Editor::~ATFS1Editor()
{
    proc.onPresetChanged = nullptr; proc.onVoiceChanged = nullptr; proc.onFseqChanged = nullptr; proc.onMessage = nullptr;
    dh::HelpRouter::remove (this);
    setLookAndFeel (nullptr);
}

void ATFS1Editor::refreshAll()
{
    for (auto* pg : pages) pg->refresh();
    rack.repaint();
}

void ATFS1Editor::showPage (int p)
{
    page = juce::jlimit (0, NumPages - 1, p);
    for (int i = 0; i < NumPages; ++i) { pages[(size_t) i]->setVisible (i == page); tabs[(size_t) i].setToggleState (i == page, juce::dontSendNotification); }
    pages[(size_t) page]->refresh();
    cursor = 0;
    for (auto* pg : pages) for (auto* f : pg->cursorFields()) f->highlighted = false;
    static const char* lit[NumPages] = { "play", "voice", "voice", "voice", "search", "perform", "effect", "util" };
    rack.litPage = lit[page];
    rack.repaint();
}

void ATFS1Editor::moveCursor (int d)
{
    auto fs = pages[(size_t) page]->cursorFields();
    for (auto* f : fs) { f->highlighted = false; f->repaint(); }
    if (fs.empty()) return;
    cursor = (cursor + d + (int) fs.size()) % (int) fs.size();
    fs[(size_t) cursor]->highlighted = true; fs[(size_t) cursor]->repaint();
    rack.flash (fs[(size_t) cursor]->getField().label, fs[(size_t) cursor]->valueText());
}

void ATFS1Editor::onDataKnob (int knob, int steps)
{
    if (knob == 0) { opPg.selectOp ((opPg.currentOp() + steps + 8) % 8); if (page != POperator) showPage (POperator); rack.flash ("Operator", juce::String (opPg.currentOp() + 1)); }
    else if (knob == 1) { showPage ((page + steps + NumPages) % NumPages); rack.flash ("Page", tabs[(size_t) page].getButtonText()); }
    else if (knob == 2) moveCursor (steps);
    else
    {
        auto fs = pages[(size_t) page]->cursorFields();
        if (! fs.empty()) { auto* f = fs[(size_t) juce::jlimit (0, (int) fs.size() - 1, cursor)]; f->highlighted = true; f->step (steps); }
    }
}

void ATFS1Editor::onRackButton (const juce::String& id)
{
    if (id == "play" || id == "exit") showPage (PPlay);
    else if (id == "perform") showPage (PPerform);
    else if (id == "effect") showPage (PEffects);
    else if (id == "voice") showPage (page == PVoice ? PCommon : (page == PCommon ? POperator : PVoice));
    else if (id == "util") showPage (PUtility);
    else if (id == "search") showPage (PFseq);
    else if (id == "mute")
    {
        const int m = (proc.muteMode.load() + 1) % 3;
        proc.muteMode.store (m);
        rack.flash ("Mute/Solo", m == 0 ? "ALL" : (m == 1 ? "VOICED" : "UNVOICED"));
    }
    else if (id == "part_dn" || id == "part_up")
    {
        const int d = id == "part_up" ? 1 : -1;
        if (page == POperator) onDataKnob (0, d);
        else if (! proc.getPresets().empty())
        {
            const int n = (int) proc.getPresets().size();
            proc.loadPreset ((proc.getCurrentProgram() + d + n) % n);
        }
    }
    else if (id == "cur_dn" || id == "cur_up") moveCursor (id == "cur_up" ? 1 : -1);
    else if (id == "val_dn" || id == "val_up") onDataKnob (3, id == "val_up" ? 1 : -1);
    else if (id == "enter")
    {
        auto fs = pages[(size_t) page]->cursorFields();
        if (! fs.empty()) { auto* f = fs[(size_t) juce::jlimit (0, (int) fs.size() - 1, cursor)]; rack.flash (f->getField().label, f->valueText()); }
    }
    else if (id == "power_off") { proc.power.store (false); proc.sendCommand (ATFS1Processor::CmdPanic); }
    else if (id == "power_on") proc.power.store (true);
}

juce::String ATFS1Editor::lcdLine1() const
{
    return "P" + juce::String (proc.getCurrentProgram() + 1).paddedLeft ('0', 3) + " " + proc.currentPresetName.substring (0, 19) + (proc.voiceDirty ? "*" : "");
}

juce::String ATFS1Editor::lcdLine2() const
{
    const auto& v = proc.voice();
    const int m = proc.muteMode.load();
    const juce::String mute = m == 1 ? " V" : (m == 2 ? " N" : "");
    switch (page)
    {
        case POperator:
        {
            const int k = opPg.currentOp();
            if (opPg.isUnvoiced()) return "OP" + juce::String (k + 1) + " NOISE  LVL " + juce::String (v.nop[(size_t) k].level) + mute;
            return "OP" + juce::String (k + 1) + " " + formNames()[v.op[(size_t) k].form & 7].toUpperCase() + " " + ratioText (v.op[(size_t) k]).toUpperCase() + mute;
        }
        case PFseq: return proc.fseq().empty() ? juce::String ("FSEQ --") : "FSEQ " + juce::String (proc.fseq().name).toUpperCase().substring (0, 10) + " " + juce::String ((int) proc.fseq().frames.size());
        case PPerform: return "EDIT PERFORM" + mute;
        case PEffects: return "EDIT EFFECT";
        case PUtility: return "UTILITY";
        case PCommon: return "VOICE COMMON" + mute;
        default: break;
    }
    juce::String s = "ALG" + juce::String (v.algorithm + 1).paddedLeft ('0', 2) + " FB" + juce::String (v.feedback);
    if (proc.value ("fseq") > 0.5f && ! proc.fseq().empty()) s << " " << juce::String (proc.fseq().name).toUpperCase().substring (0, 8);
    s = s.paddedRight (' ', 19) + juce::String (proc.live.voices.load()).paddedLeft (' ', 2) + "V" + mute;
    return s;
}

void ATFS1Editor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void ATFS1Editor::paintBackdrop (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // Barra de pestañas.
    g.setColour (col::bg); g.fillRect (0, tabY, baseW, pageY - tabY);
    g.setColour (col::border); g.drawHorizontalLine (pageY - 1, 0.0f, (float) baseW);
    // Línea de estado: ayuda o mensaje a la izquierda, voces y CPU a la derecha.
    const juce::Rectangle<int> st (16, statusY, baseW - 32, 24);
    const bool msg = juce::Time::getMillisecondCounter() < messageUntil;
    g.setFont (font (12.5f));
    g.setColour (msg ? col::green : col::dim);
    g.drawText (msg ? message : help, st.withTrimmedRight (260), juce::Justification::centredLeft, true);
    g.setColour (col::faint);
    g.drawText (juce::String (proc.live.voices.load()) + " voices   CPU " + juce::String (proc.cpuLoad.load() * 100.0f, 1) + " %", st, juce::Justification::centredRight);
}

void ATFS1Editor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content.setTransform (juce::AffineTransform::scale (s));
    int x = 16;
    for (auto& t : tabs) { t.setBounds (x, tabY + 6, 104, 28); x += 108; }
    manualBtn.setBounds (baseW - 16 - 104, tabY + 6, 104, 28);
    manualHost.layout (getLocalBounds());
}

bool ATFS1Editor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (f.endsWithIgnoreCase (".syx") || f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac")) return true;
    return false;
}

void ATFS1Editor::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    const juce::File file (files[0]);
    if (files[0].endsWithIgnoreCase (".syx") || files[0].endsWithIgnoreCase (".mid")) { proc.postMessage (proc.importSyx (file)); utilPg.refresh(); }
    else { proc.analyzeAudioAsync (file); showPage (PFseq); }
}

void ATFS1Editor::timerCallback()
{
    const float c = proc.cpuLoad.load(); const int v = proc.live.voices.load();
    if (std::abs (c - cpuShown) > 0.002f || v != voicesShown || (messageUntil != 0 && juce::Time::getMillisecondCounter() > messageUntil))
    {
        cpuShown = c; voicesShown = v;
        if (juce::Time::getMillisecondCounter() > messageUntil) messageUntil = 0;
        content.repaint (0, statusY, baseW, 24);
    }
}
