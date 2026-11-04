/* Maple Bus timing configuration.
 * Adapted from DreamPicoPort (OrangeFox86) via OGX-Mini-2026. MIT licensed. */

#pragma once

/* The minimum amount of time the line is checked to be open before taking it. */
#ifndef MAPLE_OPEN_LINE_CHECK_TIME_US
#define MAPLE_OPEN_LINE_CHECK_TIME_US 10
#endif

/* Nanoseconds per transmitted bit. 480 ns is just over 2 Mbps, as the
 * Dreamcast itself uses. Must be divisible by 3. */
#ifndef MAPLE_NS_PER_BIT
#define MAPLE_NS_PER_BIT 480
#endif

/* Percentage added on top of the expected write duration before timing out. */
#ifndef MAPLE_WRITE_TIMEOUT_EXTRA_PERCENT
#define MAPLE_WRITE_TIMEOUT_EXTRA_PERCENT 20
#endif

/* Maximum time to wait for the beginning of a response when one is expected.
 * A real peripheral starts responding somewhere after 50 us. */
#ifndef MAPLE_RESPONSE_TIMEOUT_US
#define MAPLE_RESPONSE_TIMEOUT_US 1000
#endif

/* Dreamcast controllers sometimes leave a ~180 us gap between words. */
#ifndef MAPLE_INTER_WORD_READ_TIMEOUT_US
#define MAPLE_INTER_WORD_READ_TIMEOUT_US 300
#endif

/* PicoGamepadConverter calls set_sys_clock_khz(240000, true) in main().
 * This value sets the maple_out clock divider, so it must match or the bus
 * will transmit at the wrong bit rate. */
#ifndef MAPLE_CPU_FREQ_KHZ
#define MAPLE_CPU_FREQ_KHZ 240000
#endif

/* Both PIO programs are 29 instructions and a PIO block holds only 32, so
 * they cannot be resident at the same time. They are swapped in and out of a
 * single block at offset 0. See MapleBus::loadProgram(). */
#define MAPLE_PROGRAM_OFFSET 0
