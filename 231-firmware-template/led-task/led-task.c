#include "led-task.h"
#include "pico/stdlib.h"

#define LED_PIN            25u
#define INITIAL_PERIOD_MS  1000u

static led_state_t led_state       = LED_STATE_OFF;
static uint32_t    led_period_ms   = INITIAL_PERIOD_MS;
static bool        led_on          = false;
static uint64_t    last_toggle_us  = 0;

// записать уровень в вывод и запомнить его в переменной
static void led_apply(bool on)
{
    gpio_put(LED_PIN, on);
    led_on = on;
}

// проверка периода: следующее срабатывание отсчитывается от прошлого,
// при большом отставании метка перескакивает в now
static bool period_elapsed(uint64_t *last_us, uint64_t period_us)
{
    uint64_t now = time_us_64();

    if (now - *last_us < period_us)
    {
        return false;
    }

    *last_us += period_us;

    if (now - *last_us >= period_us)
    {
        *last_us = now;
    }

    return true;
}

void led_task_init(void)
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    led_period_ms = INITIAL_PERIOD_MS;
    led_task_set_state(LED_STATE_BLINK);
}

void led_task_set_state(led_state_t state)
{
    led_state = state;

    // действие при входе в состояние — один раз, при переключении
    switch (state)
    {
    case LED_STATE_OFF:
        led_apply(false);
        break;

    case LED_STATE_ON:
        led_apply(true);
        break;

    case LED_STATE_BLINK:
        // метка в прошлом на полпериода — первое переключение сразу
        last_toggle_us = time_us_64() - (uint64_t)led_period_ms * 500u;
        break;
    }
}

void led_task_handle(void)
{
    switch (led_state)
    {
    case LED_STATE_OFF:
    case LED_STATE_ON:
        // уровень установлен при входе, делать нечего
        break;

    case LED_STATE_BLINK:
        if (period_elapsed(&last_toggle_us, (uint64_t)led_period_ms * 500u))
        {
            led_apply(!led_on);
        }
        break;
    }
}

led_state_t led_task_get_state(void)
{
    return led_state;
}

bool led_task_set_period_ms(uint32_t period_ms)
{
    if (period_ms == 0)
    {
        return false;
    }

    led_period_ms = period_ms;
    return true;
}

uint32_t led_task_get_period_ms(void)
{
    return led_period_ms;
}