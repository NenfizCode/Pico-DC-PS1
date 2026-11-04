# Wiring

Electrical connections for the Dreamcast → PS1 adapter. For the protoboard
layout and soldering order, see [assembly.md](./assembly.md). For the firmware
side, see [`src/host_files/maple_host/README.md`](../src/host_files/maple_host/README.md).

Connections are named by connector pin number throughout. Wire colours differ
from cable to cable, so map your own cables first (see
[Mapping the Dreamcast cable](#mapping-the-dreamcast-cable)).

![Pico pin map](./pico-pin-map-dark.svg)

## Safety

- **Dreamcast pin 2 is +5V.** It connects to the regulator output and the Pico's
  VSYS pin only. It must never reach a GPIO; the RP2040 is not 5V tolerant.
- **PSX pin 5 is the console's 3.3V rail.** It is not used. Leave a short stub
  and cover it with heatshrink. A bare end touching ground shorts the
  console's logic rail.
- **Identify every wire with a multimeter before soldering.**

All signal lines are 3.3V logic on both sides (the Maple bus idles at 3.3V and
the PS1 pad bus is 3.3V), so no level shifting is needed.

## Pin map

| Pico pin | GPIO | Connection | Via |
|---|---|---|---|
| 14 | GPIO10 | Dreamcast pin 1, Data A (SDCKA) | 10 Ω |
| 15 | GPIO11 | Dreamcast pin 5, Data B (SDCKB) | 10 Ω |
| 25 | GPIO19 | PSX pin 1, DATA | direct |
| 26 | GPIO20 | PSX pin 2, CMD | direct |
| 27 | GPIO21 | PSX pin 6, ATT | direct |
| 29 | GPIO22 | PSX pin 7, CLK | direct |
| 31 | GPIO26 | PSX pin 9, ACK | direct |
| 28 | GND | PSX pin 4, signal ground | |
| 38 | GND | Common ground bus | |
| 39 | VSYS | Regulator 5V output | insulated link |

The PS1 pins (GPIO 19, 20, 21, 22, 26) are fixed in
`src/device_files/psx-device/psxSPI.pio`. The Maple pins are set by
`MAPLE_PIN_A` in `src/config.h`. Data B is always `MAPLE_PIN_A + 1`, so the
pair can be moved to any other free adjacent GPIOs.

GPIO24 is read at boot to choose the output. It's the Pico's internal USB
power sense, so there's nothing to wire: no USB power means the console is
powering the adapter (PS1 mode), USB power means a PC (USB mode).

Leave these free: GPIO 0/1 (UART debug console), 5–8, 16/17 and 18. The
firmware uses them in other modes.

## PlayStation side

Use a PS1/PS2 controller cable, for example one cut from an old DualShock. Pins
are numbered 1 to 9, left to right, looking at the face of the plug. Position 8
is empty on the plug, which tells you which end is pin 1. Use continuity mode
to match each wire to its pin.

| PSX pin | Signal | Connect to |
|---|---|---|
| 1 | DATA | Pico pin 25 (GPIO19) |
| 2 | CMD | Pico pin 26 (GPIO20) |
| 3 | 7.6V | Regulator input |
| 4 | GND | Pico pin 28 |
| 5 | 3.3V | Not connected. Short stub with heatshrink |
| 6 | ATT | Pico pin 27 (GPIO21) |
| 7 | CLK | Pico pin 29 (GPIO22) |
| 8 | (empty) | |
| 9 | ACK | Pico pin 31 (GPIO26) |

If the cable has a foil shield and a bare drain wire, cut them back at the
board end and insulate them. Pin 4 is the only ground needed.

DATA and ACK don't need pull-up resistors. They're open-drain lines and the
console provides the pull-ups.

## Dreamcast side

Use a Dreamcast controller extension cable and keep the female socket end.

### Plug pinout

On the standard pinout diagram of the male plug, pin 1 is at the top right.
The numbering alternates between rows: the top row is 1, 3, 5 and the bottom
row is 2, 4. The socket is the mirror image.

| DC pin | Signal | Reading on a powered Dreamcast |
|---|---|---|
| 1 | Data A (SDCKA) | 3.3V idle |
| 2 | +5V | 5.0V |
| 3 | GND | 0V |
| 4 | Sense (tied to ground) | 0V |
| 5 | Data B (SDCKB) | 3.3V idle |

Pins 3 and 4 are joined inside the Dreamcast, so both read as ground. It
doesn't matter which is which; both go to ground.

### Mapping the Dreamcast cable

1. Cut the extension cable. You now have a male half and a female half.
2. Optionally, plug the male half into a powered Dreamcast and measure its
   bare wires against the readings above.
3. Plug the male half into the female half. Use continuity mode to find which
   female wire matches each male plug pin.
4. Write the mapping down before you unplug them.

Without the male half, you can probe the socket's contacts directly against
the cut wires instead, remembering the socket is mirrored.

Before soldering, check: with nothing plugged into the socket, its +5V and GND
wires read open to each other. Once built and powered, socket pin 2 reads
5.0V against pin 3.

### Socket connections

| DC pin | Signal | Connect to |
|---|---|---|
| 1 | Data A | 10 Ω → Pico pin 14 (GPIO10) |
| 2 | +5V | Regulator output and VSYS. Never a GPIO |
| 3 | GND | Ground bus |
| 4 | Sense | Ground bus |
| 5 | Data B | 10 Ω → Pico pin 15 (GPIO11) |
| | Shield / drain | Ground bus |

If Data A and Data B end up swapped, the controller is never detected and the
LED keeps slow-blinking. Both lines are 3.3V behind 10 Ω, so swapping them back
is safe.

## Resistors

Fit a 10 Ω resistor in series with each Maple data line, at the Pico end.

They limit the current if the firmware and the controller ever drive a line in
opposite directions at the same time. Don't leave them out. A Dreamcast uses
36 Ω here; 10 Ω is enough because the Pico's output drivers add some
resistance of their own. If the bus is ever unreliable, try 33 Ω.

No pull-ups are needed on the Maple lines. The RP2040's internal pull-ups are
enough. If you do have trouble, add 1 kΩ from each data line to 3V3 (Pico
pin 36). Don't use 10 kΩ; it's too weak for a 2 Mbps bus.

## Power

The PS1 controller port provides 3.3V and 7.6V but no 5V. A Dreamcast
controller needs 5V, so a regulator makes 5V from the 7.6V line.

![Power section](./power-section-dark.svg)

```
 PSX pin 3 ──┬── 10µF ──┬── BA50DD0T ──┬── 100µF ──┬── DC pin 2 (+5V)
 (7.6V)      │   0.1µF  │   1: IN      │   0.1µF   │
             │          │   2: GND     │           └── Pico pin 39 (VSYS)
             │          │   3: OUT     │
 PSX pin 4 ──┴──────────┴──────────────┴──────────────── common ground
 (GND)

 PSX pin 5 (3.3V) ── not connected
```

### Regulator

ROHM BA50DD0T, TO220FP-3 package, 2.54 mm pitch.

| Pin | Function |
|---|---|
| 1 | Input |
| 2 | Ground |
| 3 | Output (5V) |

It accepts 3–25V in and has a 450 mV dropout, so 7.6V in leaves 2.6V of
headroom. A 7805 drops about 2V and would be marginal here. The adapter draws
well under 150 mA, about 0.4 W in the regulator, so no heatsink is needed.

The regulator is the warmest part on the board. TO220FP has an isolated tab,
so it can touch the case or poke through a slot in the lid to stay cool.

A linear regulator is used on purpose. A switching regulator would add noise
next to the Maple bus, and at this current efficiency doesn't matter.

### Capacitors

| Position | Value | Type |
|---|---|---|
| Input (pin 1 to pin 2) | 10 µF | Electrolytic |
| Input (pin 1 to pin 2) | 0.1 µF | Ceramic |
| Output (pin 3 to pin 2) | 100 µF | Electrolytic |
| Output (pin 3 to pin 2) | 0.1 µF | Ceramic |

The datasheet asks for at least 22 µF on the output; 100 µF also covers the
controller's inrush current. The ceramics stop the regulator oscillating and
only work if they're right at its pins.

### The console's 7.6V fuse

The 7.6V line is fused inside the console (PS601/PS606 on a PS1). This can
look like a fault in the adapter:

- A tripped fuse doesn't necessarily read 0V. It can leak a few volts (around
  3.3V), which the regulator passes straight through. If the regulator's input
  and output read the same low voltage, suspect the console's fuse, not the
  regulator.
- It can stay tripped while anything draws current through it, including the
  regulator. Unplug the adapter and let it cool.

To check the port: plug a DualShock into it and turn on vibration in a game
that supports it. Vibration runs off the same 7.6V line. If it vibrates, the
line is fine and the problem is in the adapter. If not, try another port or
another console. A dead 7.6V line with working data lines is a common PS1
fault.

Optional: 10–22 Ω in series with the 7.6V input, before the 10 µF, softens the
inrush current at plug-in, which is the most likely thing to trip an aged
fuse. At about 100 mA it costs under 2V of the 2.6V headroom.

### VSYS, not 3V3

The 5V goes to VSYS, so the Pico's own regulator makes its 3.3V and the
console's 3.3V rail is left alone. USB can stay connected while the adapter
runs in a powered console (useful for serial debugging), because the Pico's
VBUS diode blocks backfeed.

Avoid having USB connected while the adapter is plugged into a console that
is switched **off**. That puts USB voltage on the regulator's output with
nothing on its input. Either unplug from the console when using USB, or fit a
1N5819 Schottky diode from the regulator's output to its input. The diode is
recommended if the adapter will stay plugged into a console.

## Grounding

All grounds are one net: PSX pin 4, the Dreamcast ground, sense and shield
wires, regulator pin 2, the capacitor negative legs, and the Pico. On the
protoboard layout, everything except PSX pin 4 goes to a ground bus tied to
Pico pin 38, and PSX pin 4 goes to Pico pin 28. All of the Pico's GND pins are
connected together on the Pico.

## Before powering on

1. Test the power section first, with the VSYS link not fitted. Plug into the
   console, power on, and check the regulator output reads 5.0V. Only then fit
   the VSYS link.
2. Check continuity from every GPIO to the +5V net and to the 7.6V net. Both
   must be open.
3. Check Pico pin 39 to pin 38 reads open. The ground bus runs right next to
   the VSYS pin on the protoboard.
4. Check that PSX pin 5 and any drain wire are insulated.
5. Only connect or disconnect the adapter or the controller with the console
   off.
