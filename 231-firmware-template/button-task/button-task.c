#include "button-task.h"
#include "pico/stdlib.h"
#include "led-task/led-task.h"

#define BUTTON_PIN        15u
#define DEBOUNCE_US       20000u   // 20 мс в микросекундах

typedef enum {
    BUTTON_STATE_RELEASED,     // отпущена
    BUTTON_STATE_PRESSING,     // нажатие?
    BUTTON_STATE_PRESSED,      // нажата
    BUTTON_STATE_RELEASING,    // отпускание?
} button_state_t;

static button_state_t button_state       = BUTTON_STATE_RELEASED;
static uint64_t       button_changed_us  = 0;
static uint32_t       button_press_count = 0;

// нажатая кнопка замыкает вывод на землю и даёт false
static bool button_level_pressed(void)
{
    return !gpio_get(BUTTON_PIN);
}

void button_task_init(void)
{
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    button_state       = BUTTON_STATE_RELEASED;
    button_changed_us  = 0;
    button_press_count = 0;
}

void button_task_handle(void)
{
    bool pressed = button_level_pressed();
    uint64_t now = time_us_64();

    switch (button_state)
    {
    case BUTTON_STATE_RELEASED:
        if (pressed)
        {
            button_changed_us = now;
            button_state = BUTTON_STATE_PRESSING;
        }
        break;

    case BUTTON_STATE_PRESSING:
        if (!pressed)
        {
            // дребезг или помеха — вернулись
            button_state = BUTTON_STATE_RELEASED;
        }
        else if (now - button_changed_us >= DEBOUNCE_US)
        {
            // нажатие подтверждено
            button_state = BUTTON_STATE_PRESSED;
            button_press_count++;
            led_task_next_state();
        }
        break;

    case BUTTON_STATE_PRESSED:
        if (!pressed)
        {
            button_changed_us = now;
            button_state = BUTTON_STATE_RELEASING;
        }
        break;

    case BUTTON_STATE_RELEASING:
        if (pressed)
        {
            button_state = BUTTON_STATE_PRESSED;
        }
        else if (now - button_changed_us >= DEBOUNCE_US)
        {
            button_state = BUTTON_STATE_RELEASED;
        }
        break;
    }
}

bool button_task_is_pressed(void)
{
    // кнопка нажата и в PRESSED, и в RELEASING: отпускание не подтвердилось
    return button_state == BUTTON_STATE_PRESSED
        || button_state == BUTTON_STATE_RELEASING;
}

uint32_t button_task_get_press_count(void)
{
    return button_press_count;
}