#pragma once
// Secuencia del AT101V: los 100 pasos del SH-101 (nota, silencio, ligadura) más lo que suma el
// editor (acento, slide y probabilidad por paso). El orden de arpegio también vive acá.

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace dh {

struct SeqStep
{
    int8_t note = 60;
    bool rest = false, tie = false, accent = false, slide = false;
    uint8_t prob = 100;            // 0..100 %

    bool operator== (const SeqStep& o) const
    {
        return note == o.note && rest == o.rest && tie == o.tie && accent == o.accent && slide == o.slide && prob == o.prob;
    }
};

struct Sequence
{
    static constexpr int kMaxSteps = 100;
    std::array<SeqStep, kMaxSteps> steps {};
    int length = 0;

    bool operator== (const Sequence& o) const
    {
        if (length != o.length) return false;
        for (int i = 0; i < length; ++i) if (! (steps[(size_t) i] == o.steps[(size_t) i])) return false;
        return true;
    }
    int firstNote() const
    {
        for (int i = 0; i < length; ++i) if (! steps[(size_t) i].rest) return steps[(size_t) i].note;
        return 60;
    }
};

// Orden del arpegio: las teclas ordenadas por altura (el 101 guarda un mapa de bits de las 32
// teclas), repetidas en las octavas que pida la extensión. U&D no repite los extremos.
class Arp
{
public:
    enum Mode { Off = 0, Down, UpDown, Up };

    void restart() { pos = -1; dir = 1; }

    int next (const std::vector<int>& keys, int mode, int octaves)
    {
        buf.clear();
        sorted.assign (keys.begin(), keys.end());
        std::sort (sorted.begin(), sorted.end());
        sorted.erase (std::unique (sorted.begin(), sorted.end()), sorted.end());
        for (int o = 0; o < std::max (1, octaves); ++o)
            for (int n : sorted) if (n + 12 * o <= 127) buf.push_back (n + 12 * o);     // sin pasar de la nota 127
        const int len = (int) buf.size();
        if (len == 0) return -1;
        if (mode == Up) pos = (pos + 1) % len;
        else if (mode == Down) pos = pos < 0 ? len - 1 : (pos - 1 + len) % len;
        else
        {
            if (len == 1) pos = 0;
            else if (pos < 0) { pos = 0; dir = 1; }
            else
            {
                pos += dir;
                if (pos >= len) { pos = len - 2; dir = -1; }
                else if (pos < 0) { pos = 1; dir = 1; }
            }
        }
        pos = std::clamp (pos, 0, len - 1);
        return buf[(size_t) pos];
    }

private:
    std::vector<int> buf = reserved (128), sorted = reserved (64);
    static std::vector<int> reserved (size_t n) { std::vector<int> v; v.reserve (n); return v; }
    int pos = -1, dir = 1;
};

} // namespace dh
