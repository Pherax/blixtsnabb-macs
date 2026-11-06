#!/usr/bin/env python3
"""
Reference encoder/decoder for the MACS bus frames observed on the IKEA BLIXTSNABB
induction hob. Pure Python 3, no dependencies.

    python3 macs_frame.py selftest
    python3 macs_frame.py decode "C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 50 B1 46 F5 78"
    python3 macs_frame.py table --on --levels 0 0 6 3 0
    python3 macs_frame.py log capture.txt        # decode every hex frame found in an ESPHome log

Everything here mirrors docs/PROTOCOL.md. Values marked "unverified" there are
reproduced here as the implementation uses them, not as established facts.
"""
import re
import sys

MARK, ACK_MARK = 0xC9, 0x98
ADDR = {0x00: "broadcast", 0x2C: "panel", 0x24: "gen 0x24", 0x44: "gen 0x44",
        0x64: "gen 0x64", 0x84: "gen 0x84", 0x0C: "status sink"}

ENTRY_IDLE, ENTRY_ABSENT = 0x7E1, 0x000
GROUP_BOTTOM_LEFT, GROUP_TOP_LEFT = 0x7D1, 0x7D3
# index = level 0..10 (10 = P). Middle/right levels are captured; left above 2 is unverified.
LEVEL_LEFT = [ENTRY_IDLE, 0x008, 0x010, 0x01E, 0x02D, 0x03C, 0x058, 0x082, 0x0B7, 0x11F, 0x190]
LEVEL_RIGHT = [ENTRY_IDLE, 0x00B, 0x020, 0x03C, 0x05A, 0x078, 0x0B1, 0x104, 0x16F, 0x23F, 0x320]
ZONES = ["bottom left", "top left", "middle", "bottom right", "top right"]
ZONE_ENTRY = [0, 2, 5, 6, 7]

CONFIG = {  # opaque appliance profile, see docs/PROTOCOL.md section 9
    0x24: ["D0 AA 6C D2 CD 1E 4B 73", "D2 CD 1E 4B 73 0B 98 98", "7D BF B8 1E 44 33 33 64", "55 10 10 00 0C 00 0A 6A"],
    0x44: ["D0 AA 6C D2 CD 1E 4B 73", "D2 CD 1E 4B 73 0B 98 98", "7D BF B8 1E 44 33 33 64", "55 10 10 00 0C 00 0A 6A"],
    0x64: ["90 AA 6C 00 00 00 00 00", "D2 CF 1A 4B E6 0B AE 05", "3D BF B8 1E 66 33 33 44", "70 00 20 00 00 00 CD 8A"],
    0x84: ["98 AA 6C CD C8 0E 70 E6", "CD C8 0E 70 E6 0B 1E 36", "7D BF F8 1E 66 33 33 64", "77 20 20 00 00 00 C8 FA"],
}
COMMIT_BOARDS = (0x24, 0x64)


def xor_of(data):
    x = 0
    for b in data:
        x ^= b
    return x


def setpoint_crc(payload16):
    """CRC-16, poly 0x1021, init 0, MSB first, no reflection, final XOR 0x235F."""
    c = 0
    for b in payload16:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c ^ 0x235F


def frame(dst, src, payload):
    f = [MARK, dst, src, len(payload)] + list(payload)
    return f + [xor_of(f)]


def ack(frame_bytes):
    cs = frame_bytes[-1]
    return [ACK_MARK, cs, ACK_MARK ^ cs]


def entry_value(entry, on, levels):
    if not on:
        return ENTRY_ABSENT
    return {
        0: LEVEL_LEFT[levels[0]],
        1: GROUP_BOTTOM_LEFT if levels[0] else ENTRY_IDLE,
        2: LEVEL_LEFT[levels[1]],
        3: GROUP_TOP_LEFT if levels[1] else ENTRY_IDLE,
        5: LEVEL_RIGHT[levels[2]],
        6: LEVEL_RIGHT[levels[3]],
        7: LEVEL_RIGHT[levels[4]],
    }.get(entry, ENTRY_ABSENT)


def setpoint_table(base, on, levels=(0, 0, 0, 0, 0)):
    """base 0 -> entries 0-5, base 6 -> entries 6-11. levels: 0..10 per logical zone."""
    p = [0xB2, 0x00, 0x0B if on else 0x00, 0x00]
    for i in range(6):
        e = base + i
        v = entry_value(e, on, levels)
        p += [(e << 4) | ((v >> 8) & 0x0F), v & 0xFF]
    c = setpoint_crc(p)
    return frame(0x24, 0x2C, p + [c >> 8, c & 0xFF])


def state_broadcast(on):
    return frame(0x00, 0x2C, [0xB2, 0x03, 0x09 if on else 0x00, 0x00, 0xBB])


def config_block(dst, idx):
    return frame(dst, 0x2C, [0x21, 0x03, 0x21 + idx] + [int(x, 16) for x in CONFIG[dst][idx].split()])


def commit(dst):
    return frame(dst, 0x2C, [0x31, 0x00, 0x25, 0x00])


def hexs(b):
    return " ".join(f"{x:02X}" for x in b)


def parse_hex(s):
    return [int(x, 16) for x in s.split()]


def level_from_raw(raw, left):
    tbl = LEVEL_LEFT if left else LEVEL_RIGHT
    if raw in tbl:
        lv = tbl.index(raw)
        return "P" if lv == 10 else str(lv)
    if raw == ENTRY_ABSENT:
        return "absent"
    return f"unknown 0x{raw:03X}"


def status_byte(s):
    code = s & 0x6F
    pan = "pan" if code == 0x05 else "no pan" if code == 0x00 else f"code {code:02X}?"
    flags = [n for bit, n in ((0x80, "hot"), (0x10, "flag10")) if s & bit]
    return pan + ("" if not flags else " +" + "+".join(flags))


def decode(b):
    """Return a human-readable description of one frame (or ACK)."""
    if len(b) == 3 and b[0] == ACK_MARK:
        ok = b[2] == (ACK_MARK ^ b[1])
        return f"ACK for frame ending {b[1]:02X}" + ("" if ok else " (bad check byte)")
    if len(b) < 6 or b[0] != MARK:
        return "not a frame"
    dst, src, n = b[1], b[2], b[3]
    if len(b) != n + 5:
        return f"length mismatch: header says {n} payload bytes, got {len(b) - 5}"
    if xor_of(b) != 0:
        return "outer XOR invalid"
    p = b[4:4 + n]
    head = f"{ADDR.get(src, hex(src))} -> {ADDR.get(dst, hex(dst))}: "
    if n == 18 and p[:2] == [0xB2, 0x00]:
        crc_ok = setpoint_crc(p[:16]) == (p[16] << 8 | p[17])
        on = p[2] == 0x0B
        parts = []
        for i in range(6):
            e = p[4 + 2 * i] >> 4
            raw = ((p[4 + 2 * i] & 0x0F) << 8) | p[5 + 2 * i]
            if e in ZONE_ENTRY:
                z = ZONE_ENTRY.index(e)
                parts.append(f"{ZONES[z]}={level_from_raw(raw, z < 2) if on else 'off'}")
            elif e in (1, 3) and on:
                parts.append(f"entry{e}=" + {0x7D1: "pair-active", 0x7D3: "pair-active", ENTRY_IDLE: "idle"}.get(raw, f"0x{raw:03X}"))
        return head + f"setpoint table entries {p[4] >> 4}-{(p[4] >> 4) + 5}, hob {'on' if on else 'off'}" \
            + ("" if crc_ok else " (INNER CRC BAD)") + (": " + ", ".join(parts) if parts else "")
    if p[:2] == [0xB2, 0x03] and n == 5:
        return head + f"state broadcast: hob {'on' if p[2] == 0x09 else 'off' if p[2] == 0 else hex(p[2])}"
    if p[:2] == [0xB5, 0x03] and n == 9:
        st = {0x00: "off", 0x03: "operating", 0x07: "starting"}.get(p[2], hex(p[2]))
        labels = (["L1 bottom", "L2", "L3", "L4 top"] if src == 0x24
                  else ["S0 (undecoded)", "middle", "bottom right", "top right"] if src == 0x64 else ["S0", "S1", "S2", "S3"])
        secs = ", ".join(f"{labels[i]}: {status_byte(p[3 + i])}" for i in range(4))
        return head + f"status, generator {st}, counter 0x{p[7]:02X}{p[8]:02X}; {secs}"
    if p[:2] == [0x21, 0x02]:
        return head + f"configuration request (counter 0x{p[2]:02X}{p[3]:02X})"
    if p[:2] == [0x21, 0x03]:
        known = dst in CONFIG and 0x21 <= p[2] <= 0x24 and hexs(p[3:11]) == CONFIG[dst][p[2] - 0x21]
        return head + f"configuration block 0x{p[2]:02X}" + (" (matches profile)" if known else " (differs from profile)")
    if p == [0x31, 0x00, 0x25, 0x00]:
        return head + "commit"
    if p[:2] == [0xB2, 0x02]:
        return head + "setpoint query"
    return head + "undecoded payload " + hexs(p)


def selftest():
    vectors = {
        # on/off idle tables (captured on two hobs)
        "C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 57 E1 85 97 8E": setpoint_table(0, True),
        "C9 24 2C 12 B2 00 0B 00 67 E1 77 E1 80 00 90 00 A0 00 B0 00 C4 E7 59": setpoint_table(6, True),
        "C9 24 2C 12 B2 00 00 00 00 00 10 00 20 00 30 00 40 00 50 00 AE 08 D7": setpoint_table(0, False),
        "C9 24 2C 12 B2 00 00 00 60 00 70 00 80 00 90 00 A0 00 B0 00 A5 66 B2": setpoint_table(6, False),
        # middle 6 and middle P from the original panel; middle 3 from the ESP in Master
        "C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 50 B1 46 F5 78": setpoint_table(0, True, (0, 0, 6, 0, 0)),
        "C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 53 20 80 3E E7": setpoint_table(0, True, (0, 0, 10, 0, 0)),
        "C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 50 3C 06 D0 90": setpoint_table(0, True, (0, 0, 3, 0, 0)),
        # state broadcasts
        "C9 00 2C 05 B2 03 09 00 BB E3": state_broadcast(True),
        "C9 00 2C 05 B2 03 00 00 BB EA": state_broadcast(False),
        # configuration and commit
        "C9 24 2C 0B 21 03 21 D0 AA 6C D2 CD 1E 4B 73 E6": config_block(0x24, 0),
        "C9 64 2C 0B 21 03 24 70 00 20 00 00 00 CD 8A 9B": config_block(0x64, 3),
        "C9 84 2C 0B 21 03 23 7D BF F8 1E 66 33 33 64 4D": config_block(0x84, 2),
        "C9 24 2C 04 31 00 25 00 D1": commit(0x24),
        "C9 64 2C 04 31 00 25 00 91": commit(0x64),
    }
    fails = 0
    for expect, got in vectors.items():
        if parse_hex(expect) != got:
            fails += 1
            print("FAIL", expect, "!=", hexs(got))
    # bottom-right 1 and 2 vectors from the specification (inner CRC / outer XOR)
    for lv, crc, x in ((1, 0xA994, 0xAA), (2, 0xD99A, 0xFF)):
        t = setpoint_table(6, True, (0, 0, 0, lv, 0))
        if (t[20] << 8 | t[21], t[22]) != (crc, x):
            fails += 1
            print("FAIL bottom right", lv, hexs(t))
    if ack(parse_hex("C9 24 2C 12 B2 00 00 00 00 00 10 00 20 00 30 00 40 00 50 00 AE 08 D7")) != [0x98, 0xD7, 0x4F]:
        fails += 1
        print("FAIL ack")
    if xor_of([0x98, 0x93, 0x0B]) != 0:
        fails += 1
        print("FAIL documented ACK example 98 93 0B")
    print("selftest:", "OK" if not fails else f"{fails} failure(s)", f"({len(vectors) + 4} checks)")
    return fails == 0


def main(argv):
    if not argv or argv[0] == "selftest":
        return 0 if selftest() else 1
    if argv[0] == "decode":
        print(decode(parse_hex(" ".join(argv[1:]))))
        return 0
    if argv[0] == "table":
        on = "--on" in argv
        levels = (0, 0, 0, 0, 0)
        if "--levels" in argv:
            i = argv.index("--levels")
            levels = tuple(10 if v.upper() == "P" else int(v) for v in argv[i + 1:i + 6])
        for base in (0, 6):
            print(hexs(setpoint_table(base, on, levels)))
        return 0
    if argv[0] == "log":
        pat = re.compile(r"\b((?:C9|98)(?: [0-9A-F]{2}){2,})")
        for line in open(argv[1], encoding="utf-8", errors="replace"):
            m = pat.search(line)
            if m:
                print(m.group(1), "|", decode(parse_hex(m.group(1))))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
