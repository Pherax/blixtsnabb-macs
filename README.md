# BLIXTSNABB MACS — open controller for the IKEA BLIXTSNABB induction hob

Reverse-engineered protocol and an ESPHome-based controller for the internal
**MACS** bus between the touch panel and the power boards of the IKEA
BLIXTSNABB induction hob (Electrolux platform). It lets an ESP32 monitor the
hob, and optionally replace the original touch panel, so the hob can be read and
controlled from Home Assistant or a dedicated remote display.

> ⚠️ **Read this first.** This project modifies a mains-powered cooking
> appliance. It is experimental, unofficial and comes without any warranty. A
> software stop cannot guarantee a hardware stop. Opening the hob may void its
> warranty and approvals. Never leave a hob controlled by this firmware
> unattended. You are responsible for your own installation. See
> [docs/HARDWARE.md#electrical-safety](docs/HARDWARE.md#electrical-safety).

## Status

| | |
|---|---|
| Passive monitoring (zone levels, pan detection, residual heat) | Working on two units |
| Full replacement of the touch panel ("Master") from a cold start | Working on two units: configuration, on/off, zone levels, pan detection |
| Remote display (reTerminal Sticky) | Working, push over encrypted UDP with HTTP fallback |
| Left-zone levels 3–9 and P, Flex/bridge, pause, timers, lock | Not verified or not implemented |
| Behaviour when communication is lost | **Not measured** |

See [docs/VALIDATION.md](docs/VALIDATION.md) for the full test status.

## How it works, in one minute

* One wire, 9600 baud, 8E1, idles low. Frames start with `C9` and end with an XOR.
* The panel (`2C`) sends two **setpoint tables** to board `24` every few
  seconds. Each zone level is a 12-bit lookup value in a fixed table entry, and
  each table carries a CRC-16 (poly `0x1021`, final XOR `0x235F`).
* Boards (`24`, `44`, `64`, `84`) **ACK** every frame (`98 cs ~cs`) and report
  pan detection and residual heat in **status** frames.
* After power-up the boards ask for an **appliance-specific configuration
  profile** (16 blocks). Whoever acts as panel must send it before anything
  heats.

Full details: [docs/PROTOCOL.md](docs/PROTOCOL.md).

```
$ python3 tools/macs_frame.py decode "C9 24 2C 12 B2 00 0B 00 07 E1 17 E1 27 E1 37 E1 40 00 50 B1 46 F5 78"
panel -> gen 0x24: setpoint table entries 0-5, hob on: bottom left=0, entry1=idle, top left=0, entry3=idle, middle=6
```

## Documentation

| Document | Contents |
|---|---|
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | Bus, framing, checksums, tables, levels, status, start-up, configuration profile, timing |
| [docs/HARDWARE.md](docs/HARDWARE.md) | Connector, interface circuit, pins, bring-up pitfalls, electrical safety |
| [docs/CONTROLLER.md](docs/CONTROLLER.md) | Controller modes, start-up state machine, supervision and its limits |
| [docs/CAPTURES.md](docs/CAPTURES.md) | Evidence behind every claim, with decoded frames |
| [docs/VALIDATION.md](docs/VALIDATION.md) | What is tested and what is still open |
| [docs/STICKY.md](docs/STICKY.md) | Remote display: push format, HTTP fallback, commands |
| [tools/macs_frame.py](tools/macs_frame.py) | Reference encoder/decoder, log decoder, self-test |

## Firmware

ESPHome external component `macs_hob` plus a ready configuration, in [`firmware/`](firmware/).

```
firmware/
  induction.yaml                 configuration: board, pins and mode at the top
  secrets.example.yaml           copy to secrets.yaml
  MACS/components/macs_hob/      the component (C++ and Python schema)
  MACS/dashboard.js              built-in web control panel (served by the ESP)
  packages/sticky-direct.yaml    optional: state push for the reTerminal Sticky remote
  packages/wifi-ap-only.example.yaml  optional: hob as its own WiFi access point
  tests/run.sh                   host tests (g++ and node)
  CHANGELOG.md
```

Requirements: ESPHome 2026.8 or newer, ESP-IDF framework, an ESP32 or ESP32-C3 and
the interface from [HARDWARE.md](docs/HARDWARE.md).

```bash
cd firmware
cp secrets.example.yaml secrets.yaml      # fill in WiFi and OTA password
esphome run induction.yaml                # starts in listen mode
```

Change `hob_mode` (and `allow_master`) in the substitutions only after reading
[CONTROLLER.md](docs/CONTROLLER.md). The configuration refuses `auto` and
`master` unless `allow_master: "true"` is set.

The controller exposes everything to Home Assistant through the ESPHome API and
serves its own control page at `http://<device>.local/`. The companion remote
display lives in a separate repository (**sticky-hob-remote**).

## Getting started

1. **Listen first.** Build the interface from [HARDWARE.md](docs/HARDWARE.md),
   flash `firmware/induction.yaml` as shipped (**listen** mode) and use the original panel normally.
   You should see tables, ACKs and status in the log. Decode them with
   `python3 tools/macs_frame.py log yourlog.txt`.
2. **Compare your hob.** Capture a cold start and a switch-on. If your
   configuration blocks differ from [the profile](docs/PROTOCOL.md#9-appliance-configuration-profile),
   **stop**: do not use Master mode with someone else's profile.
3. **Master only when supervised,** with the panel disconnected, a way to cut
   power independently, and a pan of water on a low level first.

Other Electrolux-platform hobs probably use a similar bus. Their addresses,
level tables and configuration profiles may differ. Captures from other models
are welcome; see [docs/CAPTURES.md](docs/CAPTURES.md#contributing-captures).

## Repository layout

```
README.md        this file
LICENSE          MIT
firmware/        ESPHome component, configuration, dashboard, tests
docs/            protocol, hardware, controller, captures, validation, display
tools/           macs_frame.py (no dependencies)
hardware/        enclosure/macs_housing.scad (parametric OpenSCAD)
```

## License

MIT, see [LICENSE](LICENSE). The license covers this repository's code and
documentation. It grants no rights to IKEA or Electrolux trademarks or firmware.

## Trademarks

IKEA and BLIXTSNABB are trademarks of Inter IKEA Systems B.V. Electrolux is a
trademark of AB Electrolux. This project is not affiliated with, endorsed by or
supported by either company. Names are used only to identify the hardware this
work is compatible with.
