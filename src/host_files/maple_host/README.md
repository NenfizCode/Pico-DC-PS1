# Dreamcast (Maple bus) host

Reads a Dreamcast controller and feeds it into PicoGamepadConverter's shared
gamepad report, so any output mode can use it. In this project that output is
mainly PS1/PS2.

## Source and license

The Maple bus code comes from [DreamPicoPort](https://github.com/OrangeFox86/DreamPicoPort)
by OrangeFox86, through the host-only copy in
[OGX-Mini-2026](https://github.com/MegaCadeDev/OGX-Mini-2026). Both are MIT
licensed, and DreamPicoPort's notice is in [`LICENSE`](./LICENSE) in this
folder. The rest of the project is GPL-3.0.

`maple_in.pio`, `maple_out.pio`, `MaplePacket.hpp`, `dreamcast_constants.h`,
`dreamcast_structures.h` and `maple_utils.h` are mostly unchanged from
upstream.

Two details follow DreamPicoPort rather than OGX-Mini:

- **Get Condition payload.** OGX-Mini sends `COMMAND_GET_CONDITION` with no
  payload. The command needs one payload word naming the function being
  queried (`DEVICE_FN_CONTROLLER`).
- **Addresses.** OGX-Mini has `MAPLE_HOST_ADDR` and `MAPLE_CTRL_ADDR` the wrong
  way round. The recipient address is `(port << 6) | 0x20`, where `0x20` marks
  the main peripheral, and the sender address is the port bits alone. For
  port A that's recipient `0x20`, sender `0x00`.

## One PIO block instead of two

DreamPicoPort puts `maple_out` and `maple_in` on separate PIO blocks, because
each program is 29 instructions and a block holds 32. The RP2040 only has two
blocks, and the PS1/PS2 device side needs one of them (`cmd_reader` 5 +
`dat_writer` 9 = 14 instructions). 29 + 29 + 14 = 72 doesn't fit in 64.

So both Maple programs share one block and are swapped at the write-to-read
handoff:

| Block | Contents | Instruction slots |
|---|---|---|
| `pio0` | PS1/PS2 device, 2 state machines | 14 / 32, always loaded |
| `pio1` | `maple_out` or `maple_in`, 1 state machine | 29 / 32, swapped |

`what_pio_use()` in `utils.c` returns 0 for `MAPLE`, which gives the device
side `pio0`.

### Timing

Both programs are relocatable (`.origin = -1`) and load at offset 0. A swap is
a 29-word write to `pio->instr_mem[]` plus a `pio_sm_init()` with a config
built once at startup.

According to DreamPicoPort, a peripheral starts replying at least 50 µs after
the bus goes neutral, and the existing handoff takes about 1 µs. 29 word writes
at 240 MHz add well under another microsecond.

(`MAPLE_RESPONSE_DELAY_NS` in DreamPicoPort's `configuration.h` is 50 ns. It's
a scheduling estimate, not the reply delay.)

Everything on the handoff path runs from RAM: `writeIsr`, `readIsr`,
`loadProgram` and both interrupt handlers. The instruction words are copied
into RAM arrays at startup for the same reason. To check:

```bash
arm-none-eabi-nm -C build/PicoGamepadConverter.elf | grep -i maple
```

Addresses starting with `0x2` are in RAM; `0x1` means flash.

## Interrupts

Both state machines are on the same block, so they raise interrupt flags on
the same block. `irq ... rel` makes each flag index match its state machine
index, so they get separate flags, routed to `PIOx_IRQ_0` (write done) and
`PIOx_IRQ_1` (read). OGX-Mini always clears flag 1, which is only correct if
the state machine happens to be index 1. This version clears the flag for the
actual index.

## Protocol

The same sequence a Dreamcast uses:

1. Send `COMMAND_DEVICE_INFO_REQUEST` every 16 ms until the controller answers
   with `COMMAND_RESPONSE_DEVICE_INFO`.
2. Send `COMMAND_GET_CONDITION` every 1 ms, with one payload word
   (`DEVICE_FN_CONTROLLER`). The reply is the function code followed by eight
   bytes of controller state.
3. After ten failures in a row, drop the controller, reset the report to
   neutral, and go back to step 1.

## Pins

`MAPLE_PIN_A` in `src/config.h` sets Data A (default GPIO10). Data B is always
the next pin (GPIO11). The default avoids GPIO 0/1 (UART), 5–8, 16/17, 18, and
19–22 and 26 (PS1/PS2 device).

The lines connect directly to the GPIOs through 10 Ω series resistors, with no
direction buffer (DreamPicoPort's "simple configuration"). That's why
`MapleBus` is constructed with `dirPin = -1`. The RP2040's internal pull-ups
are enough; no external pull-ups are needed.

## PS1 timing changes

These are in the PS1/PS2 device code, not here, but they were needed to use a
Dreamcast controller on a PS1:

- **ACK delay.** `dat_writer` in `psxSPI.pio` waits about 6.4 µs before
  asserting ACK. The PS1 BIOS pad driver ignores an ACK in the first 2–3 µs
  after the last clock pulse
  ([psx-spx](https://psx-spx.consoledev.net/)), so
  with the original timing the BIOS menu and some games ignored the
  controller.
- **Clock divider.** The PSX device PIO divider was changed from 50 to 96 to
  get back to the intended 2.5 MHz at the 240 MHz system clock.
