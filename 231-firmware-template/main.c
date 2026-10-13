#include <stdio.h>
#include "pico/stdlib.h"
#include "led/led.h"
#include "logging/log.h"
#include "device/device.h"
#include "profiling/profiling.h"
#include "stdio-text-protocol/stdio-text-protocol.h"
#include "api/api.h"

const uint BUTTON_PIN = 15;
const uint DEBOUNCE_MS = 20;

bool get_button_debounce(uint pin)
{
    bool state = gpio_get(pin);
    sleep_ms(DEBOUNCE_MS);
    return state && gpio_get(pin);
}

int main(void)
{
    stdio_init_all();
    led_init();

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    stdio_text_protocol_init();
    profiling_init();

    bool previous = false;

    while (1)
    {
        profiling_iteration();

        const command_t *command = stdio_text_protocol_handle();
        if (command != NULL)
        {
            api_handle(command);
        }

        bool current = get_button_debounce(BUTTON_PIN);
        if (previous && !current)
        {
            led_toggle();
            LOG_INF("led %s\n", led_is_on() ? "on" : "off");
        }
        previous = current;
    }
}