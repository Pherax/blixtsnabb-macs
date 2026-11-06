# Remote display: Seeed reTerminal Sticky

The kitchen remote is a Seeed reTerminal Sticky (800×480 e-paper with touch,
ESP32-S3) running ESPHome. It talks to the hob controller over the local
network. It is never connected to the MACS bus, and all bus scheduling and
supervision stay on the hob controller.

## State: encrypted UDP push

The hob controller broadcasts its state about once a second, and immediately on
every change, using ESPHome's `packet_transport` over UDP (port 18511) with
encryption and rolling codes. Four integer-packed values carry everything:

| Value | Bits |
|---|---|
| levels | 4 bits per zone: level + 1 (0 = unknown, 1…11 = level 0…10) |
| flags | 2 bits per zone: pan + 1 in bits 0–9, hot + 1 in bits 10–19 (0 = unknown, 1 = no, 2 = yes) |
| meta | bits 0–2 mode (1 Auto, 2 Listen, 3 Dev, 4 Master), 3–4 hob state (0 unknown, 1 off, 2 on), 5 Master, 6 ready, 7 fault |
| tick | seconds since boot, proves the data is live |

All values stay below 2²⁴, so they are exact in a float. The Sticky accepts
state only from the configured hob name with the shared key.

## Fallback: HTTP snapshot

When no push has arrived for 4 s (for example a router that filters broadcasts
between clients), the Sticky polls:

```
GET http://<hob-host>/text_sensor/Sticky%20snapshot
```

ESPHome returns an entity response whose `state` is itself a JSON snapshot:

| Field | Meaning |
|---|---|
| `schema` | 1 |
| `tick` | Hob controller `millis()` at generation (not a MACS counter) |
| `mode` | Component mode name |
| `ready`, `fault` | Master readiness, latched fault |
| `hob` | `On`, `Off`, `Unknown`, `Commanded on`, `Commanded off` |
| `levels[5]` | Bottom left, top left, middle, bottom right, top right |
| `pan[5]`, `hot[5]` | −1 unknown, 0 no, 1 yes |

Data counts as fresh for 8 s after the tick last advanced.

## Commands

Commands always use the ESPHome REST API, for example:

```
POST /button/Hob%20on/press
POST /button/Hob%20off/press
POST /number/Bottom%20right/set?value=2
```

These routes depend on the entity names and web-server version, not on the bus
protocol. An HTTP 200 is not a generator ACK and not proof of heating. The
Sticky shows a requested level as an outline until the hob reports it, and
reports "Not applied" after 4 s otherwise.

## Rules the display follows

* Never show "off" without fresh data. Show "unknown" instead.
* Remote controls are unavailable whenever their state is unknown.
* Reconnecting the display or WiFi never restores a nonzero level.
* Keep connection layers apart: WiFi up, hob reachable, fresh snapshot,
  controller READY, generator status fresh. A DNS or HTTP error is not a bus
  failure.
