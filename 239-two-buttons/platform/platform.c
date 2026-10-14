#include "platform/platform.h"
#include "pico/stdlib.h"

#define LED_PIN 25u

// номер вывода каждой кнопки — знание платформы, наружу не торчит
static const uint button_pins[PLATFORM_BUTTON_COUNT] = {
    [PLATFORM_BUTTON_LED] = 15u,
    [PLATFORM_BUTTON_PERIOD] = 14u,
};

void platform_init(void)
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    for (uint32_t i = 0; i < PLATFORM_BUTTON_COUNT; i++)
    {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }
}

void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(platform_button_t button)
{
    if (button >= PLATFORM_BUTTON_COUNT)
    {
        return false;
    }
    // нажатая кнопка замыкает вывод на землю и даёт ноль
    return !gpio_get(button_pins[button]);
}