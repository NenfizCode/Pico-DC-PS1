# Dreamcast stick → PlayStation 1/2 adapter — wiring map

**PlayStation side verified 2026-09-12** by tracing every plug pin to its Pico
pin on the working build (startup menu, Soul Edge, KOF 96 all respond). The
Dreamcast socket is from meter readings and in-use confirmation on the same
build.

---

## The one wire that kills the board

**Dreamcast pin 2 (+5V)** goes to the regulator output and to the socket. It
must never touch a GPIO. The RP2040 has no over-voltage tolerant I/O.

Everything else is safe at 3.3V:

- The Maple data lines idle at **3.3V** (measured), so GPIO connects directly.
- The PlayStation pad bus is **3.3V native**, so GPIO connects directly.

No level shifting anywhere in this design.

---

## Pico pin map

| Pico pin | GPIO | Goes to | Via |
|---|---|---|---|
| 14 | GPIO10 | DC socket — Data A (SDCKA) | 10Ω series |
| 15 | GPIO11 | DC socket — Data B (SDCKB) | 10Ω series |
| 13 | GND | Maple ground return | — |
| 25 | GPIO19 | PSX pin 1 — DATA (brown) | direct |
| 26 | GPIO20 | PSX pin 2 — CMD (orange) | direct |
| 27 | GPIO21 | PSX pin 6 — ATT/SEL (yellow) | direct |
| 29 | GPIO22 | PSX pin 7 — CLK (blue) | direct |
| 31 | GPIO26 | PSX pin 9 — ACK (green) | direct |
| 28 | GND | PSX signal ground | — |
| 39 | VSYS | Regulator 5V output | — |
| 38 | GND | Power ground | — |

GPIO 19/20/21/22/26 are fixed in `psxSPI.pio`, not in `config.h` — that file is
the authority for the PlayStation side. GPIO 10/11 come from `MAPLE_PIN_A` in
`config.h` and can be moved to any free adjacent pair if a layout wants it.

Pins deliberately left alone: 0/1 (UART console), 5–8 (alternative host pins),
16/17 (PIO USB), 18 (web config mode).

---

## PlayStation side

Harvested DualShock cable, 9-wire. Pins numbered left to right looking at
the plug face. Traced pin by pin on the working adapter on 2026-09-12; this
table is the authority, not the colours. Also cross-checked earlier: the dead
pin lands at position 8, and both supply rails measured correct against the
cable's own ground.

| PSX pin | Colour | Signal | Connect to |
|---|---|---|---|
| 1 | Brown | DATA | Pico pin 25 (GPIO19) |
| 2 | Orange | CMD | Pico pin 26 (GPIO20) |
| 3 | Purple | 7.6V | Regulator input (see [Power](#power)) — trimmed, unused on the USB-powered build |
| 4 | Black | GND | Common ground |
| 5 | Red | 3.3V | **Not connected** — trimmed |
| 6 | Yellow | ATT / SEL | Pico pin 27 (GPIO21) |
| 7 | Blue | CLK | Pico pin 29 (GPIO22) |
| 8 | — | not populated | — |
| 9 | Green | ACK | Pico pin 31 (GPIO26) |

Purple substitutes for the usual grey on pin 3.

**Pin 5 (red)** is a live 3.3V output from the console. Leaving it bare is the
easiest way to short the console's logic rail, so trim it short and insulate it
properly rather than just taping it aside.

No pull-up resistors are needed on DATA or ACK. Both are open-drain
device→console lines and the console supplies the pull-ups.

---

## Dreamcast side

### The male plug (measured, trustworthy)

Right-hand view of the standard pinout diagram, **pin 1 at top-right**. The
numbering interleaves — top row 1, 3, 5; bottom row 2, 4 — it is not sequential
per row.

| DC pin | Colour | Signal | Measured |
|---|---|---|---|
| 1 | Red | Data A (SDCKA) | 3.3V idle |
| 2 | Blue | +5V | 5.0V |
| 3 | Black | GND | 0V |
| 4 | Green | Sense | 0V |
| 5 | White | Data B (SDCKB) | 3.3V idle |

### The socket — re-meter before soldering

The female socket on the extension cable will be **mirrored** relative to the
plug above, and **its wire colours will not match** — they vary by manufacturer.

There is a clean way to map it that needs no assumptions. The harvested male
plug and the extension's female socket mate with each other:

1. Plug the known male plug into the extension cable's female end.
2. Continuity-test from each known male wire to each wire at the extension's cut
   end.
3. That gives a direct, unambiguous mapping. Write it down before unplugging.

Then sanity-check it: with the male plug removed, the socket's 5V and GND should
read open, and once powered, pin 2 should read 5.0V against pin 3.

### The extension socket — measured 2026-09-11

Six wires. Mapped by plugging the cut-off male half into a live Dreamcast and
metering its bare wires, then mating the male half to the female half and
beeping each wire through to find which female wire carries which signal.

**The colours do not match end to end.** The extension is straight-through
electrically, but the manufacturer used a different colour for the same
conductor at each end. Every colour below is the colour **on the female half** —
the socket that gets wired into the adapter.

| Female wire | Same conductor as male wire | Read on live console | Signal | Connect to |
|---|---|---|---|---|
| **Green** | Orange | 5.0V | **+5V** | Regulator output / VSYS node — **never a GPIO** |
| White | Blue | 0V | GND | Common ground |
| Blue | Green | 0V | GND (the "sense" pin) | Common ground |
| Grey | Grey | — | Shield (contacts the plug shell) | Common ground |
| **Orange** | White | 3.3V | **Data A (SDCKA)** | 10Ω → Pico pin 14 (GPIO10) |
| **Red** | Red | 3.3V | **Data B (SDCKB)** | 10Ω → Pico pin 15 (GPIO11) |

**Corrected 2026-09-19 on the board build:** female orange is A (pin 14) and
female red is B (pin 15). The earlier table had them the other way round and
the stick was not detected until they were swapped.

**The dangerous wire is green.** On the male half it was orange; on the
earlier reference cable it was blue. Three cables, three colours for +5V. Never
carry a colour from one cable to another.

**Both white and blue read 0V on the console with nothing attached.** That is
because the Dreamcast port ties pins 3 and 4 together — the "sense" pin is a
second ground on the console side as well as inside the peripheral. Confirmed
directly: the two ground wires show continuity when the male half is plugged
into the console and none when it is unplugged, so the join is inside the
Dreamcast, not in the cable. There is nothing to detect and nothing to leave
floating: both go to ground, along with the shield. Which is nominally pin 3
and which is pin 4 is unknown and irrelevant.

**Settled on hardware, 2026-09-11.** The first guess (orange = A) got no
answer; swapping them brought the stick up immediately. Red is SDCKA, orange is
SDCKB. Both are 3.3V logic behind 10Ω, so the wrong guess cost nothing but a
replug.

---

## Power

The PlayStation port supplies 3.3V and 7.6V but **no 5V rail**, and the
Dreamcast stick needs 5V.

```
 PSX pin 3 ──┬── [ 10µF ]──┬── BA50DD0T ──┬── [ 47-100µF ]──┬── DC socket pin 2 (+5V)
 (7.6V,      │   [ 0.1µF]  │   1:IN       │   [ 0.1µF    ]  │
  purple)    │             │   2:GND      │                 └── Pico pin 39 (VSYS)
             │             │   3:OUT      │
 PSX pin 4 ──┴─────────────┴──────────────┴──────────────────── common ground
 (GND, black)

 PSX pin 5 (3.3V, red) ── not connected
```

### Regulator

**ROHM BA50DD0T**, Akizuki 130253. Package **TO220FP-3**, 2.54mm pitch:

| LDO pin | Function |
|---|---|
| 1 | Input (VIN) |
| 2 | Ground |
| 3 | Output (VOUT, 5V fixed) |

Input range 3–25V, 5V fixed output, up to 2A, **450mV dropout**. Dropping
7.6V → 5.0V leaves 2.6V of headroom, comfortably above dropout — this is why an
LDO works here where a 7805 (~2V dropout) would be marginal.

TO220FP is the fully-moulded package, so the tab is isolated. No insulating
washer needed, and it can touch the enclosure.

Expected draw is well under 150mA (Pico ~25–50mA, a Dreamcast stick is mostly
switches). At 2.6V of drop that is around 0.4W — no heatsink required, just
leave it in open air.

An LDO rather than a switcher on purpose: a switching regulator injects noise
right next to a 2 Mbps bus, and at this current efficiency is irrelevant.

### Capacitors

**Four capacitors in total**, two per side. Each side gets one bulk
electrolytic and one small ceramic, wired in parallel across the same pair of
regulator legs.

| Qty | Where | Value | Type | Placement |
|---|---|---|---|---|
| 1 | LDO in (leg 1 ↔ leg 2) | 10µF | electrolytic | close to the LDO |
| 1 | LDO in (leg 1 ↔ leg 2) | 0.1µF | ceramic | legs as short as possible |
| 1 | LDO out (leg 3 ↔ leg 2) | 100µF | electrolytic | close to the LDO |
| 1 | LDO out (leg 3 ↔ leg 2) | 0.1µF | ceramic | legs as short as possible |

ROHM specifies **22µF minimum** on the output. Anything from 47µF to 100µF is
comfortably above that and gives headroom for the stick's inrush at the far end
of a cable — one capacitor, not two. More than 100µF is harmless but buys
nothing here.

The two electrolytics are the ones that make the regulator stable, so the build
works without the ceramics; the ceramics are a refinement that can be added
later across the same legs without disturbing anything.

The small ceramics matter more than the bulk ones — they are what stops the
regulator oscillating, and they only work if they are physically at the pins.

### Possible upgrade: resettable fuse on the 7.6V input

Not fitted in the first build. A 72V 400mA resettable polyfuse (already owned,
originally bought for a Dreamcast controller port repair) would go **in series
with PSX pin 3**, between the console and the regulator input, upstream of the
input capacitors.

The point is to trip before the console's own PS601/PS606 fuses do. Ours resets
itself once power is removed and it cools; theirs need the console opened and
soldered. The author of PicoCtrl blew exactly those fuses before ever testing
his board.

400mA hold against ~150mA worst-case draw is comfortable margin with no
nuisance tripping, and roughly 1Ω of series resistance costs ~0.2V out of 2.6V
of dropout headroom, so it changes nothing electrically.

It guards against a sustained short — the realistic failure mode for a
flying-lead build. It does nothing for the GPIOs (that is what the 10Ω series
resistors are for) and, since it works by self-heating, nothing for fast
transients.

### Why VSYS and not the 3V3 pin

Feeding VSYS means the Pico's own regulator makes 3.3V, so there is no need to
disable it via 3V3_EN, and the console's logic rail is left alone. It also means
**USB can stay plugged in while the adapter is powered from the console** — the
Pico's VBUS→VSYS Schottky is reverse-biased when VSYS sits at 5.0V, so nothing
back-feeds the USB port. That is what makes serial debugging possible with the
adapter live in a console.

---

## Universal board layout

Board: blue double-sided, **14 columns (A–N) × 20 rows (1–20)** of plated holes,
four corner mounting holes outside the grid, and a strip of SMD-style pads along
each short edge that are not through-holes and are not used. Diagrams:
`board-top.png` (component side) and `board-bottom.png` (solder side, drawn
**mirrored** as you see it when the board is flipped), plus `-dark` variants.
Both are generated from `board-layout.py`, which is the authority if they ever
disagree.

The Pico is 20 pins long, which is the whole length of the board, so
everything else lives in the side margins. Its pin rows sit in **columns C and
J** (seven holes apart), USB end at row 1. Pin n on the left row is at C*n*;
pin n on the right row is at J(41−n), so pin 39 is J2, pin 38 is J3, pins
31–25 are J10–J16.

### Component side

| Where | What |
|---|---|
| B1–B4 | Serial white (TX), spare RX, serial black (GND), mode switch — wires, linked one hole to pins 1–4 |
| A10 | DC **red** (data A) lands; link to A11 |
| A11–A14 | 10Ω, bottom leg linked A14→C14 (pin 14, data A) |
| A15–A18 | 10Ω, top leg linked A15→C15 (pin 15, data B) |
| A19 | DC **orange** (data B) lands; link to A18 |
| L1 / L3 | 10µF electrolytic, **striped leg at L1**, + at L3 |
| N1 / N3 | 100µF electrolytic, **striped leg at N1**, + at N3 |
| L4 | PSX purple (7.6V) lands, on the IN link |
| M4 | DC green (+5V) lands; link to N4 on the OUT link |
| L6 M6 N6 | Regulator legs 1 IN, 2 GND, 3 OUT. **Marked face toward row 20.** Body spans K–N and overhangs the right edge by about half a millimetre |
| L7 / L8 | 0.1µF ceramic across IN and ground |
| N7 / N8 | 0.1µF ceramic across OUT and ground |
| L10–L16 | 7-pin male header for the PlayStation cable: green ACK, reset button, blue CLK, black GND, yellow ATT, orange CMD, brown DATA |
| N17 N18 N19 | DC white, blue, grey (all ground) land on the edge spur |

Column A sits just outside the Pico's edge and column B just under it; keep the
two resistors lying flat and the B1–B4 wires thin. If the 100µF is the 8mm
kind it overhangs the board edge by about 2mm, which is fine; a 47µF (6.3mm)
sits inside the edge if that is preferred.

### Solder side, bare wire unless stated

| Link | Purpose |
|---|---|
| K1–N1, K1–K9, K9–N9, N9–N20 | Ground: a bus along row 1, a spur down column K, a bus along row 9, a spur down the right edge. One net |
| J3–K3 | Pico pin 38 to ground |
| L3–L6 | IN: 10µF +, purple landing at L4, regulator leg 1 |
| N3–N6 | OUT: 100µF +, DC green via N4, VSYS at N5, regulator leg 3 |
| L7–L6, N7–N6 | Ceramics to IN and OUT |
| L8–L9, N8–N9 | Ceramics to ground |
| M6–M9 | Regulator leg 2 to ground |
| M4–N4 | DC green to OUT |
| **J2 → N2 → N5, insulated** | Pico pin 39 (VSYS) to OUT. The only insulated wire; it crosses the K spur at K2 |
| L10–J10 … L16–J16 | PSX header to pins, seven two-hole links through column K |
| B1–C1 … B4–C4 | Serial and switch to pins 1–4 |
| A10–A11, A14–C14, A15–C15, A18–A19 | Dreamcast data through the resistors |

Places where a ground pad sits one hole from a live one — keep the solder tidy:
**K3/L3**, **L7/L8**, **N7/N8**. The whole of rows 2–4 in columns K–N is dense.

The K spur ends at K9 on purpose: K10–K16 are pass-through pads for the PSX
links and must not be grounded.

---

## Resistors

| Qty | Value | Where | Why |
|---|---|---|---|
| 2 | 10Ω | In series, Pico GPIO10/11 → DC socket pins 1/5 | Current limit on the Maple lines |

Place both **at the Pico end**, not at the socket.

The Dreamcast's own hardware uses 36Ω here; DreamPicoPort's author dropped to 10Ω
because the Pico's output drivers already have meaningful series resistance.
**33Ω is the fallback** if the bus turns out to be flaky.

These resistors are the only thing standing between a firmware bug and a dead
Pico: if the code ever drives a line as an output while the stick is driving it
the other way, 10Ω is what limits the resulting current. Do not omit them, even
though the reference guide says they can be.

Nothing else needs a resistor. The 1kΩ pull-up mentioned in earlier planning is
for the *host*-mode DualShock test rig, not for this build.

---

## Grounding

Bring all grounds to one point: PSX pin 4 (black), DC socket pin 3 and its
shield, the regulator's pin 2, the capacitor return legs, and the Pico. Use the
Pico's pin 38 for the power ground and pins 13 / 28 for the signal returns so
that the regulator's return current is not sharing a path with the bus signals.

---

## Before applying power

1. **Build and verify the power section on its own first.** With the Pico *not*
   connected, plug into the console, power on, and meter the regulator output.
   It must read 5.0V. Only then wire VSYS.
2. Meter continuity from every GPIO you have soldered to DC pin 2 and to the
   7.6V line. Both must read open. This is the check that saves the board.
3. Confirm the 3.3V red PSX wire is trimmed and insulated.
4. Console off before connecting or disconnecting either connector. Never
   hot-plug.
5. Change one thing at a time.

---

## Not yet confirmed

- **Whether external pull-ups are needed on the Maple data lines.** *Resolved
  2026-09-11: they are not. The stick came up on the internal pull-ups alone
  and played Melee without a hiccup. The 1kΩ stay in the drawer.*

  Original note: The protocol
  block diagram in DreamPicoPort shows the host providing pull-ups to +3.3V, but
  its Host Mode Installation Guide would not load through any route available
  here, so the Simple Configuration's exact component list is unverified. The
  firmware enables the RP2040's internal pull-ups (~50–80kΩ), which may be weak
  for 2 Mbps over a controller cable. **If the bus is unreliable, adding 1kΩ from
  each data line to 3V3 (Pico pin 36) is the first thing to try** — 1kΩ is
  already in the parts order. Do not use 10kΩ; it is far too slow for this bus.

  Measurement, 2026-09-08, Maple host mode running with nothing attached:

  | Point | Reading |
  |---|---|
  | 3V3 rail (pin 36) | 3.27V |
  | GPIO10 / GPIO11 (pins 14/15) | 3.185V |

  A repeatable 85mV, 2.6% below the rail, measured against the rail itself
  rather than a nominal figure. Two benign explanations, indistinguishable with
  a multimeter: the meter loading the weak internal pull-up (~50–80kΩ), or the
  lines being actively driven low for ~2.6% of each cycle by the 16 ms detect
  poll, which a meter averages. The second would be welcome — it would mean the
  PIO is genuinely transmitting.

  To separate them without a scope: raise `MAPLE_DETECT_INTERVAL_US` tenfold and
  re-measure. If the sag drops with it, it is duty cycle, not pull-up strength.
- **The socket pinout and colours.** Nothing on the Dreamcast side should be
  soldered until the continuity mapping above is done.
- The 7.6V rail is fused inside the console (PS601/PS606 on a PS1). A short
  there blows a console fuse, not just a session.
