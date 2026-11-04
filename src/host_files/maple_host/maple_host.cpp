/* Dreamcast (Maple Bus) host mode for PicoGamepadConverter.
 *
 * The packet layer follows DreamPicoPort, which is the proven implementation:
 * a peripheral is detected by repeating Device Info Request until it answers,
 * and only then is it polled with Get Condition, which carries a one-word
 * payload naming the function being queried. OGX-Mini-2026 skips detection,
 * sends Get Condition with no payload, and has the host and controller
 * addresses swapped, so its host path is not usable as-is.
 */

#include "maple_host.h"
#include "MapleBus.h"
#include "MaplePacket.hpp"
#include "dreamcast_constants.h"
#include "dreamcast_structures.h"
#include "maple_config.h"
#include <pico/stdlib.h>
#include <cstdio>
#include <cstring>
#include <new>

/* Poll cadence once a peripheral is answering. The device side is served from
 * the last completed reading, so this only bounds how stale that reading can
 * be; 1 ms is far below one console frame. */
#define MAPLE_POLL_INTERVAL_US 1000

/* Slower cadence while nothing is attached, so an empty port does not sit at
 * full rate forever. */
#define MAPLE_DETECT_INTERVAL_US 16000

/* Consecutive failed transactions before the peripheral is treated as gone. */
#define MAPLE_FAIL_LIMIT 10

/* Serial diagnostics. One status line per second, plus one-off lines for
 * detection and loss. Kept off the per-transaction path so the UART never
 * stalls the poll loop. Set to 0 to silence. */
#ifndef MAPLE_DEBUG
#define MAPLE_DEBUG 1
#endif
#define MAPLE_STATUS_INTERVAL_US 1000000

/* Storage for the bus so construction happens under our control rather than
 * during static init. */
static uint8_t s_busStorage[sizeof(MapleBus)] __attribute__((aligned(8)));
static MapleBus* s_bus = nullptr;

static uint8_t* s_condition = nullptr;
static uint64_t s_nextPollUs = 0;
static uint32_t s_failCount = 0;
static bool s_detected = false;
static bool s_connected = false;

static uint32_t s_okTotal = 0;
#if MAPLE_DEBUG
static uint64_t s_nextStatusUs = 0;
static uint32_t s_failTotal = 0;
static uint32_t s_busyTotal = 0;          /* write() refused: line not idle */
static uint32_t s_badCmdTotal = 0;        /* replied, but not what we asked for */
static uint8_t  s_lastCmd = 0;
static uint32_t s_lastFrame = 0;
static uint32_t s_lastFn = 0;
static MapleBus::FailureReason s_lastFail = MapleBus::FailureReason::NONE;

static const char* fail_name(MapleBus::FailureReason r) {
    switch (r) {
        case MapleBus::FailureReason::NONE:            return "none";
        case MapleBus::FailureReason::CRC_INVALID:     return "crc";
        case MapleBus::FailureReason::MISSING_DATA:    return "missing";
        case MapleBus::FailureReason::BUFFER_OVERFLOW: return "overflow";
        case MapleBus::FailureReason::TIMEOUT:         return "timeout";
    }
    return "?";
}

static void debug_status(uint64_t now) {
    if (now < s_nextStatusUs) return;
    s_nextStatusUs = now + MAPLE_STATUS_INTERVAL_US;
    printf("MAPLE %s%s ok=%lu fail=%lu busy=%lu badcmd=%lu lastfail=%s lastcmd=%02x",
           s_detected ? "detected" : "searching",
           s_connected ? "+connected" : "",
           (unsigned long)s_okTotal, (unsigned long)s_failTotal,
           (unsigned long)s_busyTotal, (unsigned long)s_badCmdTotal,
           fail_name(s_lastFail), s_lastCmd);
    if (s_condition) {
        printf(" cond=");
        for (int i = 0; i < MAPLE_CONDITION_SIZE; i++) printf("%02x", s_condition[i]);
    }
    printf("\n");
}
#endif

static void write_neutral(uint8_t* dst) {
    controller_condition_t c;
    /* Buttons are active low, so every bit set means nothing pressed. */
    memset(&c, 0xFF, sizeof(c));
    c.l = 0;            /* analog triggers released */
    c.r = 0;
    c.lAnalogUD = 128;  /* sticks centred */
    c.lAnalogLR = 128;
    c.rAnalogUD = 128;
    c.rAnalogLR = 128;
    memcpy(dst, &c, sizeof(c));
}

static void drop_peripheral(void) {
#if MAPLE_DEBUG
    if (s_detected)
        printf("MAPLE lost peripheral (lastfail=%s)\n", fail_name(s_lastFail));
#endif
    s_detected = false;
    s_connected = false;
    if (s_condition) write_neutral(s_condition);
}

static void count_failure(void) {
    if (++s_failCount >= MAPLE_FAIL_LIMIT) {
        s_failCount = 0;
        drop_peripheral();
    }
}

void maple_host_init(uint8_t pio, uint8_t pin_a, uint8_t *condition) {
    s_condition = condition;
    if (s_condition) write_neutral(s_condition);

    s_nextPollUs = 0;
    s_failCount = 0;
    s_detected = false;
    s_connected = false;

    /* dirPin = -1 selects the Simple Configuration from DreamPicoPort's host
     * mode guide: the GPIOs sit directly on the bus behind series resistors,
     * with no external direction-select buffer. */
    s_bus = new (s_busStorage) MapleBus(pio, pin_a, -1, true);

#if MAPLE_DEBUG
    printf("MAPLE host init: pio%u pins %u/%u\n", pio, pin_a, pin_a + 1);
#endif
}

bool maple_host_connected(void) {
    return s_connected;
}

uint32_t maple_host_ok_count(void) {
    return s_okTotal;
}

void maple_host_update_state(void) {
    if (!s_bus) return;

    const uint64_t now = time_us_64();

#if MAPLE_DEBUG
    debug_status(now);
#endif

    if (s_bus->isBusy()) {
        MapleBus::Status st = s_bus->processEvents(now);

        if (st.phase == MapleBus::Phase::READ_COMPLETE) {
            bool ok = false;
            if (st.readBuffer && st.readBufferLen >= 2) {
                const uint8_t cmd = MaplePacket::Frame::getFrameCommand(
                    st.readBuffer[0], st.rxByteOrder);
#if MAPLE_DEBUG
                s_lastCmd = cmd;
                s_lastFrame = st.readBuffer[0];
                s_lastFn = st.readBuffer[1];
#endif
                if (!s_detected) {
                    /* Any well-formed device info reply means something is
                     * there. Whether it is a controller is settled by the
                     * Get Condition that follows. */
                    if (cmd == COMMAND_RESPONSE_DEVICE_INFO) {
                        s_detected = true;
                        ok = true;
#if MAPLE_DEBUG
                        printf("MAPLE detected: frame=%08lx fn=%08lx words=%lu\n",
                               (unsigned long)st.readBuffer[0],
                               (unsigned long)st.readBuffer[1],
                               (unsigned long)st.readBufferLen);
#endif
                    }
                } else if (cmd == COMMAND_RESPONSE_DATA_XFER && st.readBufferLen >= 4) {
                    /* frame word, function code, then two words of condition. */
                    if (s_condition)
                        memcpy(s_condition, &st.readBuffer[2], MAPLE_CONDITION_SIZE);
#if MAPLE_DEBUG
                    if (!s_connected)
                        printf("MAPLE first condition: fn=%08lx\n",
                               (unsigned long)st.readBuffer[1]);
#endif
                    s_connected = true;
                    ok = true;
                }
#if MAPLE_DEBUG
                if (!ok) s_badCmdTotal++;
#endif
            }
            if (ok) {
                s_failCount = 0;
                s_okTotal++;
            } else {
                count_failure();
            }
        } else if (st.phase == MapleBus::Phase::READ_FAILED ||
                   st.phase == MapleBus::Phase::WRITE_FAILED) {
#if MAPLE_DEBUG
            s_failTotal++;
            s_lastFail = st.failureReason;
#endif
            count_failure();
        }
        return;
    }

    if (now < s_nextPollUs) return;
    s_nextPollUs = now + (s_detected ? MAPLE_POLL_INTERVAL_US : MAPLE_DETECT_INTERVAL_US);

    MaplePacket req;
    req.payloadByteOrder = MaplePacket::ByteOrder::HOST;
    if (s_detected) {
        req.setFrame(COMMAND_GET_CONDITION, MAPLE_CTRL_ADDR, MAPLE_HOST_ADDR, 1);
        req.payload[0] = DEVICE_FN_CONTROLLER;
    } else {
        req.setFrame(COMMAND_DEVICE_INFO_REQUEST, MAPLE_CTRL_ADDR, MAPLE_HOST_ADDR, 0);
    }

    if (!s_bus->write(req, true, MAPLE_RESPONSE_TIMEOUT_US, MaplePacket::ByteOrder::HOST)) {
        /* Line was not open, or a transaction is still settling. Try again on
         * the next tick rather than spinning here. */
#if MAPLE_DEBUG
        s_busyTotal++;
#endif
        count_failure();
    }
}
