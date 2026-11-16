#pragma once

#include <stdbool.h>

void platform_init(void);
void platform_led_set(bool on);
bool platform_button_read(void);

// режим вывода GP22, который ведёт линию RC-цепи
typedef enum {
    PLATFORM_LINE_PUSH_PULL,
    PLATFORM_LINE_OPEN_DRAIN,
} platform_line_mode_t;

// управление линией GP22
void platform_line_set(platform_line_mode_t mode, bool high);

// чтение напряжения с GP26
float platform_voltage_read(void);