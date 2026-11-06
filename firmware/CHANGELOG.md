# Changelog

## r5.1 (6 November 2026)

* ESP32 (classic) supported next to the ESP32-C3: board and pins are substitutions in `induction.yaml`.
* Public default is `listen`; `auto`/`master` require `allow_master: "true"`.
* Fix: the configuration check that rejects `mode: master`/`auto` without `allow_master` never triggered (it compared the mode name with a number). The firmware already refused Master at runtime.
* Optional `packages/sticky-direct.yaml`: encrypted UDP state push to the reTerminal Sticky remote, plus the HTTP snapshot.

## Dashboard R8 / controller r5

Replace MACS/dashboard.js in your existing ESPHome folder, compile/install and hard-refresh the dashboard. Keep your existing YAML and secrets.

R8 adds a clear off appearance and one On/Off toggle. The surface and oak edge become muted when the firmware publishes Off or Commanded off. Power levels above zero are disabled until On is reported. The toggle is illuminated while on, with an explicit state caption. Commanded on/off remains labelled as commanded, not as confirmation of heating. Connection loss or an unknown state is not rendered as confirmed off.

Residual-heat and unknown-heat symbols remain visible while off. Power-off remains available while on even if Master readiness is lost. Stop all remains a separate priority action. No firmware protocol or safety logic changes.

Validation: JavaScript syntax and DOM/event tests passed. Tests include both toggle directions, no optimistic state changes, off dimming, hot indicators while off, unknown/disconnected gating, fault gating, and priority Stop. No physical hob or browser-rendering verification performed.

All previous unverified manual features remain disabled. Controller firmware is still R5.


## R5 — automatic standby status recovery (29 October 2026)

Revision `macs-r5-20261029`. Install induction.yaml and MACS/ together,
keep your secrets, compile/install with ESPHome, and hard-refresh the dashboard.

Fix: a status gap while commanded OFF with every requested level zero no longer
latches Generator status lost. OFF maintenance traffic continues. On and positive
levels remain locked until both generators supply fresh valid status; availability
then returns automatically, without reset. No On request is queued while stale.
The dashboard says "Master, standby - waiting for generator status" during a gap.

The 12-second timeout still latches a fault whenever the hob is commanded ON,
even with zero zone levels, or any nonzero level is requested. Existing faults,
ACK failure, counter regression, startup and configuration-refresh checks remain
unchanged. This fix does not silently clear faults from an active cooking session.

Test: switch off normally, leave idle several minutes, then turn on again.
Confirm no standby status fault. If the waiting message persists, retain RX/TX
logs; the firmware will not enable heating without fresh generator status.
Confirm revision in Details. This package contains source, not a compiled binary.
31 host scenarios, dashboard tests and ESPHome source generation were checked;
physical hardware operation and a full target build remain unverified here.

---

## R4 — supervised configuration refresh (29 October 2026)

Revision: `macs-r4-20261029`. This is ESPHome source firmware for the hob C3,
including the dashboard; no precompiled binary is included.

The captured `21 02` request with counter `0A6A` followed operating status
counter `09E2`. R3 treated every such request after READY as a reboot and
commanded OFF. R4 instead allows a bounded configuration refresh when both
generators have fresh valid status and that board has a known, non-regressing
counter. Existing requested levels remain unchanged. New positive level requests
are locked until the refresh is ACK-complete. OFF and Stop remain available.

Each refresh has an 8-second deadline; repeated requests do not extend it.
ACK exhaustion, stale status, unexpected operating state, or a counter regression
still latch a fault and clear heating requests. Counter rollover is supported.
A forward counter is supporting evidence, not proof that a board did not reset.
A request from a board without a counter baseline fails closed, including an
auxiliary board first seen only after READY. Send that log for review rather
than disabling the check. A Stop during refresh requires explicit Reset fault /
reinitialization before heating can be requested again, even if no fault text
was latched. No automatic restoration of previous levels is implemented.

## Install and supervised check

1. Copy `MACS/` and `induction.yaml` together into your ESPHome config directory.
   Preserve your existing `secrets.yaml`. The alternative `induction-dev.yaml`
   forces Dev; do not use that file for a panel-removed Master test.
2. Compile and install with ESPHome. Hard-refresh the dashboard. Confirm
   `macs-r4-20261029` in boot logs and dashboard Details. Allow at least 60 seconds
   and confirm the boot-success message before power cycling after OTA.
3. With the original panel disconnected and heating initially OFF, Auto should
   select Master after 35 seconds, finish configuration, and report READY.
4. During an attended low-level test, capture the next configuration request.
   Expect `refresh with status supervision`, followed by `configuration ACK
   complete`, with no FAULT and unchanged requested level. Do not use Boost to
   diagnose this change. Verify level 0 and Stop immediately afterward.
5. If any fault occurs, stop the test and retain logs from startup through the
   fault, including TX, RX and ACK lines. Do not repeatedly reset to keep heating.

The 60-second no-pan stop, four left-section dashboard, existing pair controls,
Dev behavior and GPIO configuration are retained. Full target compilation and
hardware operation of this revision are not verified here; see VALIDATION.txt.
This revision does not establish that removing the original controller is safe
for unattended use or that every original-controller protection is reproduced.

## Previous R3 changes

## R3 update — four left-section decoding (28 October 2026)

Install MACS/components/macs_hob/__init__.py, macs_hob.h, macs_hob.cpp and
MACS/dashboard.js together. Keep your Wi-Fi/OTA secrets. The main induction.yaml
uses Auto; induction-dev.yaml forces Dev for the current panel-connected tests.
Both now explicitly select components: [macs_hob] and enable MACS DEBUG logging.
Compile/install the firmware and hard-refresh the dashboard afterward.

The main hob graphic shows four left sections plus middle and two right zones.
Left sections show individual pan/heat state and their shared pair level.
Clicking either lower section selects bottom-left control; either upper section
selects top-left control. Details also lists individual section states. Default-mode bottom-left combines
p[3]/p[4] and top-left p[5]/p[6] in B5 03 status from address 0x24.
Right/middle mapping from 0x64 is unchanged. Raw left status shows both bytes,
e.g. status=05/00; logs show all four physical section bytes.

Each section masks 0x80 (hot flag) and 0x10 (undecoded flag) before classifying
pan codes: 05 present, 00 absent, everything else unknown.
A paired zone is present if either section is present, absent only if both are
absent, otherwise unknown. Non-operating generator state means pan unknown.
The 60-second Master timer uses this aggregate and does not resume heat when
a pan returns. Either section's 0x80 flag raises the zone hot flag; the left-side
physical meaning still requires validation. No heat flag is not proof of cold.

This mapping applies to default two-zone left grouping only. Flex/bridge
regrouping and independent control of four sections are not implemented.
The UI labels observed levels as bus setpoints: captures show these can follow
pan position and need not match remembered panel selections.
The hidden countdown row no longer appears in Dev mode.

Validation: 20 host control scenarios including captured pan-sweep replay,
unknown/heat aggregation, paired-zone timeout and unchanged right mapping;
dashboard DOM/CSS tests, both YAML configurations, and ESPHome source generation.
No new physical tests or full ESP32 target compilation were performed.
The R3 section-mapping tests used Dev with the original panel connected.

