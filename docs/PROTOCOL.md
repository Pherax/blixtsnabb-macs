# MACS bus protocol — IKEA BLIXTSNABB induction hob

Protocol specification, version 1.1 (November 2026).

This document describes the communication between the touch control panel of
the IKEA BLIXTSNABB induction hob and its generator (power) boards, as observed
on the internal single-wire bus, and how the open ESPHome controller in this
project reproduces it. The bus is part of the Electrolux appliance platform and
is commonly called MACS.

Everything here was learned by passive capture and controlled tests on two
BLIXTSNABB units. Nothing comes from manufacturer documentation.

## Contents

1. [Evidence levels](#1-evidence-levels)
2. [Bus participants](#2-bus-participants)
3. [UART and frame structure](#3-uart-and-frame-structure)
4. [Checksums, ACK and echo](#4-checksums-ack-and-echo)
5. [Setpoint tables and state broadcast](#5-setpoint-tables-and-state-broadcast)
6. [Zone mapping and level encoding](#6-zone-mapping-and-level-encoding)
7. [Status reports, pan and residual heat](#7-status-reports-pan-and-residual-heat)
8. [Startup and board configuration](#8-startup-and-board-configuration)
9. [Appliance configuration profile](#9-appliance-configuration-profile)
10. [Timing and bus ownership](#10-timing-and-bus-ownership)
11. [Undecoded messages](#11-undecoded-messages)
12. [Reference algorithms](#12-reference-algorithms)

Notation: bytes are hexadecimal; offsets are zero-based. `f` is the complete
frame and `p` its payload, so `p[0] = f[4]`. Times are in milliseconds unless
stated otherwise.

---

## 1. Evidence levels

Every claim in this project carries one of these labels.

| Label | Meaning |
|---|---|
| **Observed** | Directly visible in serial captures, tied to a known physical action. |
| **Implemented** | What the controller firmware does. An implementation choice is not automatically a protocol requirement. |
| **Inferred** | A consistent interpretation of observations that still needs a discriminating test. |
| **Unverified** | Not established. Do not assume a value. |

Captures referenced as E1–E11 are listed in [CAPTURES.md](CAPTURES.md).

---

## 2. Bus participants

| Address | Role | Status |
|---|---|---|
| `00` | Global state broadcast destination | Observed |
| `2C` | Original touch panel (source). The controller uses this identity in Master mode. | Observed |
| `24` | Destination of both setpoint tables; configuration endpoint; status source for the four left sections | Observed |
| `44` | Configuration endpoint. Whether it is a separate physical generator is not established. | Observed / unverified |
| `64` | Configuration endpoint; status source for middle and right zones | Observed |
| `84` | Configuration endpoint | Observed |
| `0C` | Destination of `B5` status reports. Do not assume it is a separate physical device. | Observed |

The panel is the only command source. The generator endpoints receive tables
and configuration, and send status, configuration requests and ACKs.

---

## 3. UART and frame structure

| Parameter | Value |
|---|---|
| Bit rate | 9600 baud |
| Format | 8 data bits, even parity, 1 stop bit (8E1) |
| Wire time per byte | 11 bits / 9600 = 1.146 ms |
| Line idle level | Low (≈ 0 V); data pulses go to ≈ 5 V. See [HARDWARE.md](HARDWARE.md). |
| Frame marker | `C9` |
| ACK marker | `98` |

Because the line idles **low**, a standard UART must invert both directions
(`inverted: true` on RX and TX in ESPHome) when the interface itself does not
invert. **Observed** with an oscilloscope on both test units.

### Ordinary frame

| Offset | Field | Meaning |
|---|---|---|
| 0 | `C9` | Start of frame |
| 1 | Destination | Logical address |
| 2 | Source | Logical address |
| 3 | `N` | Payload length in bytes, including any inner CRC |
| 4 … N+3 | Payload | Message family, subtype, data |
| N+4 | XOR | XOR of all preceding bytes, starting with `C9` |

Total length is `N + 5`. No byte stuffing or escaping has been observed.

Example (hob on broadcast): `C9 00 2C 05 B2 03 09 00 BB E3`, meaning
destination `00`, source `2C`, length 5, payload `B2 03 09 00 BB`, XOR `E3`.

### Parser rules

* Accumulate bytes across UART reads; a read boundary is not a frame boundary.
* After a plausible `C9` header, wait for `N + 5` bytes and check the XOR.
* Only treat `98` as an ACK marker **outside** a delimited frame: payloads and
  checksums can contain `98` and `C9`.
* Resynchronise on bad length or XOR. Never publish partially decoded state.

The firmware accepts payload lengths 1–64 and drops an incomplete frame after
500 ms. Both are local limits, not measured protocol maxima. **Implemented.**

---

## 4. Checksums, ACK and echo

### Outer XOR

XOR of every byte before it, including `C9`. A valid frame XORs to zero. It
detects many transmission errors but is neither strong integrity protection nor
authentication.

### Inner setpoint CRC

The 18-byte setpoint payload (`B2 00 …`) ends with a CRC over `p[0..15]`:

* polynomial `0x1021`, MSB first, initial value `0x0000`, no reflection
* **final XOR `0x235F`** (this is not the standard CCITT-FALSE preset)
* written big-endian into `p[16]`, `p[17]`

This parameterisation reproduces every captured table. **Observed.**

| Vector | Inner CRC | Outer XOR |
|---|---|---|
| Hob on, all idle, entries 0–5 | `85 97` | `8E` |
| Hob on, all idle, entries 6–11 | `C4 E7` | `59` |
| Hob off, entries 0–5 | `AE 08` | `D7` |
| Hob off, entries 6–11 | `A5 66` | `B2` |
| Bottom right at 1, rest idle (entries 6–11) | `A9 94` | `AA` |
| Bottom right at 2, rest idle (entries 6–11) | `D9 9A` | `FF` |
| Middle at 6, rest idle (entries 0–5) | `46 F5` | `78` |

### ACK

```
98 CS (98 XOR CS)
```

`CS` is the outer XOR byte of the frame being acknowledged. Example: `98 93 0B`
acknowledges a frame ending in `93`. **Observed.**

An ACK has no source, destination or sequence number. It cannot identify its
sender, cannot distinguish two frames with the same checksum, and does not prove
that heating happened. The firmware allows one outstanding transaction and
accepts a matching ACK within 180 ms. **Implemented.**

### Echo

The bus is a single shared wire, so a transmitter receives its own frame back.
The firmware matches complete received byte sequences against its recent
transmissions (600 ms window) and excludes them from decoding and panel
detection. An echo proves that your own receive path works. It does **not**
prove that the frame reached the hob: on a breadboard interface the echo path
can stay intact while the wire to the hob is loose (E10).

---

## 5. Setpoint tables and state broadcast

### Setpoint table (`B2 00`)

| Payload offset | Content |
|---|---|
| `p[0..1]` | `B2 00`: subtype |
| `p[2..3]` | `0B 00` = hob on, `00 00` = hob off |
| `p[4..15]` | Six entries, two bytes each |
| `p[16..17]` | Inner CRC, big-endian |

Each entry is a 16-bit big-endian word: the **upper nibble is the entry index**
and the **lower 12 bits are the raw value**.

```
entry = hi >> 4
raw   = ((hi & 0x0F) << 8) | lo
```

The panel always sends two tables: entries 0–5, then entries 6–11. Both go from
`2C` to `24`. Each table is a complete half-table, not a single-zone command.
Validate consecutive entry indices before accepting a table.

Canonical tables (**Observed**, identical on both units):

```
on,  entries 0-5 : C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 57 E1 85 97 8E
on,  entries 6-11: C9 24 2C 12 B2 00 0B 00 67 E1 77 E1 80 00 90 00 A0 00 B0 00 C4 E7 59
off, entries 0-5 : C9 24 2C 12 B2 00 00 00 00 00 10 00 20 00 30 00 40 00 50 00 AE 08 D7
off, entries 6-11: C9 24 2C 12 B2 00 00 00 60 00 70 00 80 00 90 00 A0 00 B0 00 A5 66 B2
```

* Hob on, zone at 0: raw `7E1` ("idle").
* Hob off: raw `000` in every entry.
* Entries 4 and 8–11 are `000` in every capture.

Do not merge the two zero encodings.

### State broadcast (`B2 03`)

```
on : C9 00 2C 05 B2 03 09 00 BB E3
off: C9 00 2C 05 B2 03 00 00 BB EA
```

The meaning of `BB` is unknown; it is constant in every capture.

---

## 6. Zone mapping and level encoding

| Logical zone | Name | Setpoint entry | Status source and byte |
|---|---|---|---|
| 0 | Bottom left (pair) | 0, companion 1 | `24`, sections L1 + L2 |
| 1 | Top left (pair) | 2, companion 3 | `24`, sections L3 + L4 |
| 2 | Middle | 5 | `64`, S1 |
| 3 | Bottom right | 6 | `64`, S2 |
| 4 | Top right | 7 | `64`, S3 |

The left side is a flexible area with four physical sections, L1 (bottom) to L4
(top). In the default mode they form two logical zones. While a zone on the left
is active, its companion entry holds `7D1` (bottom pair) or `7D3` (top pair);
when idle it holds `7E1`. **Observed** for the values; the meaning "pair link"
is **inferred**. A diagram showing four sections does not establish that four
independent left zones or arbitrary Flex grouping can be commanded.

### Level lookup

Levels are lookup values, not a linear scale. P (boost) is encoded as level 10.

| Setting | Middle / right raw | Left raw | Status |
|---|---|---|---|
| 0, hob on | `7E1` | `7E1` | Observed |
| 1 | `00B` | `008` | Observed (both) |
| 2 | `020` | `010` | Observed (both) |
| 3 | `03C` | `01E` | Middle/right observed, left implemented |
| 4 | `05A` | `02D` | Middle/right observed, left implemented |
| 5 | `078` | `03C` | Middle/right observed, left implemented |
| 6 | `0B1` | `058` | Middle/right observed, left implemented |
| 7 | `104` | `082` | Middle/right observed, left implemented |
| 8 | `16F` | `0B7` | Middle/right observed, left implemented |
| 9 | `23F` | `11F` | Middle/right observed, left implemented |
| P | `320` | `190` | Middle/right observed, left implemented |

Middle levels 1–9 and P were captured from the panel (E7, E11). Bottom right 1,
2, 3 and 6 were captured; 4, P and 0 were injected and acknowledged (E1, E2,
E5). Left values above 2 come from the firmware table and still need per-level
confirmation on hardware.

**Inferred:** the left values are exactly half the right values (except level 1).
IKEA rates all five zones equally (2300 W, booster 3200 W), so the left value is
most likely per section, with two sections forming one zone. This fits the
numbers but has not been measured. The ratio between consecutive steps is
roughly 1.4–1.5, which suggests a power setpoint rather than an index. Do not
read these values as watts.

---

## 7. Status reports, pan and residual heat

```
C9 0C SS 09 B5 03 GS S0 S1 S2 S3 CH CL XX
```

| Field | Frame offset | Meaning |
|---|---|---|
| `B5 03` | 4–5 | Status subtype |
| `GS` | 6 | Generator state: `00` off, `03` operating, `07` seen during startup |
| `S0..S3` | 7–10 | Four section status bytes |
| `CH CL` | 11–12 | 16-bit big-endian counter, per endpoint, wraps at 65536 |
| `XX` | 13 | Outer XOR |

* Source `24`: S0..S3 = left sections L1 (bottom) … L4 (top).
* Source `64`: S1 = middle, S2 = bottom right, S3 = top right; S0 is undecoded.

Keep all four bytes. An early logger printed only three values for source `24`
and hid a pan on the bottom section (E6).

### Section status byte

| Value | Interpretation | Status |
|---|---|---|
| `00` | No pan, no hot flag | Observed |
| `05` | Pan detected | Observed |
| `80` | Hot (residual heat), no pan | Observed |
| `85` | Hot + pan | Observed |
| `10`, `15`, `95` | As above, plus an undecoded `0x10` flag | Observed |
| `04` and others | Transitional or undecoded; **not** "absent" | Observed |

Decoding used by the firmware (**Implemented**):

```
code = status & 0x6F      # strips 0x80 and 0x10
pan  = present if code == 0x05, absent if code == 0x00, otherwise unknown
hot  = status & 0x80
```

Pan state is only published while the generator state is `03`. For a left
pair: present if either section is present, absent only if both are absent,
otherwise unknown. Hot for a pair is the OR of both sections.

The bottom-right lift test (`85` → `80` → `85` while the display kept showing
"H") shows that pan detection and the hot flag are independent (E3). The hot flag
is not a calibrated surface temperature, and its absence does not mean "safe to
touch".

**The `0x10` flag.** Observed (E8, E11): as soon as any zone is set above 0,
source `24` reports `10` in L1 and L2, and source `64` reports `10` in S0, even
when those sections have no pan. With a pan on the middle zone, `64` S1 shows
`15`. The flag looks like "a zone on this generator is active". This is
**inferred** only.

Counters run at roughly 50 counts per second in several captures; no exact
clock is established. The firmware uses unsigned deltas and treats a jump of
`0x8000` or more after readiness as a counter regression (board restart).

---

## 8. Startup and board configuration

A generator board that has just powered up does nothing useful until it has been
configured. **Observed** on both units.

### Messages

```
configuration request : C9 2C SS 04 21 02 CH CL XX      (board -> panel)
configuration block   : C9 DD 2C 0B 21 03 ID b0..b7 XX  (panel -> board, ID = 21..24)
commit                : C9 DD 2C 04 31 00 25 00 XX      (panel -> 24 and 64 only)
setpoint query        : C9 2C SS 04 B2 02 CH CL XX      (board -> panel)
```

### Sequence with the original panel (E9)

1. In the first ~1.2 s after power-up, each board sends `21 02` three times
   (`24`, then `44`, `64`, `84`).
2. About 4 s after power-up the panel sends its two off tables. The boards ACK them
   and send `21 02` again.
3. The panel sends blocks 21–24 to `24`, then to `44`, then commits `24`;
   then blocks to `64`, commit `64`; then blocks to `84`. Each frame is ACKed.
4. `64` and `84` usually request and receive their configuration a second time.
5. The boards start sending `B5 03` status reports.

When only the panel or controller restarts while the boards stay powered, only
some boards may ask for configuration (E9, second capture). That is normal.

### Sequence in Master mode (Implemented, E8)

1. Enter Master: clear all levels; queue two off tables and an off broadcast.
2. Answer every `21 02` with blocks 21–24 for that board; ACK requests addressed
   to `2C`.
3. After block 24 is ACKed, send the commit to `24` and `64` only.
4. When all four boards are configured, both tables are ACKed and fresh `B5`
   status has arrived from `24` and `64`, the controller reports **READY**. Heating
   requests remain zero until explicitly set.

Boards keep answering tables with `21 02` until they are configured, even if no
one spoke to them for more than 35 s after power-up (E8). There is no short
configuration window to hit.

A `21 02` alone does not prove a board restarted; the panel also answers such
requests during operation. The firmware allows reconfiguration after READY only
with fresh status and a continuous counter, blocks new heating requests while it
runs, and faults if it takes longer than 8 s. **Implemented.**

---

## 9. Appliance configuration profile

Sent as payload `21 03 <block ID> <eight bytes>`. Identical on both test units,
byte for byte (E9). Treat it as an opaque, model-specific profile, not as
editable calibration.

| Destination | Block | Bytes |
|---|---|---|
| `24` | `21` | `D0 AA 6C D2 CD 1E 4B 73` |
| `24` | `22` | `D2 CD 1E 4B 73 0B 98 98` |
| `24` | `23` | `7D BF B8 1E 44 33 33 64` |
| `24` | `24` | `55 10 10 00 0C 00 0A 6A` |
| `44` | `21` | `D0 AA 6C D2 CD 1E 4B 73` |
| `44` | `22` | `D2 CD 1E 4B 73 0B 98 98` |
| `44` | `23` | `7D BF B8 1E 44 33 33 64` |
| `44` | `24` | `55 10 10 00 0C 00 0A 6A` |
| `64` | `21` | `90 AA 6C 00 00 00 00 00` |
| `64` | `22` | `D2 CF 1A 4B E6 0B AE 05` |
| `64` | `23` | `3D BF B8 1E 66 33 33 44` |
| `64` | `24` | `70 00 20 00 00 00 CD 8A` |
| `84` | `21` | `98 AA 6C CD C8 0E 70 E6` |
| `84` | `22` | `CD C8 0E 70 E6 0B 1E 36` |
| `84` | `23` | `7D BF F8 1E 66 33 33 64` |
| `84` | `24` | `77 20 20 00 00 00 C8 FA` |

Commit (`31 00 25 00`) goes to `24` and `64` only. Configuration frames carry
only the outer XOR, no inner CRC.

The two test units are three years apart and wired differently (one on two
phases, one on a single supply), yet send identical profiles. Power-phase wiring
is therefore not part of this profile. Other hardware revisions may differ: do
not mix blocks from different hobs or fill gaps by symmetry.

---

## 10. Timing and bus ownership

| Item | Value | Status |
|---|---|---|
| 23-byte setpoint frame on the wire | ≈ 26.4 ms | Calculated |
| 3-byte ACK on the wire | ≈ 3.4 ms | Calculated |
| ACK latency after frame end | typically 5–15 ms in logs | Observed (log timestamps, not wire timing) |
| Panel table cadence, hob on | ≈ 8 s, plus immediately on every change | Observed |
| Panel table cadence, hob off and idle | ≈ 25 s (sometimes ≈ 8 s) | Observed |
| Controller table refresh | 4 s (configurable, max 8 s) | Implemented |
| Controller state broadcast | 10 s | Implemented |
| Boot quiet window | 5 s | Implemented |
| Auto observation window | 35 s | Implemented |
| ACK timeout / attempts | 180 ms / 3 | Implemented |
| Quiet bus before transmit | 12 ms after RX, 30 ms after TX | Implemented (heuristic, not collision-proof) |

No generator watchdog threshold (how long a board keeps heating without a fresh
table) has been measured.

**Ownership.** The bus has no arbitration. Exactly one device may act as panel
(`2C`). The firmware's Auto mode listens for 35 s and becomes Master only if it
sees no panel traffic. The 35 s are needed because an idle panel only transmits
every ~25 s. Silence does not prove the panel is absent, so a second controller
on the same bus must be fixed to **Listen**. In Dev mode, an injected command is
replaced by the panel's next table within seconds (E1, E2, E5).

---

## 11. Undecoded messages

| Message | Direction | When | Status |
|---|---|---|---|
| `26 02` | `2C` → `24` | Right after hob on, twice | Observed on both units (E9, E11) |
| `26 03 00 01 00 01 02` + counter | `24` → `0C` | Reply to `26 02` | Observed on both units |
| `B6 02` | `2C` → `24` | Right after hob on | Observed on both units |
| `B5 02` | `2C` → `64` / `24` | Occasional, e.g. after a panel touch with hob off | Observed |

Example frames: `C9 24 2C 02 26 02 E7`, `C9 24 2C 02 B6 02 77`,
`C9 64 2C 02 B5 02 34`.

The controller does **not** send `26 02` or `B6 02`, and heating at level 3 on
the middle zone works without them (E8). Whether they matter for other levels,
zones, boost or power management is **unverified**.

Multi-zone, pause, bridge, timers, child lock and the Flex presets have not been
isolated on the bus. Do not enable writes for them without dedicated captures.

---

## 12. Reference algorithms

```python
def xor_of(data):
    x = 0
    for b in data:
        x ^= b
    return x

def setpoint_crc(p16):                      # p[0..15]
    c = 0
    for b in p16:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c ^ 0x235F

def ack(frame):
    return [0x98, frame[-1], 0x98 ^ frame[-1]]
```

Building a table: payload `B2 00 <0B|00> 00` + six entries, append
`setpoint_crc` (high byte first), prepend `C9 24 2C 12`, append the outer XOR.

[`tools/macs_frame.py`](../tools/macs_frame.py) implements all of this, including
a decoder for ESPHome logs. Its self-test reproduces every vector in this
document.
