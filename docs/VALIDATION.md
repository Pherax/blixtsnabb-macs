# Validation status

What has been tested on real hardware, and what has not. "Pass" means the
acceptance evidence below was observed at least once on at least one of the two
test units. It is not a qualification.

| Test | Acceptance evidence | Status |
|---|---|---|
| Frame parser and integrity | All vectors in PROTOCOL.md decode; bad XOR/CRC rejected | **Pass** (offline, `tools/macs_frame.py selftest`) |
| Identical behaviour across units | Config profile, tables and switch-on exchange byte-identical on two hobs | **Pass** (E9) |
| ACK and echo handling | Own echo never counts as ACK or as panel traffic | **Pass** (E8, E10) |
| Dev injection | Injected level ACKed, audible, overwritten by panel | **Pass** (E1, E2, E5) |
| Cold start in Master, panel removed | All 16 blocks + 2 commits ACKed, READY, starts off and at zero | **Pass** (E8) |
| Heating in Master | Middle at 3 with pan heats; pan detected (`10 15`) | **Pass** (E8) |
| Zone mapping | Pan per physical section moves the expected status byte | **Pass** for left sections (E4); right side by single-zone tests |
| Pan vs residual heat | Lift test separates the two bits | **Pass** on bottom right (E3); left sections open |
| Level lookup, middle | Levels 1–9 and P captured from panel | **Pass** (E7, E11) |
| Level lookup, right | 1, 2, 3, 6 captured; 4, P, 0 injected and ACKed | Partial |
| Level lookup, left | 1 and 2 captured | **Open** for 3–9 and P |
| Warm ESP restart | Hob powered, ESP restarts; no restored heat; correct detection | Open |
| Standby recovery | Off beyond status timeout; READY returns without latched fault | Open (implemented) |
| No-pan timeout | 60 s absence stops only that zone; return cancels | Open (implemented) |
| Stop priority | Stop during retry/configuration; no queued nonzero table restarts heat | Open (implemented) |
| Ownership conflict | Panel appearing during Master cannot lead to two controllers | Open |
| Loss of communication | Generator shuts down by itself; latency measured; no automatic resume | **Open — most important** |
| Network/display loss | Sticky or HA offline cannot affect bus scheduling; stale UI cannot enable heat | Open (by design) |
| Extended operation | All zones, boost limiting, thermal cycling, repeated power transitions | Open |

## Recording a test

For each run keep: firmware revision, hob unit, controller mode, physical
action, timestamps, raw RX/TX/ECHO/ACK lines, expected and observed outcome, and
an explicit pass/fail. A successful compile or a desktop test does not pass a
safety test.
