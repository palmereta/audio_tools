#pragma once
// Página SEQUENCER: edita los 100 pasos del 101 en una grilla de notas, con acento, slide,
// ligadura y probabilidad por paso, y herramientas que el original no tiene (largo, rotar,
// transponer, generar, mutar, exportar a .mid). La carga desde el teclado (LOAD) sigue en el panel.

#include <JuceHeader.h>
#include "../../../../shared/ui/Showing.h"
#include "../PluginProcessor.h"
#include "Widgets101.h"

namespace dh {

class SeqView101 : public juce::Component, private juce::Timer
{
public:
    explicit SeqView101 (AT101VProcessor& p) : proc (p)
    {
        setSize ((int) Assets101::get().viewWidth(), (int) Assets101::get().viewHeight());
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const int body = juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("body")->load());
        w101::backdrop (g, getLocalBounds().toFloat(), body);
        seq = proc.getSequence();
        const int playing = proc.panelValue (P::seq_play) > 0.5f ? proc.stepPos.load() : -1;

        // ── barra de herramientas ──
        const auto tb = juce::Rectangle<float> (34.0f, 30.0f, getWidth() - 68.0f, 96.0f);
        w101::frame (g, tb, {}, body);
        g.setColour (col101::silk); g.setFont (col101::font (30.0f));
        g.drawText ("SEQUENCER", tb.withTrimmedLeft (24.0f).withWidth (260.0f).withTrimmedBottom (34.0f), juce::Justification::bottomLeft);
        w101::silk (g, juce::String (seq.length) + " of 100 steps", tb.withTrimmedLeft (26.0f).withTrimmedTop (64.0f).withWidth (240.0f).withHeight (18.0f),
                    11.5f, juce::Justification::centredLeft, col101::silkDim);
        for (auto& b : buttons ())
        {
            const bool down = pressed == b.id;
            w101::creamButton (g, b.r, down, b.label, b.dark);
            if (b.led >= 0) w101::led (g, { b.r.getCentreX(), b.r.getY() - 9.0f }, ledState (b.led));
        }
        w101::display (g, lengthDisplay(), juce::String (seq.length));
        w101::silk (g, "LENGTH", lengthDisplay().translated (0, 40.0f).withHeight (14.0f), 9.5f);

        // ── páginas y octavas ──
        for (int pg = 0; pg < 4; ++pg)
        {
            const auto r = pageRect (pg);
            w101::creamButton (g, r, page == pg, {}, true);
            const int a = pg * kCols + 1, b = std::min (100, (pg + 1) * kCols);
            w101::silk (g, juce::String (a) + "-" + juce::String (b), r.translated (0, page == pg ? 1.2f : 0.0f), 10.0f);
            w101::led (g, { r.getRight() + 9.0f, r.getCentreY() }, page == pg);
        }
        {
            auto r = followRect();
            w101::creamButton (g, r, follow, {}, true);
            w101::silk (g, "FOLLOW", r.translated (0, follow ? 1.2f : 0.0f), 10.0f);
            w101::led (g, { r.getRight() + 9.0f, r.getCentreY() }, follow);
            w101::creamButton (g, octRect (0), pressed == "oct_dn", {}, true);
            w101::creamButton (g, octRect (1), pressed == "oct_up", {}, true);
            w101::silk (g, "-", octRect (0), 14.0f); w101::silk (g, "+", octRect (1), 14.0f);
            w101::display (g, octDisplay(), noteName (lowNote));
            w101::silk (g, "LOWEST ROW", octDisplay().withX (octDisplay().getRight() + 10.0f).withWidth (90.0f), 9.5f, juce::Justification::centredLeft);
        }

        // ── grilla ──
        const auto grid = gridRect();
        w101::frame (g, grid.expanded (10.0f, 26.0f).withTrimmedBottom (16.0f), {}, body);
        g.setColour (juce::Colour (0xff131314)); g.fillRect (grid.expanded (1.0f));
        const float cw = grid.getWidth() / (float) kCols, rh = grid.getHeight() / (float) kRows;
        // Teclas a la izquierda.
        for (int r = 0; r < kRows; ++r)
        {
            const int note = lowNote + (kRows - 1 - r);
            const bool black = juce::MidiMessage::isMidiNoteBlack (note);
            auto kr = juce::Rectangle<float> (grid.getX() - 64.0f, grid.getY() + r * rh, 56.0f, rh - 1.0f);
            g.setColour (black ? juce::Colour (0xff141415) : juce::Colour (0xffd9d6cd));
            g.fillRoundedRectangle (kr, 1.5f);
            if (note % 12 == 0 || r == kRows - 1)
                w101::silk (g, noteName (note), kr.withTrimmedRight (4.0f), 9.5f, juce::Justification::centredRight, black ? col101::silk : juce::Colour (0xff222222));
        }
        // Celdas.
        for (int c = 0; c < kCols; ++c)
        {
            const int step = page * kCols + c;
            const bool inSeq = step < seq.length;
            const float x = grid.getX() + c * cw;
            for (int r = 0; r < kRows; ++r)
            {
                const int note = lowNote + (kRows - 1 - r);
                auto cell = juce::Rectangle<float> (x + 1.0f, grid.getY() + r * rh + 0.5f, cw - 2.0f, rh - 1.0f);
                juce::Colour base = juce::MidiMessage::isMidiNoteBlack (note) ? juce::Colour (0xff232325) : juce::Colour (0xff2c2c2f);
                if (c % 4 == 0) base = base.brighter (0.08f);
                if (! inSeq) base = base.interpolatedWith (juce::Colour (0xff1c1c1e), 0.7f);
                g.setColour (base); g.fillRect (cell);
            }
            if (step >= 100) { g.setColour (col101::editorBg.withAlpha (0.8f)); g.fillRect (x, grid.getY(), cw, grid.getHeight()); continue; }
            // Playhead (LED de paso arriba).
            w101::led (g, { x + cw * 0.5f, grid.getY() - 14.0f }, step == playing);
            if (step % 4 == 0) w101::silk (g, juce::String (step + 1), { x, grid.getBottom() + 3.0f, cw, 13.0f }, 9.0f, juce::Justification::centred, col101::silkDim);
            if (! inSeq) continue;
            const auto& s = seq.steps[(size_t) step];
            if (s.rest)
            {
                g.setColour (col101::silkDim.withAlpha (0.35f));
                g.fillRect (x + cw * 0.3f, grid.getCentreY() - 1.0f, cw * 0.4f, 2.0f);
                continue;
            }
            const int row = kRows - 1 - (s.note - lowNote);
            const float alpha = 0.45f + 0.55f * (float) s.prob / 100.0f;
            if (row < 0 || row >= kRows)
            {
                // Fuera de vista: una flecha en el borde.
                const float ay = row < 0 ? grid.getY() + 4.0f : grid.getBottom() - 4.0f;
                juce::Path a; a.addTriangle (x + cw * 0.5f, ay + (row < 0 ? -3.0f : 3.0f), x + cw * 0.35f, ay, x + cw * 0.65f, ay);
                g.setColour (col101::cream.withAlpha (0.6f)); g.fillPath (a);
                continue;
            }
            const bool joinNext = (s.tie || (step + 1 < seq.length && seq.steps[(size_t) step + 1].slide)) && step + 1 < seq.length;
            auto nr = juce::Rectangle<float> (x + 2.0f, grid.getY() + row * rh + 1.5f, cw - (joinNext ? 0.0f : 4.0f), rh - 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (nr.translated (0, 1.5f), 2.0f);
            juce::ColourGradient gr (col101::cream.brighter (0.15f).withAlpha (alpha), nr.getX(), nr.getY(), col101::cream.darker (0.2f).withAlpha (alpha), nr.getX(), nr.getBottom(), false);
            g.setGradientFill (gr); g.fillRoundedRectangle (nr, 2.0f);
            if (s.accent) { g.setColour (col101::orange); g.fillRect (nr.getX() + 3.0f, nr.getCentreY() - 1.0f, std::min (cw - 10.0f, nr.getWidth() - 6.0f), 2.2f); }
            if (s.prob < 100) w101::silk (g, juce::String ((int) s.prob), nr, 8.5f, juce::Justification::centred, juce::Colour (0xff3a2a20));
            // Slide: línea verde hacia la nota siguiente.
            if (step + 1 < seq.length && seq.steps[(size_t) step + 1].slide && ! seq.steps[(size_t) step + 1].rest && c + 1 < kCols)
            {
                const int r2 = kRows - 1 - (seq.steps[(size_t) step + 1].note - lowNote);
                if (r2 >= 0 && r2 < kRows)
                {
                    g.setColour (col101::green);
                    g.drawLine (nr.getRight() - 2.0f, nr.getCentreY(), x + cw + 3.0f, grid.getY() + r2 * rh + rh * 0.5f, 2.2f);
                }
            }
        }

        // ── carriles: acento, slide, ligadura, probabilidad ──
        static const char* laneNames[] = { "ACCENT", "SLIDE", "TIE", "PROB" };
        const juce::Colour laneCols[] = { col101::orange, col101::green, col101::yellow, col101::cream };
        for (int l = 0; l < 4; ++l)
        {
            const auto lr = laneRect (l);
            w101::silk (g, laneNames[l], { lr.getX() - 72.0f, lr.getY(), 62.0f, lr.getHeight() }, 10.5f, juce::Justification::centredRight);
            for (int c = 0; c < kCols; ++c)
            {
                const int step = page * kCols + c;
                const float x = lr.getX() + c * cw;
                auto cell = juce::Rectangle<float> (x + 3.0f, lr.getY() + 2.0f, cw - 6.0f, lr.getHeight() - 4.0f);
                g.setColour (step < seq.length ? juce::Colour (0xff262628) : juce::Colour (0xff1c1c1e));
                g.fillRoundedRectangle (cell, 2.0f);
                if (step >= seq.length) continue;
                const auto& s = seq.steps[(size_t) step];
                if (l < 3)
                {
                    const bool on = l == 0 ? s.accent : (l == 1 ? s.slide : s.tie);
                    if (on)
                    {
                        g.setColour (laneCols[l].withAlpha (0.25f)); g.fillRoundedRectangle (cell.reduced (1.0f), 2.0f);
                        g.setColour (laneCols[l]); g.fillRoundedRectangle (cell.reduced (cell.getWidth() * 0.22f, cell.getHeight() * 0.36f), 1.5f);
                    }
                }
                else
                {
                    const float h = (cell.getHeight() - 4.0f) * (float) s.prob / 100.0f;
                    g.setColour (s.prob < 100 ? col101::cream : col101::cream.withAlpha (0.45f));
                    g.fillRoundedRectangle (cell.getX() + 3.0f, cell.getBottom() - 2.0f - h, cell.getWidth() - 6.0f, h, 1.5f);
                }
            }
        }

        w101::silk (g, "Click a cell to place a note, click it again for a rest. Drag to paint. Shift-click a note to toggle accent. "
                       "LOAD on the panel records from the keyboard, as on the original.",
                    { 120.0f, (float) getHeight() - 40.0f, (float) getWidth() - 240.0f, 16.0f }, 10.0f, juce::Justification::centred, col101::silkDim);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        for (auto& b : buttons()) if (b.r.expanded (2.0f).contains (p)) { pressed = b.id; doButton (b.id); repaint(); return; }
        for (int pg = 0; pg < 4; ++pg) if (pageRect (pg).contains (p)) { page = pg; follow = false; repaint(); return; }
        if (followRect().contains (p)) { follow = ! follow; repaint(); return; }
        if (octRect (0).contains (p)) { pressed = "oct_dn"; lowNote = std::max (12, lowNote - 12); repaint(); return; }
        if (octRect (1).contains (p)) { pressed = "oct_up"; lowNote = std::min (96, lowNote + 12); repaint(); return; }
        if (lengthDisplay().contains (p)) { dragLength = true; lengthStartY = p.y; lengthStart = seq.length; return; }
        paintMode = -1;
        handleGrid (p, e.mods, true);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragLength)
        {
            const int len = juce::jlimit (1, 100, lengthStart + (int) ((lengthStartY - e.position.y) / 4.0f));
            proc.editSequence ([&] (Sequence& s) { setLength (s, len); });
            repaint();
            return;
        }
        handleGrid (e.position, e.mods, false);
    }

    void mouseUp (const juce::MouseEvent&) override { pressed = {}; dragLength = false; lastCell = { -1, -1 }; repaint(); }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (gridRect().contains (e.position))
        {
            wheelAcc += w.deltaY;
            if (std::abs (wheelAcc) > 0.12f) { lowNote = juce::jlimit (12, 96, lowNote + (wheelAcc > 0 ? 1 : -1)); wheelAcc = 0; repaint(); }
        }
    }

private:
    static constexpr int kCols = 32, kRows = 24;
    struct Btn { juce::String id, label; juce::Rectangle<float> r; int led = -1; bool dark = false; };

    AT101VProcessor& proc;
    Sequence seq;
    int page = 0, lowNote = 36, lastStep = -2;
    bool follow = true, dragLength = false;
    juce::String pressed;
    int paintMode = -1, lengthStart = 0;
    float lengthStartY = 0, wheelAcc = 0;
    juce::Point<int> lastCell { -1, -1 };
    juce::Random rng;
    int lastVersion = -1;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<float> gridRect() const { return { 120.0f, 210.0f, (float) getWidth() - 160.0f, 408.0f }; }
    juce::Rectangle<float> laneRect (int l) const
    {
        const auto gr = gridRect();
        return { gr.getX(), gr.getBottom() + 26.0f + (float) l * 34.0f, gr.getWidth(), l == 3 ? 44.0f : 30.0f };
    }
    juce::Rectangle<float> pageRect (int pg) const { return { 120.0f + (float) pg * 92.0f, 150.0f, 66.0f, 22.0f }; }
    juce::Rectangle<float> followRect() const { return { 120.0f + 4.0f * 92.0f + 20.0f, 150.0f, 70.0f, 22.0f }; }
    juce::Rectangle<float> octRect (int i) const { return { (float) getWidth() - 360.0f + (float) i * 36.0f, 150.0f, 28.0f, 22.0f }; }
    juce::Rectangle<float> octDisplay() const { return { (float) getWidth() - 280.0f, 148.0f, 60.0f, 26.0f }; }
    juce::Rectangle<float> lengthDisplay() const { return { 330.0f, 50.0f, 64.0f, 34.0f }; }

    std::vector<Btn> buttons() const
    {
        std::vector<Btn> b;
        float x = 412.0f; const float y = 60.0f, w = 40.0f, h = 18.0f;
        auto add = [&] (const char* id, const char* label, float gap = 52.0f, int led = -1) { b.push_back ({ id, label, { x, y, w, h }, led }); x += gap; };
        add ("len_dn", "LEN -"); add ("len_up", "LEN +", 76.0f);
        add ("shift_l", "SHIFT <"); add ("shift_r", "SHIFT >", 76.0f);
        add ("tr_dn", "TRANSP -"); add ("tr_up", "TRANSP +"); add ("oct_tr_dn", "OCT -"); add ("oct_tr_up", "OCT +", 76.0f);
        add ("random", "RANDOM"); add ("mutate", "MUTATE"); add ("clear", "CLEAR", 76.0f);
        add ("export", "EXPORT .MID", 90.0f);
        add ("load", "LOAD", 52.0f, 0); add ("play", "PLAY", 52.0f, 1);
        return b;
    }

    bool ledState (int i) const
    {
        if (i == 0) return proc.loadingLed.load();
        return proc.panelValue (P::seq_play) > 0.5f;
    }

    static void setLength (Sequence& s, int len)
    {
        len = juce::jlimit (0, Sequence::kMaxSteps, len);
        for (int i = s.length; i < len; ++i)
        {
            s.steps[(size_t) i] = {};
            s.steps[(size_t) i].rest = true;
            s.steps[(size_t) i].note = (int8_t) (i > 0 ? s.steps[(size_t) i - 1].note : 48);
        }
        s.length = len;
    }

    void doButton (const juce::String& id)
    {
        auto edit = [this] (auto fn) { proc.editSequence (fn); };
        if (id == "len_dn") edit ([] (Sequence& s) { setLength (s, std::max (1, s.length - 1)); });
        else if (id == "len_up") edit ([] (Sequence& s) { setLength (s, s.length + 1); });
        else if (id == "shift_l" || id == "shift_r")
            edit ([r = id == "shift_r"] (Sequence& s) {
                if (s.length < 2) return;
                if (r) std::rotate (s.steps.begin(), s.steps.begin() + s.length - 1, s.steps.begin() + s.length);
                else std::rotate (s.steps.begin(), s.steps.begin() + 1, s.steps.begin() + s.length);
            });
        else if (id.startsWith ("tr_") || id.startsWith ("oct_tr_"))
        {
            const int d = (id.endsWith ("up") ? 1 : -1) * (id.startsWith ("oct") ? 12 : 1);
            edit ([d] (Sequence& s) { for (int i = 0; i < s.length; ++i) s.steps[(size_t) i].note = (int8_t) juce::jlimit (0, 127, s.steps[(size_t) i].note + d); });
        }
        else if (id == "clear") edit ([] (Sequence& s) { s = Sequence(); setLength (s, 16); });
        else if (id == "random") edit ([this] (Sequence& s) { generate (s); });
        else if (id == "mutate") edit ([this] (Sequence& s) { mutate (s); });
        else if (id == "export") exportMidi();
        else if (id == "load")
        {
            if (proc.panelValue (P::seq_play) > 0.5f) proc.setPanelValue (P::seq_play, 0.0f);
            proc.sendCommand (AT101VProcessor::CmdLoadToggle);
        }
        else if (id == "play")
        {
            const bool on = proc.panelValue (P::seq_play) < 0.5f;
            if (on && proc.loadingLed.load()) proc.sendCommand (AT101VProcessor::CmdLoadToggle);
            if (on) proc.setPanelValue (P::arp_mode, 0.0f);
            proc.setPanelValue (P::seq_play, on ? 1.0f : 0.0f);
        }
    }

    // Línea de bajo al azar en escala menor pentatónica (o dórica), con acentos, slides y silencios.
    void generate (Sequence& s)
    {
        static const int pent[] = { 0, 3, 5, 7, 10 }, dor[] = { 0, 2, 3, 5, 7, 9, 10 };
        const bool usePent = rng.nextBool();
        const int len = s.length >= 8 ? s.length : 16;
        const int root = 36 + rng.nextInt (8);
        s = Sequence(); s.length = len;
        for (int i = 0; i < len; ++i)
        {
            auto& st = s.steps[(size_t) i];
            const int deg = usePent ? pent[rng.nextInt (5)] : dor[rng.nextInt (7)];
            const int oct = rng.nextFloat() < 0.22f ? 12 : (rng.nextFloat() < 0.08f ? 24 : 0);
            st.note = (int8_t) (root + deg + oct);
            if (i % 4 == 0 && rng.nextFloat() < 0.5f) st.note = (int8_t) (root + (rng.nextBool() ? 0 : 12));
            st.rest = i > 0 && rng.nextFloat() < 0.2f;
            st.accent = rng.nextFloat() < 0.25f;
            st.slide = i > 0 && ! st.rest && rng.nextFloat() < 0.15f;
            st.tie = ! st.rest && rng.nextFloat() < 0.06f;
        }
    }

    void mutate (Sequence& s)
    {
        if (s.length == 0) { generate (s); return; }
        for (int k = 0; k < std::max (2, s.length / 6); ++k)
        {
            auto& st = s.steps[(size_t) rng.nextInt (s.length)];
            switch (rng.nextInt (4))
            {
                case 0: st.note = (int8_t) juce::jlimit (0, 127, st.note + (rng.nextBool() ? 12 : -12)); break;
                case 1: st.accent = ! st.accent; break;
                case 2: st.slide = ! st.slide; break;
                default: st.rest = ! st.rest; break;
            }
        }
    }

    void exportMidi()
    {
        const auto s = proc.getSequence();
        if (s.length == 0) return;
        juce::MidiMessageSequence track;
        const int ticksPerStep = 24;          // 96 por negra, pasos de semicorchea
        int i = 0;
        while (i < s.length)
        {
            const auto& st = s.steps[(size_t) i];
            int len = 1;
            while (i + len < s.length && (s.steps[(size_t) i + len - 1].tie || s.steps[(size_t) i + len].slide) && ! s.steps[(size_t) i + len].rest
                   && s.steps[(size_t) i + len].note == st.note) ++len;
            if (! st.rest)
            {
                const double t0 = (double) (i * ticksPerStep), t1 = t0 + (double) (len * ticksPerStep) * (st.tie ? 1.0 : 0.5);
                track.addEvent (juce::MidiMessage::noteOn (1, st.note, (juce::uint8) (st.accent ? 127 : 90)), t0);
                track.addEvent (juce::MidiMessage::noteOff (1, st.note), t1);
            }
            i += len;
        }
        track.updateMatchedPairs();
        auto midiFile = std::make_shared<juce::MidiFile>();
        midiFile->setTicksPerQuarterNote (96);
        midiFile->addTrack (track);
        chooser = std::make_unique<juce::FileChooser> ("Export sequence as MIDI",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("AT101V Sequence.mid"), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [midiFile] (const juce::FileChooser& fc) {
                const auto f = fc.getResult();
                if (f == juce::File()) return;
                f.deleteFile();
                juce::FileOutputStream os (f.withFileExtension ("mid"));
                if (os.openedOk()) midiFile->writeTo (os);
            });
    }

    void handleGrid (juce::Point<float> p, const juce::ModifierKeys& mods, bool first)
    {
        const auto grid = gridRect();
        const float cw = grid.getWidth() / (float) kCols;
        const int c = (int) std::floor ((p.x - grid.getX()) / cw);
        if (c < 0 || c >= kCols) return;
        const int step = page * kCols + c;
        if (step >= 100) return;
        if (grid.contains (p))
        {
            const int r = (int) std::floor ((p.y - grid.getY()) / (grid.getHeight() / (float) kRows));
            const int note = lowNote + (kRows - 1 - juce::jlimit (0, kRows - 1, r));
            if (! first && lastCell == juce::Point<int> (step, note)) return;
            lastCell = { step, note };
            proc.editSequence ([&] (Sequence& s) {
                if (step >= s.length) setLength (s, step + 1);
                auto& st = s.steps[(size_t) step];
                if (mods.isShiftDown() && first) { st.accent = ! st.accent; paintMode = 2; return; }   // Shift: sólo el acento
                if (! first && paintMode == 2) return;                                                 // (el arrastre no pinta notas)
                if (first) paintMode = (! st.rest && st.note == note) ? 0 : 1;     // 0 = borrar, 1 = pintar
                if (paintMode == 0) { if (st.note == note) st.rest = true; }
                else { st.note = (int8_t) note; st.rest = false; }
            });
            if (paintMode == 1) audition (note);
            repaint();
            return;
        }
        for (int l = 0; l < 4; ++l)
        {
            const auto lr = laneRect (l);
            if (! lr.contains (p) || step >= seq.length) continue;
            if (! first && l < 3 && lastCell == juce::Point<int> (step, -10 - l)) return;
            lastCell = { step, -10 - l };
            proc.editSequence ([&] (Sequence& s) {
                auto& st = s.steps[(size_t) step];
                if (l == 3) { st.prob = (uint8_t) juce::jlimit (0, 100, (int) std::lround ((lr.getBottom() - 4.0f - p.y) / (lr.getHeight() - 8.0f) * 4.0f) * 25); return; }
                bool& flag = l == 0 ? st.accent : (l == 1 ? st.slide : st.tie);
                if (first) paintMode = flag ? 0 : 1;
                flag = paintMode == 1;
            });
            repaint();
            return;
        }
    }

    // Escucha la nota al ponerla (si el secuenciador no está corriendo).
    void audition (int note)
    {
        if (proc.panelValue (P::seq_play) > 0.5f) return;
        // Con LOAD (grabando pasos) o KEY TRANSPOSE armado, la nota de prueba entraría al motor como una tecla: agregaría
        // un paso o cambiaría la transposición. Ahí no se escucha.
        if (proc.loadingLed.load() || proc.keyTrArmed.load()) return;
        proc.keyboardState.noteOn (1, note, 0.7f);
        juce::Timer::callAfterDelay (160, [safe = juce::Component::SafePointer<SeqView101> (this), note] {
            if (safe != nullptr) safe->proc.keyboardState.noteOff (1, note, 0.0f);
        });
    }

    void timerCallback() override
    {
        if (! dh::reallyShowing (*this)) return;      // ventana escondida (Live la esconde sin cerrarla): sin trabajo (H3)
        const int pos = proc.panelValue (P::seq_play) > 0.5f ? proc.stepPos.load() : -1;
        if (follow && pos >= 0 && pos / kCols != page) page = pos / kCols;
        const int v = proc.seqVersion.load();
        if (pos != lastStep || v != lastVersion || proc.loadingLed.load()) { lastStep = pos; lastVersion = v; repaint(); }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqView101)
};

} // namespace dh
