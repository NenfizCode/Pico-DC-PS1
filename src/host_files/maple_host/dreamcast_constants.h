/* Dreamcast Maple Bus commands. Ported from DreamPicoPort. */

#pragma once

#include <cstdint>

#define DEVICE_FN_CONTROLLER 0x00000001

enum DreamcastCommand : uint8_t {
    COMMAND_DEVICE_INFO_REQUEST = 0x01,
    COMMAND_EXT_DEVICE_INFO_REQUEST = 0x02,
    COMMAND_RESET = 0x03,
    COMMAND_SHUTDOWN = 0x04,
    COMMAND_RESPONSE_DEVICE_INFO = 0x05,
    COMMAND_RESPONSE_EXT_DEVICE_INFO = 0x06,
    COMMAND_RESPONSE_ACK = 0x07,
    COMMAND_RESPONSE_DATA_XFER = 0x08,
    COMMAND_GET_CONDITION = 0x09,
    COMMAND_GET_MEMORY_INFORMATION = 0x0A,
    COMMAND_BLOCK_READ = 0x0B,
    COMMAND_BLOCK_WRITE = 0x0C,
    COMMAND_GET_LAST_ERROR = 0x0D,
    COMMAND_SET_CONDITION = 0x0E,
    COMMAND_RESPONSE_AR_ERROR = 0xF9,
    COMMAND_RESPONSE_LCD_ERROR = 0xFA,
    COMMAND_RESPONSE_FILE_ERROR = 0xFB,
    COMMAND_RESPONSE_REQUEST_RESEND = 0xFC,
    COMMAND_RESPONSE_UNKNOWN_COMMAND = 0xFD,
    COMMAND_RESPONSE_FUNCTION_CODE_NOT_SUPPORTED = 0xFE,
    COMMAND_INVALID = 0xFF
};

/* Maple addressing for port A. The upper two bits select the port
 * (A=0, B=1, C=2, D=3); bit 5 marks the main peripheral.
 *
 * Note: OGX-Mini-2026 defines these two the other way round, which would
 * address the request to nothing. DreamPicoPort builds a recipient address as
 * (playerIndex << 6) | MAIN_PERIPHERAL_ADDR_MASK with MAIN_PERIPHERAL_ADDR_MASK
 * = 0x20, and sends with the sender address set to the port bits alone. */
#define MAPLE_HOST_ADDR  0x00
#define MAPLE_CTRL_ADDR  0x20
