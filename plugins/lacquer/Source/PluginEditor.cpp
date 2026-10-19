#include "PluginEditor.h"

using namespace dh::lq;
using namespace dh::lq::ui;

namespace {
juce::String signedDb (double v, int dec = 1) { return (v > 0 ? "+" : "") + juce::String (v, dec) + " dB"; }
juce::String hzText (double hz) { return hz >= 1000.0 ? (hz >= 10000.0 ? juce::String (juce::roundToInt (hz / 1000.0)) : juce::String (hz / 1000.0, 1)) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }
}

LacquerEditor::LacquerEditor (LacquerProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    content.painter = [this] (juce::Graphics& g) { paintBackdrop (g); };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &disc, &stylePicker, &targetSwitch, &intensity, &ceiling, &readouts, &balance, &modules,
                                                                       &learnBtn, &fileBtn, &manualBtn, &playBtn, &exportBtn, &closeBtn, &compareBtn })
        content.addAndMakeVisible (c);
    learnBtn.getProperties().set ("primary", true);
    fileBtn.getProperties().set ("primary", true);

    // Estilo
    stylePicker.setIndex ((int) proc.value (params::style));
    stylePicker.onChange = [this] (int i) { proc.setValue (params::style, (float) i); };
    stylePicker.onHover = [this] (const juce::String& s) { hoverHelp = s; content.repaint (statusR); };
    // Objetivo: al elegirlo, el techo va al del objetivo (el usuario lo puede cambiar después).
    juce::StringArray tn, ts;
    for (auto& t : loudTargets())
    {
        tn.add (juce::String (t.name).upToFirstOccurrenceOf (" ", false, false).replace ("Broadcast", "Broadcast"));
        ts.add (t.matchInput ? juce::String ("keep the mix level") : juce::String (juce::roundToInt (t.lufs)) + " LUFS");
    }
    tn.set (6, "Match");
    targetSwitch.setItems (tn, ts);
    targetSwitch.setIndex ((int) proc.value (params::target));
    targetSwitch.onChange = [this] (int i) {
        proc.setValue (params::target, (float) i);
        proc.setValue (params::ceiling, (float) loudTargets()[(size_t) i].ceilingDb);
    };
    targetSwitch.setTooltip ("Loudness target. Streaming platforms turn louder masters down, so -14 LUFS loses nothing there; Loud and Club trade dynamics for level.");

    for (auto* s : { &intensity, &ceiling })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    }
    intensity.getProperties().set ("ticks", 11);
    ceiling.getProperties().set ("ticks", 7);
    intensityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, params::intensity, intensity);
    ceilingAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, params::ceiling, ceiling);
    intensity.setTooltip ("Intensity: how much Lacquer changes the tone and the dynamics. 0 % only sets the loudness; 50 % is the default. It does not change the loudness target.");
    ceiling.setTooltip ("Ceiling: the highest true peak allowed. -1 dBTP leaves room for MP3/AAC encoding; -0.3 only for CD or WAV delivery.");
    intensity.onValueChange = ceiling.onValueChange = [this] { content.repaint(); };

    compareBtn.setClickingTogglesState (true);
    compareAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, params::compare, compareBtn);
    compareBtn.setTooltip ("Plays the original mix turned up or down to the master's loudness, so you compare the sound and not the level.");
    const char* pv[] = { "Off", "Spotify", "Apple", "Broadcast" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = previewBtns[i];
        b.setButtonText (pv[i]); b.setClickingTogglesState (false);
        b.onClick = [this, i] { proc.setValue (params::preview, (float) i); syncFromParams(); };
        content.addAndMakeVisible (b);
    }
    previewBtns[1].setTooltip ("Hear the master as Spotify and YouTube play it: turned down to -14 LUFS if it is louder.");
    previewBtns[2].setTooltip ("Hear it as Apple Music plays it (-16 LUFS).");
    previewBtns[3].setTooltip ("Hear it at broadcast level (-23 LUFS).");

    learnBtn.onClick = [this] {
        const auto st = proc.learnState.load();
        if (st == LacquerProcessor::LearnState::listening) proc.stopLearning();
        else if (st != LacquerProcessor::LearnState::analysing) proc.startLearning();
    };
    learnBtn.setTooltip ("Press Learn and play your song: the whole song for an exact loudness, or its loudest 30 s for a quick result. Lacquer stops when the music stops.");
    fileBtn.onClick = [this] { chooseFile(); };
    fileBtn.setTooltip ("Master a whole song from a file (WAV, AIFF, FLAC, MP3): analyse, preview, export. You can also drop a file on the window.");
    playBtn.onClick = [this] { proc.filePlaying.store (! proc.filePlaying.load()); };
    exportBtn.onClick = [this] { chooseExport(); };
    closeBtn.onClick = [this] { proc.closeFile(); refreshPlan(); };
    manualBtn.onClick = [this] { showManual (manual == nullptr); };
   #if ! DH_HAS_MANUAL
    manualBtn.setVisible (false);
   #endif
    disc.onSeek = [this] (double f) { proc.seekFile (f * proc.fileSeconds()); };

    proc.onPlanChanged = [this] { refreshPlan(); };
    refreshPlan();
    syncFromParams();

    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.6), (int) (baseH * 1.6));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.9), (int) (baseH * 0.9));
    startTimerHz (30);

    // Capturas sin pantalla (manual, web): LACQUER_SNAPSHOT=/ruta.png, LACQUER_FILE=/audio.wav (lo masteriza antes),
    // LACQUER_WAIT=segundos, LACQUER_MANUAL=1, LACQUER_COMPARE=1, LACQUER_SEEK=0..1 (cabezal), LACQUER_STYLE=n,
    // LACQUER_TARGET=n, LACQUER_INTENSITY=0..100, LACQUER_LEARN_FILE=/audio.wav (modo en vivo, como si hubiera sonado).
    const auto snap = juce::SystemStats::getEnvironmentVariable ("LACQUER_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
        const auto file = env ("LACQUER_FILE", "");
        const int wait = env ("LACQUER_WAIT", "4").getIntValue();
        const bool man = env ("LACQUER_MANUAL", "0").getIntValue() != 0;
        const int style = env ("LACQUER_STYLE", "-1").getIntValue(), target = env ("LACQUER_TARGET", "-1").getIntValue();
        const float inten = env ("LACQUER_INTENSITY", "-1").getFloatValue();
        if (style >= 0) proc.setValue (params::style, (float) style);
        if (target >= 0) { proc.setValue (params::target, (float) target); proc.setValue (params::ceiling, (float) loudTargets()[(size_t) target].ceilingDb); }
        if (inten >= 0) proc.setValue (params::intensity, inten);
        if (env ("LACQUER_COMPARE", "0").getIntValue() != 0) proc.setValue (params::compare, 1.0f);
        auto resolve = [] (const juce::String& path) { return juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File::getCurrentWorkingDirectory().getChildFile (path); };
        if (file.isNotEmpty()) proc.openFile (resolve (file));
        if (const auto lf = env ("LACQUER_LEARN_FILE", ""); lf.isNotEmpty()) proc.learnFromFile (resolve (lf));
        const double seek = env ("LACQUER_SEEK", "-1").getDoubleValue();
        juce::Component::SafePointer<LacquerEditor> safe (this);
        juce::Timer::callAfterDelay (wait * 1000, [safe, snap, man, seek] {
            if (safe == nullptr) return;
            if (seek >= 0.0) safe->proc.seekFile (seek * safe->proc.fileSeconds());
            if (man) safe->showManual (true);
            safe->refreshPlan(); safe->timerCallback();
            juce::Timer::callAfterDelay (500, [safe, snap, man] {
                if (safe == nullptr) return;
                juce::Component& shot = man && safe->manual != nullptr ? (juce::Component&) *safe : (juce::Component&) safe->content;
                const auto img = shot.createComponentSnapshot (shot.getLocalBounds(), true, man ? 2.0f * (float) baseW / (float) safe->getWidth() : 2.0f);
                juce::File f (snap); f.deleteFile();
                juce::FileOutputStream os (f);
                juce::PNGImageFormat().writeImageToStream (img, os);
            });
        });
    }
}

LacquerEditor::~LacquerEditor() { proc.onPlanChanged = nullptr; manual.reset(); setLookAndFeel (nullptr); }

void LacquerEditor::syncFromParams()
{
    stylePicker.setIndex ((int) proc.value (params::style));
    targetSwitch.setIndex ((int) proc.value (params::target));
    const int pv = (int) proc.value (params::preview);
    for (int i = 0; i < 4; ++i) previewBtns[i].setToggleState (i == pv, juce::dontSendNotification);
}

void LacquerEditor::refreshPlan()
{
    const Plan P = proc.planCopy();
    const auto in = proc.inputCopy();
    const bool have = proc.hasPlan() && in.valid;
    const bool fileMode = proc.fileState.load() != LacquerProcessor::FileState::none;
    disc.setGroove (have ? proc.outGrooveCopy() : std::vector<float> {}, {});
    BandCurve om {}; const bool hasOut = proc.outSpectrum (om);
    balance.set (in.mid, om, hasOut && have, styleDef (P.opt.style).target, P.eqFit, have);

    std::vector<ModulesView::Row> rows;
    {
        // Las dos correcciones más grandes de la EQ (tres no entran en la línea).
        std::vector<EqBand> e (P.eq.begin(), P.eq.end());
        std::sort (e.begin(), e.end(), [] (const EqBand& a, const EqBand& b) { return std::fabs (a.gainDb) > std::fabs (b.gainDb); });
        juce::StringArray parts;
        for (int i = 0; i < 2; ++i)
            if (std::fabs (e[(size_t) i].gainDb) >= 0.3)
                parts.add (signedDb (e[(size_t) i].gainDb) + (e[(size_t) i].type == 1 ? " below " : e[(size_t) i].type == 2 ? " above " : " at ") + hzText (e[(size_t) i].hz));
        rows.push_back ({ "Tone EQ", parts.isEmpty() ? juce::String ("Already balanced, no EQ") : parts.joinIntoString (", "), ! parts.isEmpty(), 0.0f, 0.0f });
    }
    rows.push_back ({ "Harshness", P.harsh.on ? "Up to -" + juce::String (P.harsh.maxCutDb, 1) + " dB at " + hzText (P.harsh.hz) + ", only when it sticks out" : juce::String ("Not needed"), P.harsh.on, 0.0f, 4.0f });
    rows.push_back ({ "De-esser", P.sib.on ? "Up to -" + juce::String (P.sib.maxCutDb, 1) + " dB at " + hzText (P.sib.hz) : juce::String ("Not needed"), P.sib.on, 0.0f, 4.0f });
    rows.push_back ({ "Multiband", P.mbOn ? "3 bands (" + hzText (P.xo1) + ", " + hzText (P.xo2) + "), " + juce::String (P.mb[0].ratio, 1) + ":1" : juce::String ("Off"), P.mbOn, 0.0f, 4.0f });
    rows.push_back ({ "Glue", P.glueOn ? juce::String (P.glue.ratio, 2) + ":1, attack " + juce::String (juce::roundToInt (P.glue.attackMs)) + " ms, release " + juce::String (juce::roundToInt (P.glue.releaseMs)) + " ms" : juce::String ("Off"), P.glueOn, 0.0f, 4.0f });
    {
        juce::StringArray s;
        if (P.stereo.monoHz > 0) s.add ("mono below " + hzText (P.stereo.monoHz));
        if (std::fabs (P.stereo.sideGainDb) >= 0.1) s.add ("sides " + signedDb (P.stereo.sideGainDb));
        if (std::fabs (P.stereo.sideHighShelfDb) >= 0.1) s.add ("high sides " + signedDb (P.stereo.sideHighShelfDb));
        rows.push_back ({ "Stereo", s.isEmpty() ? juce::String ("Untouched") : s.joinIntoString (", ").replaceSection (0, 1, s.joinIntoString (", ").substring (0, 1).toUpperCase()), P.stereo.on && ! s.isEmpty(), 0.0f, 0.0f });
    }
    rows.push_back ({ "Saturation", P.sat.on ? juce::String (P.sat.flavour == 0 ? "Tape" : "Valve") + ", " + juce::String (P.sat.driveDb, 1) + " dB drive" : juce::String ("Off"), P.sat.on, 0.0f, 0.0f });
    rows.push_back ({ "Clipper", P.clipOn ? "Rounds off peaks above " + juce::String (P.clipThresholdDb, 1) + " dBFS before the limiter" : juce::String ("Off"), P.clipOn, 0.0f, 0.0f });
    rows.push_back ({ "Limiter", "Ceiling " + juce::String (P.lim.ceilingDb, 1) + " dBTP, gain " + signedDb (P.gainDb) + ", " + juce::String (P.limiterAvgGrDb, 1) + " dB average", have, 0.0f, 6.0f });
    modules.set (rows);

    Readouts::Values v;
    v.have = have; v.inI = in.lufs; v.inTp = in.tp; v.inLra = in.lra; v.outI = P.outLufs; v.outTp = P.outTruePeakDb; v.outLra = P.outLra;
    v.target = P.targetLufs; v.ceiling = P.ceilingDb;
    readouts.set (v);
    disc.setLabel (have ? juce::String (styleDef (P.opt.style).name) : juce::String ("Lacquer"),
                   have ? juce::String (P.outLufs, 1) : juce::String ("--"),
                   have ? "LUFS, " + juce::String (P.outTruePeakDb, 1) + " dBTP" : juce::String ("press Learn"));
    playBtn.setVisible (fileMode); exportBtn.setVisible (fileMode); closeBtn.setVisible (fileMode);
    syncFromParams();
    content.repaint();
}

void LacquerEditor::timerCallback()
{
    proc.inMeter.updateUi(); proc.outMeter.updateUi();
    const auto ls = proc.learnState.load();
    const auto fs = proc.fileState.load();
    const bool fileMode = fs != LacquerProcessor::FileState::none;
    // Botón LEARN según el estado.
    if (ls == LacquerProcessor::LearnState::listening)
        learnBtn.setButtonText (proc.learnedSeconds() >= LacquerProcessor::kLearnMinSeconds ? "Stop (" + juce::String ((int) proc.learnedSeconds()) + " s)" : "Listening " + juce::String ((int) proc.learnedSeconds()) + " s");
    else learnBtn.setButtonText (ls == LacquerProcessor::LearnState::analysing ? "Analysing" : "Learn");
    learnBtn.setToggleState (ls == LacquerProcessor::LearnState::listening, juce::dontSendNotification);
    playBtn.setButtonText (proc.filePlaying.load() ? "Pause" : "Play");
    playBtn.setEnabled (fs == LacquerProcessor::FileState::ready);
    exportBtn.setEnabled (fs == LacquerProcessor::FileState::ready);

    // Disco.
    if (ls == LacquerProcessor::LearnState::listening)
    {
        const double frac = 1.0 - std::exp (-proc.learnedSeconds() / 90.0);      // el cabezal avanza más lento con el tiempo
        disc.setState (1.0, frac, true, false);
        spin += 0.05f;
    }
    else if (fileMode && fs == LacquerProcessor::FileState::ready)
    {
        const double frac = proc.fileSeconds() > 0 ? proc.filePos.load() / proc.fileSeconds() : 0.0;
        disc.setState (1.0, frac, false, true);
        if (proc.filePlaying.load()) spin += 0.05f;
    }
    else if (fileMode) { disc.setState (proc.fileProgress.load(), proc.fileProgress.load(), true, false); spin += 0.05f; }
    else disc.setState (1.0, proc.hasPlan() ? 1.0 : -1.0, false, false);
    disc.setSpin (spin);

    // Lectores en vivo y reducciones.
    auto v = Readouts::Values();
    {
        const Plan P = proc.planCopy(); const auto in = proc.inputCopy();
        v.have = proc.hasPlan() && in.valid; v.inI = in.lufs; v.inTp = in.tp; v.inLra = in.lra; v.outI = P.outLufs; v.outTp = P.outTruePeakDb; v.outLra = P.outLra;
        v.target = P.targetLufs; v.ceiling = P.ceilingDb;
    }
    v.liveM = proc.outMeter.momentary.load(); v.liveS = proc.outMeter.shortTerm.load(); v.liveTp = proc.outMeter.truePeak.load();
    readouts.set (v);
    modules.setGr (1, proc.grHarsh.load()); modules.setGr (2, proc.grSib.load());
    modules.setGr (3, std::max ({ proc.grMb[0].load(), proc.grMb[1].load(), proc.grMb[2].load() }));
    modules.setGr (4, proc.grGlue.load()); modules.setGr (8, proc.grLim.load());
    modules.repaint();
    compareBtn.setButtonText (proc.hasPlan() ? "A/B with the original (" + signedDb (proc.abGainDb.load()) + ")" : juce::String ("A/B with the original"));
    content.repaint (statusR); content.repaint (underDiscR); content.repaint (turnDownR);
}

void LacquerEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a mix to master", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
        const auto f = fc.getResult();
        if (f.existsAsFile()) { proc.openFile (f); refreshPlan(); }
    });
}

void LacquerEditor::chooseExport()
{
    const auto src = juce::File (proc.fileName());
    const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> ("Export the master", dir.getChildFile (src.getFileNameWithoutExtension() + "_master.wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("wav");
        juce::String err;
        if (proc.exportMaster (f, 24, err))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Master exported", f.getFileName() + " (24-bit WAV) and " + f.withFileExtension ("json").getFileName() + " (report) are ready.");
        else
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed", err);
    });
}

bool LacquerEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}
void LacquerEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { proc.openFile (juce::File (f)); refreshPlan(); break; }
    content.repaint();
}

void LacquerEditor::showManual (bool on)
{
   #if DH_HAS_MANUAL
    if (! on) { manual.reset(); manualBtn.setToggleState (false, juce::dontSendNotification); return; }
    dh::manual::Theme t;
    t.bg = col::enamelHi; t.panel = col::enamel; t.text = col::ink; t.muted = col::inkSoft; t.accent = col::cut; t.line = col::seam;
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<LacquerEditor> (this)] { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*m);
    m->setBounds (getLocalBounds());
    m->grabKeyboardFocus();
    manual = std::move (m);
    manualBtn.setToggleState (true, juce::dontSendNotification);
   #else
    juce::ignoreUnused (on);
   #endif
}

void LacquerEditor::paint (juce::Graphics& g) { g.fillAll (col::enamel); }

void LacquerEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    const int y = (barH - 34) / 2;
    learnBtn.setBounds (196, y, 150, 34);
    fileBtn.setBounds (354, y, 150, 34);
    manualBtn.setBounds (baseW - 16 - 96, y, 96, 34);
    statusR = { 520, 0, baseW - 520 - 124, barH };

    disc.setBounds (20, barH + 14, 452, 470);
    underDiscR = { 20, barH + 490, 452, 52 };
    playBtn.setBounds (underDiscR.getX(), underDiscR.getY() + 8, 90, 34);
    exportBtn.setBounds (underDiscR.getRight() - 132 - 116, underDiscR.getY() + 8, 128, 34);
    closeBtn.setBounds (underDiscR.getRight() - 112, underDiscR.getY() + 8, 112, 34);

    const int rx = 492, rw = baseW - rx - 20;
    stylePicker.setBounds (rx, barH + 30, rw, 74);
    intensity.setBounds (rx + 10, barH + 150, 150, 150);
    targetSwitch.setBounds (rx + 176, barH + 118, 330, 236);
    ceiling.setBounds (rx + rw - 140, barH + 164, 120, 120);
    readouts.setBounds (rx, barH + 372, rw, 156);
    turnDownR = { rx, barH + 530, rw, 22 };
    compareBtn.setBounds (rx, barH + 558, 250, 34);
    for (int i = 0; i < 4; ++i) previewBtns[i].setBounds (rx + rw - 4 * 82 + i * 82, barH + 558, 78, 34);

    balance.setBounds (20, barH + 606, 700, baseH - barH - 606 - 16);
    modules.setBounds (732, barH + 606, baseW - 732 - 20, baseH - barH - 606 - 16);
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void LacquerEditor::paintBackdrop (juce::Graphics& g)
{
    // Panel esmaltado con un leve granulado de luz de arriba.
    juce::ColourGradient bg (col::enamelHi, 0, 0, col::enamel.darker (0.04f), 0, (float) baseH, false);
    g.setGradientFill (bg); g.fillAll();
    // Barra superior: chapa oscura con el nombre.
    g.setColour (col::ink); g.fillRect (0, 0, baseW, barH);
    g.setColour (col::label); g.setFont (bold (32.0f));
    g.drawText ("Lacquer", 20, 0, 140, barH, juce::Justification::centredLeft);
    g.setColour (col::label.withAlpha (0.55f)); g.setFont (regular (14.0f));
    g.drawText ("Audio Tools", 132, 4, 80, barH, juce::Justification::centredLeft);
    // Luz de grabación (encendida mientras escucha o renderiza).
    const auto ls = proc.learnState.load();
    const auto fs = proc.fileState.load();
    const bool rec = ls == LacquerProcessor::LearnState::listening || ls == LacquerProcessor::LearnState::analysing
                     || fs == LacquerProcessor::FileState::loading || fs == LacquerProcessor::FileState::rendering;
    g.setColour (rec ? col::cut : col::cut.withAlpha (0.25f));
    g.fillEllipse (statusR.getX() - 4.0f, barH * 0.5f - 5, 10, 10);
    if (rec) { g.setColour (col::cut.withAlpha (0.3f)); g.fillEllipse (statusR.getX() - 9.0f, barH * 0.5f - 10, 20, 20); }
    g.setColour (col::label.withAlpha (hoverHelp.isNotEmpty() ? 1.0f : 0.85f)); g.setFont (regular (15.5f));
    g.drawFittedText (hoverHelp.isNotEmpty() ? hoverHelp : proc.statusText(), statusR.withTrimmedLeft (16), juce::Justification::centredLeft, 2, 0.9f);

    // Leyendas de los controles.
    const int rx = 492, rw = baseW - rx - 20;
    g.setColour (col::ink); g.setFont (bold (18.0f));
    g.drawText ("Style", rx, barH + 6, 200, 22, juce::Justification::centredLeft);
    g.drawText ("Intensity", rx + 10, barH + 122, 150, 22, juce::Justification::centred);
    g.drawText ("Loudness target", rx + 176, barH + 112, 330, 22, juce::Justification::centred);
    g.drawText ("Ceiling", rx + rw - 140, barH + 136, 120, 22, juce::Justification::centred);
    g.setFont (bold (24.0f));
    g.drawText (juce::String ((int) proc.value (params::intensity)) + " %", rx + 10, barH + 296, 150, 28, juce::Justification::centred);
    g.drawText (juce::String (proc.value (params::ceiling), 1), rx + rw - 140, barH + 282, 120, 28, juce::Justification::centred);
    g.setColour (col::inkSoft); g.setFont (regular (14.0f));
    g.drawText ("dBTP", rx + rw - 140, barH + 306, 120, 18, juce::Justification::centred);
    g.drawText ("Low", rx + 10, barH + 280, 50, 16, juce::Justification::centredLeft);
    g.drawText ("High", rx + 110, barH + 280, 50, 16, juce::Justification::centredRight);
    const int pvx = rx + rw - 4 * 82;
    g.drawText ("Hear it as", pvx - 90, barH + 558, 84, 34, juce::Justification::centredRight);

    // Cuánto lo van a bajar las plataformas.
    const Plan P = proc.planCopy();
    if (proc.hasPlan())
    {
        juce::String t;
        const double sp = P.outLufs - (-14.0), ap = P.outLufs - (-16.0);
        if (sp > 0.2) t = "Streaming players will turn this down: Spotify and YouTube " + juce::String (sp, 1) + " dB, Apple Music " + juce::String (ap, 1) + " dB.";
        else if (ap > 0.2) t = "Plays at full level on Spotify and YouTube; Apple Music turns it down " + juce::String (ap, 1) + " dB.";
        else t = "Plays at full level on Spotify, YouTube and Apple Music.";
        g.setColour (col::inkSoft); g.setFont (regular (15.0f));
        g.drawText (t, turnDownR, juce::Justification::centredLeft);
    }

    // Debajo del disco.
    g.setColour (col::inkSoft); g.setFont (regular (16.0f));
    if (fs == LacquerProcessor::FileState::ready)
    {
        auto tm = [] (double s) { return juce::String ((int) s / 60) + ":" + juce::String ((int) s % 60).paddedLeft ('0', 2); };
        g.setColour (col::ink); g.setFont (bold (18.0f));
        g.drawText (tm (proc.filePos.load()) + " / " + tm (proc.fileSeconds()), underDiscR.getX() + 98, underDiscR.getY() + 8, 110, 34, juce::Justification::centredLeft);
    }
    else if (fs == LacquerProcessor::FileState::loading || fs == LacquerProcessor::FileState::rendering)
        g.drawText ("Mastering " + proc.fileName() + "  " + juce::String ((int) (proc.fileProgress.load() * 100)) + " %", underDiscR, juce::Justification::centred);
    else if (ls == LacquerProcessor::LearnState::listening)
        g.drawText ("Cutting what it hears: " + juce::String ((int) proc.learnedSeconds()) + " s" + (proc.learnedSeconds() < 30.0 ? juce::String (" (30 s is enough for a quick master)") : juce::String (", keep going for the whole song")), underDiscR, juce::Justification::centred);
    else if (proc.hasPlan())
    {
        const auto in = proc.inputCopy();
        g.drawText ("Learned from " + juce::String (juce::roundToInt (in.seconds)) + " s of audio. The groove shows the master's level over time.", underDiscR, juce::Justification::centred);
    }
    else
        g.drawText ("Press Learn and play your song, or drop an audio file here.", underDiscR, juce::Justification::centred);

    if (dragOver)
    {
        g.setColour (col::cut.withAlpha (0.12f)); g.fillRect (0, barH, baseW, baseH - barH);
        g.setColour (col::cut); g.drawRect (4, barH + 4, baseW - 8, baseH - barH - 8, 3);
        g.setFont (bold (30.0f)); g.drawText ("Drop to master this file", 0, barH, baseW, 80, juce::Justification::centred);
    }
}
