#pragma once

#include <stdbool.h>
#include <stdint.h>

// номер кнопки на плате; сколько кнопок, знает только platform
typedef enum {
    PLATFORM_BUTTON_LED,        // GP15 — состояние светодиода
    PLATFORM_BUTTON_PERIOD,     // GP14 — период мигания
    PLATFORM_BUTTON_COUNT
} platform_button_t;

void platform_init(void);
void platform_led_set(bool on);

// нажата ли кнопка с этим номером сейчас, без подавления дребезга
bool platform_button_read(platform_button_t button);