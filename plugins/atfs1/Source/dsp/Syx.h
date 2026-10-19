#pragma once
// Bulks de sysex del FS1R: lee voces (608 bytes) y Fseqs (32 + 50·N bytes) de un .syx, y escribe una voz
// o un Fseq como bulk que acepta el original (y los editores que hablan su formato). Mapa de bytes de la
// lista de datos del FS1R (tablas 2 y 3); ver docs/hardware-spec.md §6.

#include "FsData.h"
#include "Fseq.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace dh::fs::syx {

struct Bulk { uint8_t hi = 0, mid = 0, lo = 0; std::vector<uint8_t> data; };

// Separa los mensajes F0 43 0n 5E ... F7. Los Fseq no respetan el contador de bytes: se toma todo hasta F7.
inline std::vector<Bulk> parse (const std::vector<uint8_t>& raw)
{
    std::vector<Bulk> out;
    for (size_t i = 0; i < raw.size(); ++i)
    {
        if (raw[i] != 0xF0) continue;
        size_t j = i + 1;
        while (j < raw.size() && raw[j] != 0xF7) ++j;
        if (j >= raw.size()) break;
        // F0 43 0n 5E bh bl ah am al <data> cs F7
        bool clean = true;      // un byte de estado en el medio: el mensaje está roto
        for (size_t k = i + 1; k < j; ++k) if (raw[k] >= 0x80) { clean = false; break; }
        if (clean && j - i >= 11 && raw[i + 1] == 0x43 && (raw[i + 2] & 0xF0) == 0x00 && raw[i + 3] == 0x5E)
        {
            Bulk b; b.hi = raw[i + 6]; b.mid = raw[i + 7]; b.lo = raw[i + 8];
            b.data.assign (raw.begin() + (long) i + 9, raw.begin() + (long) j - 1);
            out.push_back (std::move (b));
        }
        i = j;
    }
    return out;
}

inline int s7 (uint8_t v) { return (int) v - 7; }      // −7..+7 con 7 = 0
inline int s64 (uint8_t v) { return (int) v - 64; }
inline int s50 (uint8_t v) { return (int) v - 50; }
inline std::string nameOf (const uint8_t* p, int n)
{
    std::string s;
    for (int i = 0; i < n; ++i) s += (p[i] >= 0x20 && p[i] < 0x7F) ? (char) p[i] : ' ';
    while (! s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

inline bool isVoice (const Bulk& b) { return ((b.hi >= 0x40 && b.hi <= 0x43) || b.hi == 0x51) && b.data.size() >= 608; }
inline bool isFseq (const Bulk& b) { return (b.hi == 0x60 || b.hi == 0x61 || b.hi == 0x70) && b.data.size() >= 32 + 50 * 8; }

inline VoiceData readVoice (const uint8_t* d)
{
    VoiceData v;
    v.name = nameOf (d, 10);
    v.category = d[0x0E];
    v.lfo1Wave = d[0x10] % 6; v.lfo1Speed = d[0x11]; v.lfo1Delay = d[0x12]; v.lfo1KeySync = d[0x13] & 1;
    v.lfo1Pmd = d[0x15]; v.lfo1Amd = d[0x16]; v.lfo1Fmd = d[0x17];
    v.lfo2Wave = d[0x18] % 6; v.lfo2Speed = d[0x19]; v.lfo2Phase = d[0x1C] & 3; v.lfo2KeySync = d[0x1D] & 1;
    v.noteShift = (int) d[0x1E] - 24;
    v.pegLevel = { s50 (d[0x1F]), s50 (d[0x20]), s50 (d[0x21]), s50 (d[0x3E]), s50 (d[0x22]) };
    v.pegTime = { d[0x23], d[0x24], d[0x25], d[0x26] };
    v.pegVel = d[0x27] & 7;
    const int fsV = ((d[0x28] & 1) << 7) | (d[0x29] & 0x7F), fsN = ((d[0x2A] & 1) << 7) | (d[0x2B] & 0x7F);
    v.algorithm = std::min<int> (87, d[0x2C]);
    for (int k = 0; k < 8; ++k) v.carrierAtt[(size_t) k] = d[0x2D + k] & 15;
    v.pegRange = d[0x3B] & 3; v.pegTimeScale = d[0x3C] & 7; v.feedback = d[0x3D] & 7;
    for (int k = 0; k < 5; ++k)
    {
        auto unpack = [] (uint8_t b, uint8_t depth) { KnobSlot s; s.dest = (b >> 4) & 3; s.unvoiced = (b >> 3) & 1; s.op = b & 7; s.depth = s64 (depth); return s; };
        v.formantKnob[(size_t) k] = unpack (d[0x40 + k], d[0x45 + k]);
        v.fmKnob[(size_t) k] = unpack (d[0x4A + k], d[0x4F + k]);
    }
    v.filterType = d[0x54] % 6; v.resonance = (int) d[0x55] - 16; v.resVel = s7 (d[0x56]); v.cutoff = d[0x57];
    v.fegDepthVel = s7 (d[0x58]); v.lfo1FilterDepth = d[0x59]; v.lfo2FilterDepth = d[0x5A];
    v.keyScaleDepth = s64 (d[0x5B]); v.keyScaleBreak = d[0x5C]; v.inputGain = (int) d[0x5D] - 12;
    v.fegDepth = s64 (d[0x64]);
    v.fegLevel = { s50 (d[0x66]), s50 (d[0x67]), s50 (d[0x68]), s50 (d[0x65]) };
    v.fegTime = { d[0x69], d[0x6A], d[0x6B], d[0x6C] };
    v.fegAttackVel = (d[0x6E] >> 3) & 7; v.fegTimeScale = d[0x6E] & 7;

    for (int k = 0; k < 8; ++k)
    {
        const uint8_t* o = d + 112 + 62 * k;
        auto& op = v.op[(size_t) k];
        op.keySync = (o[0x00] >> 6) & 1; op.transpose = (int) (o[0x00] & 0x3F) - 24;
        op.coarse = o[0x01] & 0x1F; op.fine = o[0x02] & 0x7F; op.noteScale = o[0x03];
        op.bwBias = (int) ((o[0x04] >> 3) & 15) - 7; op.form = o[0x04] & 7;
        op.fixed = (o[0x05] >> 6) & 1; op.skirt = (o[0x05] >> 3) & 7; op.fseqTrack = o[0x05] & 7;
        op.bwRes = o[0x06]; op.detune = (int) o[0x07] - 15;
        op.feg = { s50 (o[0x08]), s50 (o[0x09]), o[0x0A], o[0x0B] };
        op.eg.level = { o[0x0C], o[0x0D], o[0x0E], o[0x0F] };
        op.eg.time = { o[0x10], o[0x11], o[0x12], o[0x13] };
        op.eg.hold = o[0x14]; op.eg.timeScale = o[0x15] & 7;
        op.level = o[0x16]; op.breakPoint = o[0x17]; op.leftDepth = o[0x18]; op.rightDepth = o[0x19];
        op.leftCurve = o[0x1A] & 3; op.rightCurve = o[0x1B] & 3;
        op.freqBias = s7 ((o[0x1F] >> 3) & 15); op.pitchMod = o[0x1F] & 7;
        op.freqMod = (o[0x20] >> 4) & 7; op.freqVel = s7 (o[0x20] & 15);
        op.ampMod = (o[0x21] >> 4) & 7; op.ampVel = s7 (o[0x21] & 15);
        op.egBias = s7 (o[0x22]);
        op.fseq = (fsV >> k) & 1;   // bit 0 = op 1 … bit 7 = op 8
        if (! op.fixed) op.fine = std::min (op.fine, 99);

        auto& u = v.nop[(size_t) k];
        const uint8_t* n = o + 0x23;
        u.transpose = (int) n[0] - 24; u.mode = (n[1] >> 5) & 3; u.coarse = n[1] & 0x1F; u.fine = n[2];
        u.noteScale = n[3]; u.bandwidth = n[4]; u.bwBias = s7 (n[5]);
        u.resonance = (n[6] >> 3) & 7; u.skirt = n[6] & 7;
        u.feg = { s50 (n[7]), s50 (n[8]), n[9], n[10] };
        u.level = n[11]; u.levelKeyScale = s7 (n[12]);
        u.eg.level = { n[13], n[14], n[15], n[16] };
        u.eg.time = { n[17], n[18], n[19], n[20] };
        u.eg.hold = n[21]; u.eg.timeScale = n[22] & 7;
        u.freqBias = s7 (n[23] & 15); u.freqMod = (n[24] >> 4) & 7; u.freqVel = s7 (n[24] & 15);
        u.ampMod = (n[25] >> 4) & 7; u.ampVel = s7 (n[25] & 15); u.egBias = s7 (n[26]);
        u.fseq = (fsN >> k) & 1;
    }
    return v;
}

inline std::vector<uint8_t> writeVoiceData (const VoiceData& v)
{
    std::vector<uint8_t> d (608, 0);
    auto u7 = [] (int x, int lo, int hi) { return (uint8_t) std::clamp (x, lo, hi); };
    for (int i = 0; i < 10; ++i) d[(size_t) i] = (uint8_t) (i < (int) v.name.size() && v.name[(size_t) i] >= 0x20 && v.name[(size_t) i] < 0x7F ? v.name[(size_t) i] : ' ');
    d[0x0E] = u7 (v.category, 0, 22);
    d[0x10] = u7 (v.lfo1Wave, 0, 5); d[0x11] = u7 (v.lfo1Speed, 0, 99); d[0x12] = u7 (v.lfo1Delay, 0, 99); d[0x13] = v.lfo1KeySync ? 1 : 0;
    d[0x15] = u7 (v.lfo1Pmd, 0, 99); d[0x16] = u7 (v.lfo1Amd, 0, 99); d[0x17] = u7 (v.lfo1Fmd, 0, 99);
    d[0x18] = u7 (v.lfo2Wave, 0, 5); d[0x19] = u7 (v.lfo2Speed, 0, 127); d[0x1C] = u7 (v.lfo2Phase, 0, 3); d[0x1D] = v.lfo2KeySync ? 1 : 0;
    d[0x1E] = u7 (v.noteShift + 24, 0, 48);
    d[0x1F] = u7 (v.pegLevel[0] + 50, 0, 100); d[0x20] = u7 (v.pegLevel[1] + 50, 0, 100); d[0x21] = u7 (v.pegLevel[2] + 50, 0, 100);
    d[0x3E] = u7 (v.pegLevel[3] + 50, 0, 100); d[0x22] = u7 (v.pegLevel[4] + 50, 0, 100);
    for (int i = 0; i < 4; ++i) d[(size_t) 0x23 + (size_t) i] = u7 (v.pegTime[(size_t) i], 0, 99);
    d[0x27] = u7 (v.pegVel, 0, 7);
    int fsV = 0, fsN = 0;
    for (int k = 0; k < 8; ++k) { fsV |= (v.op[(size_t) k].fseq ? 1 : 0) << k; fsN |= (v.nop[(size_t) k].fseq ? 1 : 0) << k; }
    d[0x28] = (uint8_t) ((fsV >> 7) & 1); d[0x29] = (uint8_t) (fsV & 0x7F); d[0x2A] = (uint8_t) ((fsN >> 7) & 1); d[0x2B] = (uint8_t) (fsN & 0x7F);
    d[0x2C] = u7 (v.algorithm, 0, 87);
    for (int k = 0; k < 8; ++k) d[(size_t) 0x2D + (size_t) k] = u7 (v.carrierAtt[(size_t) k], 0, 15);
    d[0x3B] = u7 (v.pegRange, 0, 3); d[0x3C] = u7 (v.pegTimeScale, 0, 7); d[0x3D] = u7 (v.feedback, 0, 7);
    for (int k = 0; k < 5; ++k)
    {
        auto pack = [&] (const KnobSlot& s) { return (uint8_t) (((s.dest & 3) << 4) | ((s.unvoiced & 1) << 3) | (s.op & 7)); };
        d[(size_t) 0x40 + (size_t) k] = pack (v.formantKnob[(size_t) k]); d[(size_t) 0x45 + (size_t) k] = u7 (v.formantKnob[(size_t) k].depth + 64, 0, 127);
        d[(size_t) 0x4A + (size_t) k] = pack (v.fmKnob[(size_t) k]); d[(size_t) 0x4F + (size_t) k] = u7 (v.fmKnob[(size_t) k].depth + 64, 0, 127);
    }
    d[0x54] = u7 (v.filterType, 0, 5); d[0x55] = u7 (v.resonance + 16, 0, 116); d[0x56] = u7 (v.resVel + 7, 0, 14); d[0x57] = u7 (v.cutoff, 0, 127);
    d[0x58] = u7 (v.fegDepthVel + 7, 0, 14); d[0x59] = u7 (v.lfo1FilterDepth, 0, 99); d[0x5A] = u7 (v.lfo2FilterDepth, 0, 99);
    d[0x5B] = u7 (v.keyScaleDepth + 64, 0, 127); d[0x5C] = u7 (v.keyScaleBreak, 0, 127); d[0x5D] = u7 (v.inputGain + 12, 0, 24);
    d[0x64] = u7 (v.fegDepth + 64, 0, 127);
    d[0x66] = u7 (v.fegLevel[0] + 50, 0, 100); d[0x67] = u7 (v.fegLevel[1] + 50, 0, 100); d[0x68] = u7 (v.fegLevel[2] + 50, 0, 100); d[0x65] = u7 (v.fegLevel[3] + 50, 0, 100);
    for (int i = 0; i < 4; ++i) d[(size_t) 0x69 + (size_t) i] = u7 (v.fegTime[(size_t) i], 0, 99);
    d[0x6E] = (uint8_t) (((v.fegAttackVel & 7) << 3) | (v.fegTimeScale & 7));
    for (int k = 0; k < 8; ++k)
    {
        uint8_t* o = d.data() + 112 + 62 * k;
        const auto& op = v.op[(size_t) k];
        o[0x00] = (uint8_t) ((op.keySync ? 0x40 : 0) | u7 (op.transpose + 24, 0, 48));
        o[0x01] = u7 (op.coarse, 0, 31); o[0x02] = u7 (op.fine, 0, 127); o[0x03] = u7 (op.noteScale, 0, 99);
        o[0x04] = (uint8_t) ((u7 (op.bwBias + 7, 0, 14) << 3) | (op.form & 7));
        o[0x05] = (uint8_t) ((op.fixed ? 0x40 : 0) | ((op.skirt & 7) << 3) | (op.fseqTrack & 7));
        o[0x06] = u7 (op.bwRes, 0, 99); o[0x07] = u7 (op.detune + 15, 0, 30);
        o[0x08] = u7 (op.feg.init + 50, 0, 100); o[0x09] = u7 (op.feg.attack + 50, 0, 100); o[0x0A] = u7 (op.feg.attackTime, 0, 99); o[0x0B] = u7 (op.feg.decayTime, 0, 99);
        for (int i = 0; i < 4; ++i) { o[0x0C + i] = u7 (op.eg.level[(size_t) i], 0, 99); o[0x10 + i] = u7 (op.eg.time[(size_t) i], 0, 99); }
        o[0x14] = u7 (op.eg.hold, 0, 99); o[0x15] = u7 (op.eg.timeScale, 0, 7);
        o[0x16] = u7 (op.level, 0, 99); o[0x17] = u7 (op.breakPoint, 0, 99); o[0x18] = u7 (op.leftDepth, 0, 99); o[0x19] = u7 (op.rightDepth, 0, 99);
        o[0x1A] = (uint8_t) (op.leftCurve & 3); o[0x1B] = (uint8_t) (op.rightCurve & 3);
        o[0x1F] = (uint8_t) ((u7 (op.freqBias + 7, 0, 14) << 3) | (op.pitchMod & 7));
        o[0x20] = (uint8_t) (((op.freqMod & 7) << 4) | u7 (op.freqVel + 7, 0, 14));
        o[0x21] = (uint8_t) (((op.ampMod & 7) << 4) | u7 (op.ampVel + 7, 0, 14));
        o[0x22] = u7 (op.egBias + 7, 0, 14);
        const auto& u = v.nop[(size_t) k];
        uint8_t* n = o + 0x23;
        n[0] = u7 (u.transpose + 24, 0, 48); n[1] = (uint8_t) (((u.mode & 3) << 5) | u7 (u.coarse, 0, 21)); n[2] = u7 (u.fine, 0, 127);
        n[3] = u7 (u.noteScale, 0, 99); n[4] = u7 (u.bandwidth, 0, 99); n[5] = u7 (u.bwBias + 7, 0, 14);
        n[6] = (uint8_t) (((u.resonance & 7) << 3) | (u.skirt & 7));
        n[7] = u7 (u.feg.init + 50, 0, 100); n[8] = u7 (u.feg.attack + 50, 0, 100); n[9] = u7 (u.feg.attackTime, 0, 99); n[10] = u7 (u.feg.decayTime, 0, 99);
        n[11] = u7 (u.level, 0, 99); n[12] = u7 (u.levelKeyScale + 7, 0, 14);
        for (int i = 0; i < 4; ++i) { n[13 + i] = u7 (u.eg.level[(size_t) i], 0, 99); n[17 + i] = u7 (u.eg.time[(size_t) i], 0, 99); }
        n[21] = u7 (u.eg.hold, 0, 99); n[22] = u7 (u.eg.timeScale, 0, 7);
        n[23] = u7 (u.freqBias + 7, 0, 14); n[24] = (uint8_t) (((u.freqMod & 7) << 4) | u7 (u.freqVel + 7, 0, 14));
        n[25] = (uint8_t) (((u.ampMod & 7) << 4) | u7 (u.ampVel + 7, 0, 14)); n[26] = u7 (u.egBias + 7, 0, 14);
    }
    return d;
}

// ── Fseq ──
inline float wordHz (int w) { return 440.0f * std::exp2 (((float) w - 26861.0f) / 1024.0f); }
inline int hzWord (float hz) { return std::clamp ((int) std::lround (26861.0f + 1024.0f * std::log2 (std::max (1.0f, hz) / 440.0f)), 0, 32766) & ~1; }

inline FseqData readFseq (const std::vector<uint8_t>& d)
{
    FseqData f;
    f.name = nameOf (d.data(), 8);
    f.loopStart = (d[0x10] << 7) | d[0x11];
    f.loopEnd = (d[0x12] << 7) | d[0x13];
    f.loopMode = d[0x14] & 1;
    f.noteAssign = d[0x18];
    const int fmt = d[0x1B] & 3;
    int frames = std::min ((fmt + 1) * 128, (int) ((d.size() - 32) / 50));
    const int endStep = (d[0x1E] << 7) | d[0x1F];
    if (endStep > 1 && endStep < frames) frames = endStep + 1;
    f.frames.resize ((size_t) frames);
    for (int i = 0; i < frames; ++i)
    {
        const uint8_t* p = d.data() + 32 + 50 * i;
        auto& fr = f.frames[(size_t) i];
        fr.pitch = wordHz (p[0] * 256 + p[1] * 2);
        for (int k = 0; k < 8; ++k)
        {
            fr.vFreq[(size_t) k] = wordHz (p[2 + k] * 256 + p[10 + k] * 2);
            fr.vLevel[(size_t) k] = p[18 + k] & 0x7F;
            fr.nFreq[(size_t) k] = wordHz (p[26 + k] * 256 + p[34 + k] * 2);
            fr.nLevel[(size_t) k] = p[42 + k] & 0x7F;
        }
    }
    f.loopStart = std::clamp (f.loopStart, 0, frames - 1); f.loopEnd = std::clamp (f.loopEnd, 0, frames - 1);
    return f;
}

inline std::vector<uint8_t> writeFseqData (const FseqData& f)
{
    const int fmt = std::clamp (((int) f.frames.size() - 1) / 128, 0, 3);
    const int total = (fmt + 1) * 128;
    std::vector<uint8_t> d ((size_t) (32 + 50 * total), 0);
    for (int i = 0; i < 8; ++i) d[(size_t) i] = (uint8_t) (i < (int) f.name.size() && f.name[(size_t) i] >= 0x20 && f.name[(size_t) i] < 0x7F ? f.name[(size_t) i] : ' ');
    d[0x10] = (uint8_t) ((f.loopStart >> 7) & 0x7F); d[0x11] = (uint8_t) (f.loopStart & 0x7F);
    d[0x12] = (uint8_t) ((f.loopEnd >> 7) & 0x7F); d[0x13] = (uint8_t) (f.loopEnd & 0x7F);
    d[0x14] = (uint8_t) (f.loopMode & 1); d[0x15] = 64; d[0x18] = (uint8_t) std::clamp (f.noteAssign, 0, 127); d[0x19] = 63;
    d[0x1B] = (uint8_t) fmt;
    const int last = std::max (0, std::min (total, (int) f.frames.size()) - 1);
    d[0x1E] = (uint8_t) ((last >> 7) & 0x7F); d[0x1F] = (uint8_t) (last & 0x7F);
    for (int i = 0; i < total; ++i)
    {
        const auto& fr = f.frames[(size_t) std::min (i, (int) f.frames.size() - 1)];
        uint8_t* p = d.data() + 32 + 50 * i;
        auto put = [] (uint8_t* hi, uint8_t* lo, float hz) { const int w = hzWord (hz); *hi = (uint8_t) ((w >> 8) & 0x7F); *lo = (uint8_t) ((w & 0xFF) >> 1); };
        put (p, p + 1, fr.pitch);
        for (int k = 0; k < 8; ++k)
        {
            put (p + 2 + k, p + 10 + k, fr.vFreq[(size_t) k]);
            p[18 + k] = i < (int) f.frames.size() ? (uint8_t) (fr.vLevel[(size_t) k] & 0x7F) : 0;
            put (p + 26 + k, p + 34 + k, fr.nFreq[(size_t) k]);
            p[42 + k] = i < (int) f.frames.size() ? (uint8_t) (fr.nLevel[(size_t) k] & 0x7F) : 0;
        }
    }
    return d;
}

// Mensaje completo con contador y suma de verificación (los Fseq de 512 cuadros desbordan el contador de
// 14 bits: el original lo ignora y se escribe truncado, como hacen sus propios volcados).
inline std::vector<uint8_t> wrap (uint8_t hi, uint8_t mid, uint8_t lo, const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> m { 0xF0, 0x43, 0x00, 0x5E };
    const int count = (int) data.size() & 0x3FFF;
    m.push_back ((uint8_t) ((count >> 7) & 0x7F)); m.push_back ((uint8_t) (count & 0x7F));
    m.push_back (hi); m.push_back (mid); m.push_back (lo);
    int sum = ((count >> 7) & 0x7F) + (count & 0x7F) + hi + mid + lo;
    for (auto b : data) { m.push_back (b & 0x7F); sum += b & 0x7F; }
    m.push_back ((uint8_t) ((128 - (sum & 0x7F)) & 0x7F));
    m.push_back (0xF7);
    return m;
}
inline std::vector<uint8_t> voiceSyx (const VoiceData& v) { return wrap (0x40, 0x00, 0x00, writeVoiceData (v)); }
inline std::vector<uint8_t> fseqSyx (const FseqData& f) { return wrap (0x60, 0x00, 0x00, writeFseqData (f)); }

} // namespace dh::fs::syx
