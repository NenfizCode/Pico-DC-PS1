/* Dreamcast (Maple Bus) host mode for PicoGamepadConverter.
 *
 * Mirrors the shape of the N64 host mode: init once, then call the update
 * function repeatedly from the host loop. It never blocks, so a PS1/PS2
 * transaction on the other core is never held up waiting for the stick.
 */

#ifndef MAPLE_HOST_H
#define MAPLE_HOST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Size of the buffer maple_host_init() fills: one controller_condition_t. */
#define MAPLE_CONDITION_SIZE 8

/** Set up the Maple bus.
 *  @param pio        PIO block index (0 or 1) given over to the Maple bus.
 *                    Both PIO programs live here, one at a time.
 *  @param pin_a      GPIO for data line A. Line B must be pin_a + 1.
 *  @param condition  Buffer of MAPLE_CONDITION_SIZE bytes that receives the
 *                    latest controller state. Seeded with a neutral reading so
 *                    it is safe to convert before the stick has ever replied. */
void maple_host_init(uint8_t pio, uint8_t pin_a, uint8_t *condition);

/** Advance the bus. Polls the stick on a fixed cadence and services the
 *  in-flight transaction. Returns immediately if there is nothing to do. */
void maple_host_update_state(void);

/** True once a well-formed response has been seen and none has been missed
 *  since. Goes false after several consecutive failures. */
bool maple_host_connected(void);

/** Number of successful Get Condition reads since boot. Diagnostic. */
uint32_t maple_host_ok_count(void);

#ifdef __cplusplus
}
#endif

#endif /* MAPLE_HOST_H */
