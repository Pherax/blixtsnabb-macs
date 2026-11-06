# Controller behaviour

How the ESPHome component (`macs_hob`) uses the protocol. These are
implementation choices (**Implemented** in [PROTOCOL.md](PROTOCOL.md) terms),
not protocol requirements.

## Modes

| Mode | Transmits | Shows in UI |
|---|---|---|
| **Listen** | Nothing. No ACKs, no configuration. | Panel tables and generator status |
| **Dev** | Only explicit one-shot test commands. No retries, no provisioning. The panel overwrites them on its next cycle. | Panel settings plus status |
| **Master** | Own tables, configuration, ACKs, supervision. Replaces the panel. | Commanded settings plus reported status |
| **Auto** | Boot policy: listen 35 s; if a panel frame (valid CRC) is seen → Dev, otherwise → Master (off). | Detection state until decided |

* Auto is chosen at boot only. Selecting Auto later requires a reboot.
* Once Auto has seen the panel in a session, a runtime switch to Master is
  refused. Remove the panel and reboot instead.
* Silence is not proof that the panel is absent (an idle panel sends every
  ~25 s). **Never run two controllers in Auto or Master on one bus.** Fix the
  second one to Listen.
* Master requires `allow_master: true` in the configuration.

## Master state progression

1. Start **off**, all levels zero.
2. Configure every board that asks (`21 02`), commit `24` and `64`.
3. **READY** when all four boards are configured, both tables are ACKed and
   fresh status has arrived from `24` and `64`.
4. An explicit **on** request switches the hob on; a separate level request
   starts heating.
5. **Stop** clears all levels and schedules off traffic. Stop frames take
   priority over everything queued.
6. A **fault** latches, clears all levels and attempts a stop. Resetting the
   fault re-initialises in the off state; previous levels are never restored.

An idle gap in status while fully off only removes READY temporarily; fresh
status restores it without a manual reset. Loss of status while on is a fault.

## Supervision

| Condition | Response |
|---|---|
| Pan absent (fresh status) on a zone above 0 | After 60 s, that zone goes to 0 |
| Pan returns before 60 s | Timer cancelled |
| Pan unknown while heating | Separate timer; fault after the communication timeout |
| Status stale while on | Fault, all levels cleared |
| Generator state other than `03` more than 3 s after on | Fault |
| Start-up not completed within 45 s | Fault |
| No ACK after 3 attempts | Fault |
| No new command for 30 min while a level is active | Levels cleared (command timeout) |
| API supervision enabled and lost for ~15 s | Fault |
| Shutdown or OTA | Best-effort zero tables and off broadcast (not verified by ACK) |

`communication_timeout` is 12 s (minimum 10 s). The public freshness limit for
displayed values is 60 s. These are different clocks, so a UI must show
readiness and faults alongside values.

For a left pair, a pan on either section keeps the pair's absence timer
cancelled. This is pair-level, not per-coil, supervision.

## Limits of a software stop

An off frame travels over the same wire that may have failed. Firmware cannot
guarantee a shutdown through an open wire, a crashed processor or a generator
that stops listening. `on_shutdown` does not run on sudden power loss or every
watchdog reset. How long a generator keeps heating without fresh tables has
**not been measured**. Treat the hob's own safety functions as the last line,
and do not leave an ESP-controlled zone unattended.
