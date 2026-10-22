#!/usr/bin/env python3
"""Biblioteca de patrones de Thrum: escribe assets/patterns/library.json.

    python3 plugins/thrum/tools/patterns.py

Rasgueo, un carácter por paso (ver Source/dsp/Performer.h): D/U abajo/arriba, d/u sólo las cuerdas agudas, M/m con la
palma, X/x muerto, B bajo, A bajo alternado, S abajo y cortado, | cortar todo, . nada. Acentos: 0 normal, 1 acento,
2 fuerte. Dedos: T bajo, A bajo alternado, 1..4 cuerdas desde la más aguda del voicing, T1 pinza, . silencio, > acento.
Patrones propios, escritos para este instrumento (géneros: rock, pop, funk, reggae, balada, post-punk y otros).
"""
import json, os

STRUM = [
    # nombre, género, pasos, acentos, división, swing (%)
    ("Eighths", "Rock", "DDDDDDDD", "20102010", "1/8", 50),
    ("Drive", "Rock", "D.DUDUDU", "20101010", "1/8", 50),
    ("Muted chug", "Rock", "DMMMDMMM", "20001000", "1/8", 50),
    ("Sixteenths", "Rock", "D.DUD.DUD.DUDUDU", "2000100010001000", "1/16", 50),
    ("Anthem", "Rock", "D...D.DUD...D.DU", "2000100020001000", "1/16", 50),
    ("Island", "Pop", "D.DU.UDU", "20001010", "1/8", 50),
    ("Push", "Pop", "D.DU.UD.", "20001010", "1/8", 50),
    ("Sixteen", "Pop", "D.D.DUD.D.D.DUDU", "2000100020001000", "1/16", 50),
    ("Half time", "Pop", "D...D.U.D...DUDU", "2000100020001000", "1/16", 50),
    ("Scratch", "Funk", "DxUxDxUDxUDxDxUx", "2000100010002000", "1/16", 54),
    ("Chop", "Funk", "S..xS..x.xS.S..x", "2000100000101000", "1/16", 54),
    ("Ghost sixteenths", "Funk", "XxXxDxXxXxXxDxXx", "0000200000002000", "1/16", 56),
    ("Disco", "Funk", "xUxUxUxUxUxUxUxU", "0101010101010101", "1/16", 50),
    ("One drop", "Reggae", "..S...S.", "00200020", "1/8", 50),
    ("Skank", "Reggae", ".u.u.u.u", "01010101", "1/8", 50),
    ("Double chop", "Reggae", "....SS......SS..", "0000210000002100", "1/16", 56),
    ("Slow strum", "Ballad", "D...D.DU", "20001010", "1/8", 50),
    ("Bass and strum", "Ballad", "B.d.u.d.", "20101010", "1/8", 50),
    ("Six-eight", "Ballad", "D.UD.UD.UD.U", "200100100100", "1/8T", 50),
    ("Downstroke drive", "Post-punk", "DDDDDDDDDDDDDDDD", "2010101020101010", "1/16", 50),
    ("Choppy", "Post-punk", "D.d.D.d.D.dUD.d.", "2000100020001000", "1/16", 50),
    ("Cold stabs", "Post-punk", "S..S..S.", "20020010", "1/8", 50),
    ("Night drive", "Dark wave", "D.D.D.DU", "20101000", "1/8", 50),
    ("Wall", "Shoegaze", "DUDUDUDU", "10101010", "1/8", 50),
    ("Boom chuck", "Country", "B.D.A.D.", "20101010", "1/8", 50),
    ("Train", "Country", "B.d.A.d.B.d.A.du", "2010101020101010", "1/16", 50),
    ("Folk strum", "Folk", "B.DUA.DU", "20102010", "1/8", 50),
    ("Down eights", "Punk", "MMMMMMMM", "11111111", "1/8", 50),
    ("Shuffle", "Blues", "DUDUDUDU", "20102010", "1/8", 67),
    ("Slow twelve-eight", "Blues", "D.UD.UD.UD.U", "200100200100", "1/8T", 50),
    ("Bossa", "Latin", "B..D..D.A.D..D..", "2001001020010010", "1/16", 50),
    ("Waltz", "Other", "BDD", "200", "1/4", 50),
]

ARP = [
    ("Travis", "Folk", "T 2 A 1 T 2 A 1", "1/8", 50),
    ("Travis pinch", "Folk", "T1 2 A 1 T 2 A1 2", "1/8", 50),
    ("Alternating bass", "Folk", "T 2 A 2 T 2 A 2", "1/8", 50),
    ("Forward roll", "Country", "T 3 2 1 T 3 2 1", "1/8", 50),
    ("Backward roll", "Country", "1 2 3 T 1 2 3 T", "1/8", 50),
    ("p-i-m-a", "Classical", "T 3 2 1 2 3", "1/8T", 50),
    ("Ballad arpeggio", "Ballad", "T 3 2 3 1 3 2 3", "1/8", 50),
    ("Pinch and roll", "Ballad", "T1 3 2 3 A1 3 2 3", "1/8", 50),
    ("Cascade", "Post-rock", "4 3 2 1 4 3 2 1 4 3 2 1 4 3 2 1", "1/16", 50),
    ("Night picking", "Dark wave", "T 1 3 1 A 1 3 1", "1/8", 50),
    ("Sixteenth arpeggio", "Pop", "T 3 2 1 2 3 2 1 A 3 2 1 2 3 2 1", "1/16", 50),
]

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "assets", "patterns", "library.json")
    for n, g, s, a, d, sw in STRUM:
        assert len(s) == len(a), (n, len(s), len(a))
    data = {
        "strum": [{"name": n, "genre": g, "steps": s, "accents": a, "div": d, "swing": sw} for n, g, s, a, d, sw in STRUM],
        "arp": [{"name": n, "genre": g, "steps": s, "div": d, "swing": sw} for n, g, s, d, sw in ARP],
    }
    with open(out, "w") as f:
        json.dump(data, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print(f"{out}: {len(STRUM)} patrones de rasgueo, {len(ARP)} de dedos")

if __name__ == "__main__":
    main()
