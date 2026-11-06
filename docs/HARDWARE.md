# Hardware interface

> ⚠️ **An induction hob is a mains-powered appliance.** The logic on the panel
> connector is not proven to be isolated from mains. Read
> [Electrical safety](#electrical-safety) before you open the hob or connect
> anything.

## The panel connector

The touch panel connects to the power electronics with a small harness that
carries three signals:

| Signal | Measured |
|---|---|
| +5 V | ≈ 4.8 V, powers the panel logic |
| GND | Logic ground |
| IO | Single bidirectional data line, shared by all devices |

The data line **idles low (≈ 0 V)** and data bits are **pulses up to ≈ 5 V**.
Measured with an oscilloscope on two units while the original panel was
communicating.

The controller can be powered from the hob's own 5 V rail. An ESP32 board with
an on-board regulator works fine from it (measured 3.3 V stable on the logic
side while transmitting on WiFi).

## Interface circuit

Proven on two builds (ESP32-C3 SuperMini and ESP32 D1 mini):

```
                    3V3
                     |
                  +--+--------------+
  hob IO ---------| A    74LVC1G17  |
                  |      Schmitt    |Y |-------------> ESP RX  (inverted: true)
             +----| GND  buffer     |
             |    +-----------------+
             |
            GND (shared hob / ESP ground)

  ESP TX  ----|>|----- hob IO            (Schottky, stripe/cathode towards the bus;
              anode                        TX inverted: true)
```

**Receive.** An **SN74LVC1G17** (non-inverting Schmitt-trigger buffer, SOT-23-5)
powered from the ESP's 3.3 V. Its input tolerates 5 V, so it level-shifts the
5 V bus to 3.3 V logic. It does not invert, so the ESP's UART RX is set to
`inverted: true` because the bus idles low.

**Transmit.** The ESP's TX pin drives the bus through a Schottky diode with the
cathode (stripe) towards the bus. With `inverted: true` the TX pin idles low,
and the diode blocks, so the bus is left alone. For each logical 0 the pin goes
to 3.3 V and the diode pushes the bus to about 3 V, which the boards read as
high. This only works because the bus idles low.

**Echo.** Because TX and RX share the bus, every transmitted frame comes back on
RX. The firmware uses this as a self-check.

### Pins used

| Board | RX | TX | Notes |
|---|---|---|---|
| ESP32-C3 SuperMini | GPIO0 | GPIO1 | |
| ESP32 (classic, D1 mini form factor) | GPIO27 | GPIO25 | On a D1 mini, GPIO25/27 are on the inner header rows; the shield's printed labels are ESP8266 names. |

ESPHome UART configuration (both boards):

```yaml
uart:
  id: macs_uart
  rx_pin:
    number: GPIO27      # GPIO0 on the C3
    inverted: true
    mode: INPUT
  tx_pin:
    number: GPIO25      # GPIO1 on the C3
    inverted: true
  baud_rate: 9600
  data_bits: 8
  parity: EVEN
  stop_bits: 1
  rx_buffer_size: 4096
```

### Lessons from bring-up

These cost real time. Check them first.

* **Buffer vs. inverter.** The '1G17 (buffer) and '1G14 (inverter) look
  identical. With an inverter in the RX path, `rx_pin: inverted` must be the
  opposite value. Read the part number before reasoning about polarity.
* **Ground on the buffer.** A '1G17 with VCC but no GND connection still
  measures "3 V on VCC" against the ESP's ground, but its output only floats and
  picks up mains hum (≈ 0.6 V, 50 Hz). On a scope that hum looks like activity.
* **Echo is not proof of connection.** On a breadboard, TX → diode → buffer → RX
  stays intact even when the wire to the hob is loose. The ESP then hears its own
  frames but the boards never answer. Scope the hob side of the connector.
* **SOT-23-5 on a SOT-23-6 adapter.** Chip pin 5 (VCC) lands on adapter pad 6.
  Wire by the chip's pin numbers, not the adapter's.
* **Mains hum on a floating pin.** At 5 ms/div, a 20 ms periodic, ~0.6 V
  trapezoid is 50 Hz mains, not data. Real frames are dense bursts of square
  pulses with ≈ 104 µs bits, lasting ≈ 26 ms for a table.

## Electrical safety

* The 5 V rail and logic GND are **not established to be isolated from mains**
  or tied to protective earth. Opto-couplers visible elsewhere on the boards do
  not prove the harness is safe to earth.
* Do not connect an earth-referenced oscilloscope ground clip, or a USB cable to
  a mains-powered computer, to the hob's logic without establishing the
  isolation. Battery-powered or isolated instruments are the safe choice.
* Work on the hob only with its supply disconnected at the breaker. Live tests
  need a means to remove power independently of the controller.
* The diode is not an isolated driver and not a universal open-drain stage. It
  is a minimal level interface that worked on these two units.
* No electrical compliance claim is made. Modifying the appliance may void its
  warranty and approvals.

## Enclosure

A parametric OpenSCAD enclosure for the ESP32 D1 mini + shield stack is in
[`hardware/enclosure/macs_housing.scad`](../hardware/enclosure/macs_housing.scad).
Measure your stack and adjust the parameters at the top first. Print it in ASA/ABS or at
least PETG; the inside of a hob gets warm. Keep the ESP's antenna end away from
the metal chassis: WiFi signal inside the hob was around −60 to −77 dBm.
