//
// Created by loc15 on 5/02/26.
//

#include "tusb.h"

#include "process_keyboard.h"

#define KEYBOARD_MASK_UP    0
#define KEYBOARD_MASK_DOWN  1
#define KEYBOARD_MASK_LEFT  2
#define KEYBOARD_MASK_RIGHT 3
#define KEYBOARD_MASK_START 4
#define KEYBOARD_MASK_BACK  5
#define KEYBOARD_MASK_LEFT_THUMB     6
#define KEYBOARD_MASK_RIGHT_THUMB    7
#define KEYBOARD_MASK_LEFT_SHOULDER  8
#define KEYBOARD_MASK_RIGHT_SHOULDER 9
#define KEYBOARD_MASK_GUIDE 10
#define KEYBOARD_MASK_A     12
#define KEYBOARD_MASK_B     13
#define KEYBOARD_MASK_X     14
#define KEYBOARD_MASK_Y     15
#define KEYBOARD_MASK_LX_L  16
#define KEYBOARD_MASK_LX_R  17
#define KEYBOARD_MASK_LX_U  18
#define KEYBOARD_MASK_LX_D  19
#define KEYBOARD_MASK_RX_L  20
#define KEYBOARD_MASK_RX_R  21
#define KEYBOARD_MASK_RX_U  22
#define KEYBOARD_MASK_RX_D  23
#define KEYBOARD_MASK_LEFT_TRIGGER   24
#define KEYBOARD_MASK_RIGHT_TRIGGER  25



// Lower-Case ASCII codes by keyboard-code index, 16 elements per row
/*static const uint8_t lower[] = {
    31,  31,   31,   31,   31,   31,   31,   31,  31,  31,   31,   31,   31,   31, 31, 31,
    31,  31,   31,   31,   31,  31, 31, 31,  31,  31, 31, KEYBOARD_MASK_DOWN, KEYBOARD_MASK_LEFT, KEYBOARD_MASK_UP, 31, 31,
    31,  31, 31, KEYBOARD_MASK_RIGHT, 31, 31, 31, 31,  31,  31, 31, 31, KEYBOARD_MASK_LEFT_TRIGGER, 31, 31, 31,
    31,  31, 31, KEYBOARD_MASK_X, 31, KEYBOARD_MASK_LEFT_SHOULDER, 31, 31,  31,  31, 31, KEYBOARD_MASK_A, KEYBOARD_MASK_Y, 31, 31, 31,
    31,  31, KEYBOARD_MASK_B, KEYBOARD_MASK_RIGHT_SHOULDER, KEYBOARD_MASK_RIGHT_TRIGGER, 31, 31, 31,  31, 31, 31, 31, 31, KEYBOARD_MASK_GUIDE, 31, 31,
    31,  31,  31, 31, 31, 31, 31,   31,  31,  31,   KEYBOARD_MASK_START, 31, 31, 31 ,31,   31,
    31,  31,   31,   31,   31,   31,   KEYBOARD_MASK_BACK,  31,  31,  31,   31,   KEYBOARD_MASK_LX_L,   31,   31,   31,   31,
    31,  31,   KEYBOARD_MASK_LX_D,   31,   KEYBOARD_MASK_LX_R,   KEYBOARD_MASK_LX_U,   31, 31,  31,  31,   31,   31,   31,   31,   31,   31
};*/

static const uint8_t keycode2xinput[][2] = {
    {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {KEYBOARD_MASK_LEFT, KEYBOARD_MASK_LX_L}, {31, 31}, {31, 31}, {KEYBOARD_MASK_RIGHT, KEYBOARD_MASK_LX_R},
    {KEYBOARD_MASK_RIGHT_SHOULDER, KEYBOARD_MASK_RIGHT_SHOULDER}, {31, 31}, {31, 31}, {KEYBOARD_MASK_X, KEYBOARD_MASK_X},
    {31, 31}, {KEYBOARD_MASK_A, KEYBOARD_MASK_A}, {KEYBOARD_MASK_B, KEYBOARD_MASK_B}, {31, 31},
    {KEYBOARD_MASK_RIGHT_THUMB, KEYBOARD_MASK_RIGHT_THUMB}, {KEYBOARD_MASK_LEFT_THUMB, KEYBOARD_MASK_LEFT_THUMB}, {31, 31}, {KEYBOARD_MASK_GUIDE, KEYBOARD_MASK_GUIDE},
    {KEYBOARD_MASK_LEFT_SHOULDER, KEYBOARD_MASK_LEFT_SHOULDER}, {KEYBOARD_MASK_RIGHT_TRIGGER, KEYBOARD_MASK_RIGHT_TRIGGER},
    {KEYBOARD_MASK_DOWN, KEYBOARD_MASK_LX_D}, {31, 31},
    {KEYBOARD_MASK_Y, KEYBOARD_MASK_Y}, {31, 31}, {KEYBOARD_MASK_UP, KEYBOARD_MASK_LX_U}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {KEYBOARD_MASK_START, KEYBOARD_MASK_START}, {31, 31}, {KEYBOARD_MASK_BACK, KEYBOARD_MASK_BACK}, {KEYBOARD_MASK_LEFT_TRIGGER, KEYBOARD_MASK_LEFT_TRIGGER},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31},
    {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}, {31, 31}
};

static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) {
            return true;
        }
    }
    return false;
}

void process_kbd_report(uint8_t const *report_, uint32_t *keyboard_report, uint8_t keybd_dev_addr, uint8_t keybd_instance) {

    hid_keyboard_report_t *report = (hid_keyboard_report_t *)report_;

    static hid_keyboard_report_t prev_report = {0, 0, {0}};// previous report to check key released
    static bool CAPS_LOCK = false;

    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i]) {
            if(report->keycode[i] == HID_KEY_CAPS_LOCK) {
                if(find_key_in_report(&prev_report, report->keycode[i])){
                    // exist in previous report means the current key is holding
                }
                else {
                    // not existed in previous report means the current key is pressed
                    if (report->keycode[i] == HID_KEY_CAPS_LOCK) {
                        CAPS_LOCK = !CAPS_LOCK;
                        uint8_t leds = (CAPS_LOCK ? KEYBOARD_LED_CAPSLOCK : 0);
                        tuh_hid_set_report(keybd_dev_addr, keybd_instance, 0, HID_REPORT_TYPE_OUTPUT, &leds, sizeof(leds));
                    }
                }
            }
            else {
                bool const is_shift = (report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) | CAPS_LOCK;
                uint8_t ch = keycode2xinput[report->keycode[i]][is_shift ? 1 : 0];
                *keyboard_report |= (ch != 31 ? (1 << ch) : 0);
            }
        }
    }

    prev_report = *report;
}

double get_scaled_mouse_axis(double axisPos, int axis, keyboard_mouse_report_t *keyboard_compose) {
    double *maxAxis;
    double *minAxis;

    if(axis){
        maxAxis = &(keyboard_compose->mouse_report.max_axis_Y);
        minAxis = &(keyboard_compose->mouse_report.min_axis_Y);
    }
    else{
        maxAxis = &(keyboard_compose->mouse_report.max_axis_X);
        minAxis = &(keyboard_compose->mouse_report.min_axis_X);
    }

    *maxAxis = axisPos > *maxAxis ? axisPos : *maxAxis;
    *minAxis = axisPos < *minAxis ? axisPos : *minAxis;

    return axisPos > 0 ? axisPos / *maxAxis : -axisPos / *minAxis;
}