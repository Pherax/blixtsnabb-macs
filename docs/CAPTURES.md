# Capture evidence

Each entry ties bus traffic to a physical action. Timestamps are the logging
device's clock, not synchronised wall time. Decode any frame with
`python3 tools/macs_frame.py decode "<bytes>"`.

| ID | What happened | Key frames | Conclusion |
|---|---|---|---|
| **E1** | Dev mode: controller injects bottom right = 4 | TX table ending `93`, ECHO, then `98 93 0B`; ≈ 1.9 s later the panel sends level 2 again | Injection is accepted at transport level; the panel restores its own setting on its next cycle |
| **E2** | Dev mode: controller injects bottom right = P | ACK `66`; boost audible; panel restores 1 | Boost responds to an injected table; no lasting ownership |
| **E3** | Pan lifted from and returned to bottom right with residual heat | `64` S2: `85` → `80` → `85`, display shows "H" throughout | Pan bit and hot bit are independent |
| **E4** | Pan moved bottom → top across the left side and back | `24` S0..S3 change position by position | Keep all four left bytes; pair them for the default two-zone UI |
| **E5** | Dev mode: controller injects a zone = 0 | ACK `59`; coil goes silent; panel restores 5 | Zero stops heating temporarily in Dev |
| **E6** | Pan on bottom-left section | Frame contains `05` in S0 while an old logger printed `00 00 00` | That logger dropped a status byte |
| **E7** | Middle zone stepped through levels | Middle raw `078` at 5, `23F` at 9, `320` at P | Levels are lookup values, not numbers |
| **E8** | Cold start, panel removed, ESP32 in Auto → Master | 35 s silence; `21 02` from each board after the first table; 16 blocks + 2 commits ACKed; `READY`; hob on; middle = 3 (`50 3C`); `64` reports `10 15`; pan heats | Full replacement start-up works without `26 02`/`B6 02`; boards do not time out waiting for configuration |
| **E9** | Same capture on two different hobs (3 years apart, one on two phases) | Config requests, all 16 blocks, commits, on/off tables and the `26 02` / `26 03 00 01 00 01 02` / `B6 02` switch-on exchange are identical | One appliance profile covers both units |
| **E10** | Second controller (C3 on breadboard) sees only ECHO, never an ACK | No RX from any board for minutes; works again after reseating a wire | Echo alone does not prove a bus connection |
| **E11** | Original panel: hob on, middle 6, then P, then off | `50 B1` (0x0B1), `53 20` (0x320); `64` S1 = `05`; `24` L1/L2 = `10` while middle heats | Confirms middle 6 and P values; `0x10` flag appears on other sections while a zone is active |

## Example: E11 decoded

```
C9 00 2C 05 B2 03 09 00 BB E3                         panel -> broadcast: hob on
C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 57 E1 85 97 8E   all idle
C9 24 2C 02 26 02 E7                                  panel -> 24: undecoded 26 02
C9 0C 24 08 26 03 00 01 00 01 02 BF 71                24 -> 0C: undecoded 26 03
C9 24 2C 02 B6 02 77                                  panel -> 24: undecoded B6 02
C9 0C 64 09 B5 03 03 00 05 00 00 04 0D 11             64 status: middle pan
C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 50 B1 46 F5 78   middle = 6
C9 0C 24 09 B5 03 03 10 10 00 00 03 0C 52             24 status: L1/L2 flag 0x10
C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 53 20 80 3E E7   middle = P
C9 00 2C 05 B2 03 00 00 BB EA                         panel -> broadcast: hob off
```

## What these captures do not establish

An ACK does not prove temperature, power or that a pan heats. Audible coil noise
is corroboration, not a power measurement. Successful runs do not cover every
generator restart, lost ACK, bus collision, thermal limit or all-zone load case.
"Master works" is functional evidence, not qualification of a replacement
controller.

## Contributing captures

Useful captures state:

* firmware revision and controller mode
* which hob (model number, age if known)
* the exact physical action and when it happened
* the raw RX/TX/ECHO/ACK lines at DEBUG level, not just decoded values

A new interpretation should cite a capture, update the affected tables in
[PROTOCOL.md](PROTOCOL.md) and the vectors in the tool, and say whether it
changes display-only decoding or bus writes.
