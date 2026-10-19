#pragma once
// Generado por tools/layout.py — no editar a mano.

namespace dh::layout {

struct ExtKnobDef { const char* param; float x, y, radius, startDeg, endDeg; int steps; };

inline constexpr ExtKnobDef extKnobs[] = {
    { "voc_on", 88.0f, 118.0f, 25.7f, -40.0f, 40.0f, 2 },
    { "voc_bands", 206.0f, 118.0f, 25.7f, -60.0f, 60.0f, 4 },
    { "voc_gain", 324.0f, 118.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "voc_shift", 88.0f, 280.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "voc_q", 206.0f, 280.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "voc_hiss", 324.0f, 280.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "voc_attack", 88.0f, 442.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "voc_release", 206.0f, 442.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "voc_mix", 324.0f, 442.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "voices", 480.0f, 118.0f, 25.7f, -70.0f, 70.0f, 6 },
    { "unison", 598.0f, 118.0f, 25.7f, -60.0f, 60.0f, 4 },
    { "detune", 480.0f, 280.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "spread", 598.0f, 280.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "priority", 480.0f, 442.0f, 25.7f, -50.0f, 50.0f, 3 },
    { "retrigger", 598.0f, 442.0f, 25.7f, -40.0f, 40.0f, 2 },
    { "dly_on", 755.0f, 118.0f, 25.7f, -40.0f, 40.0f, 2 },
    { "dly_time", 873.0f, 118.0f, 25.7f, -150.0f, 150.0f, 12 },
    { "dly_fb", 755.0f, 280.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "dly_tone", 873.0f, 280.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "dly_mix", 755.0f, 442.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "rev_on", 1030.0f, 118.0f, 25.7f, -40.0f, 40.0f, 2 },
    { "rev_size", 1148.0f, 118.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "rev_damp", 1030.0f, 280.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "rev_mix", 1148.0f, 280.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "filter_rev", 1305.0f, 118.0f, 25.7f, -40.0f, 40.0f, 2 },
    { "analog", 1423.0f, 118.0f, 25.7f, -150.0f, 150.0f, 0 },
    { "mg_sync", 1541.0f, 118.0f, 25.7f, -40.0f, 40.0f, 2 },
    { "bend_range", 1305.0f, 280.0f, 25.7f, -150.0f, 150.0f, 13 },
    { "quality", 1423.0f, 280.0f, 25.7f, -40.0f, 40.0f, 2 },
    { "midi_out", 1541.0f, 280.0f, 25.7f, -60.0f, 60.0f, 4 },
    { "output", 1423.0f, 442.0f, 25.7f, -150.0f, 150.0f, 0 },
};

inline constexpr float extLedX = 384.0f, extLedY = 34.0f;

} // namespace dh::layout
