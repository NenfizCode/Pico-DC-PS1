//
// Created by loc15 on 5/02/26.
//

#ifndef PICOGAMEPADCONVERTER_PROCESS_KEYBOARD_H
#define PICOGAMEPADCONVERTER_PROCESS_KEYBOARD_H

typedef struct {
    uint8_t buttons;
    int8_t  x;
    int8_t  y;
    double min_axis_X;
    double max_axis_X;
    double min_axis_Y;
    double max_axis_Y;
} mouse_report_t;

typedef struct {
    uint32_t keyboard_report;
    uint32_t current_keyboard_report;
    mouse_report_t mouse_report;
} keyboard_mouse_report_t;

void process_kbd_report(uint8_t const *report_, uint32_t *keyboard_report, uint8_t keybd_dev_addr, uint8_t keybd_instance);

double get_scaled_mouse_axis(double axisPos, int axis, keyboard_mouse_report_t *keyboard_compose);

#endif //PICOGAMEPADCONVERTER_PROCESS_KEYBOARD_H