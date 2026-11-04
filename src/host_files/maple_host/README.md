# Dreamcast (Maple Bus) host mode

Reads a real Dreamcast controller or arcade stick and feeds it into
PicoGamepadConverter's shared gamepad report, so any device mode — PS1/PS2 in
particular — can present it to a console.

## Origin and licensing

The Maple Bus layer comes from [DreamPicoPort](https://github.com/OrangeFox86/DreamPicoPort)
by OrangeFox86, via the trimmed host-only copy in
[OGX-Mini-2026](https://github.com/MegaCadeDev/OGX-Mini-2026). Both are MIT
licensed, as is this project. `maple_in.pio`, `maple_out.pio`, `MaplePacket.hpp`,
`dreamcast_constants.h`, `dreamcast_structures.h` and `maple_utils.h` are taken
essentially unchanged.

Two things were **not** taken from OGX-Mini, because its host path does not look
exercised:

- It sends `COMMAND_GET_CONDITION` with an empty payload. Get Condition needs a
  one-word payload naming the function being queried (`DEVICE_FN_CONTROLLER`).
- It defines `MAPLE_HOST_ADDR` as `0x20` and `MAPLE_CTRL_ADDR` as `0x00`, which
  is the wrong way round. DreamPicoPort builds a recipient address as
  `(playerIndex << 6) | 0x20`, where `0x20` marks the main peripheral, and sends
  with the sender address set to the port bits alone. For port A that is
  recipient `0x20`, sender `0x00`.

Both follow DreamPicoPort here instead.

## Why this differs: one PIO block, not two

DreamPicoPort puts `maple_out` on one PIO block and `maple_in` on the other, and
says why in its README:

> Using 2 separate PIO blocks for reading and writing is necessary because each
> PIO block can only hold up to 32 instructions, and this interface is too
> complex to fit both read and write into a single block.

Both programs are exactly 29 instructions. On an RP2040 there are only two
blocks totalling 64 instruction slots, and PicoGamepadConverter needs one of
them for the PS1/PS2 device side (`cmd_reader` 5 + `dat_writer` 9 = 14). Wanting
29 + 29 + 14 = 72 slots does not fit.

So this version keeps **both Maple programs in one block and swaps them during
the write-to-read handoff**:

| Block | Contents | Slots |
|---|---|---|
| `pio0` | PS1/PS2 device, 2 state machines | 14 / 32, resident |
| `pio1` | `maple_out` **or** `maple_in`, 1 state machine each | 29 / 32, swapped |

`what_pio_use()` in `utils.c` returns 0 for `MAPLE`, which is what hands the
device side `pio0`.

### Why the swap fits

Both programs are relocatable (`.origin = -1`), so both load at offset 0 and the
swap is a fixed 29-word overwrite of `pio->instr_mem[]` plus a `pio_sm_init()`
with a config built once at construction. DreamPicoPort measured the budget:

> Testing showed the handoff always occurs within 1 microsecond after bringing
> the bus back to neutral. A device on the Maple Bus starts responding some time
> after 50 microseconds from the point of the bus going neutral after an end
> sequence.

So there is roughly 50 µs available and the existing handoff spends about 1 µs
of it. 29 word stores at 240 MHz is well under a microsecond on top of that.

Note that `MAPLE_RESPONSE_DELAY_NS` in DreamPicoPort's `configuration.h` is 50
**nanoseconds** — that is a scheduling estimate, not the turnaround, and it is
easy to misread as the real figure.

To keep that budget honest, everything on the handoff path is in RAM:
`writeIsr`, `readIsr`, `loadProgram`, and the two interrupt handlers themselves
(the vector table points straight at those, so leaving them in flash would put
an XIP fetch at the front of the path). The instruction words are staged into
RAM arrays at construction for the same reason. Verify with:

```bash
arm-none-eabi-nm -C build/PicoGamepadConverter.elf | grep -i maple
```

Addresses starting `0x2` are RAM; `0x1` is execute-in-place flash.

## Interrupts

Both state machines now share a block, so they raise PIO interrupt flags on the
same block. `irq ... rel` makes the flag index equal to the state machine index,
so the two get distinct flags, routed to `PIOx_IRQ_0` (write complete) and
`PIOx_IRQ_1` (read). OGX-Mini hardcodes `pio_interrupt_clear(pio, 1u)`, which is
only correct if the claimed state machine happens to be index 1; this version
clears the flag matching the actual index.

## Protocol flow

Mirrors what a real console does, which is what DreamPicoPort implements:

1. Repeat `COMMAND_DEVICE_INFO_REQUEST` (no payload) every 16 ms until the
   peripheral answers with `COMMAND_RESPONSE_DEVICE_INFO`.
2. Then poll `COMMAND_GET_CONDITION` every 1 ms, carrying one payload word
   naming the function (`DEVICE_FN_CONTROLLER`). The reply is three payload
   words: the function code, then eight bytes of condition.
3. After ten consecutive failures the peripheral is dropped, the buffer is
   reset to neutral, and detection starts again.

Skipping step 1 and polling Get Condition cold may well work, but a console
does not do that, and there is no way to check against hardware yet.

## Wiring

`MAPLE_PIN_A` in `config.h` is GPIO 10, with line B on GPIO 11. That avoids
GPIO 5–8 (alternative host pins), 16/17 (PIO USB), 18 (web config), 19–22 and 26
(PS1/PS2 device) and 0/1 (UART console).

The bus is driven in DreamPicoPort's *Simple Configuration* — GPIOs sitting
directly on the Maple lines behind series resistors, with no direction-select
buffer — which is why `MapleBus` is constructed with `dirPin = -1`.

## Status

**Working end to end as of 2026-09-11.** A Dreamcast arcade stick, read over
the single-block PIO swap, played a game on a fat PS1 with Device set to
PS1/PS2, USB-powered. Earlier the same evening it played Melee on PC with
Device set to Xinput. The RP2040's internal pull-ups were sufficient on the
Maple lines; no external pull-ups were fitted.

Two things to know:

- The **startup menu and Soul Edge ignored the stick** at first while KOF 96
  worked. The cause was ACK timing on the PS1 side, not Maple: the device PIO
  asserted ACK about 1 us after the last clock edge, and the kernel pad
  driver ignores ACKs in the first 2-3 us after the last SCK pulse (psx-spx),
  so it clocked the poll in full but discarded it. Games that ship their own
  pad driver did not care. `dat_writer` in `psxSPI.pio` now holds ~6.4 us
  before ACK, which fixed both. Analog mode was unrelated: those titles
  refuse a `0x73` pad by design, exactly as they refuse a DualShock with its
  light on.
- The PSX device's PIO clock divider was changed from 50 to 96 to restore the
  designed 2.5 MHz at this project's 240 MHz system clock (ACK 2.4 us instead
  of 1.25 us).

Not yet done: the console-powered configuration through the regulator, and
assembly onto the universal board.
