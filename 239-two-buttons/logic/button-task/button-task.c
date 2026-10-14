#include "button-task/button-task.h"
#include "platform/platform.h"
#include "systime/systime.h"
#include <stddef.h>

// время дребезга: 20 мс, выражено в микросекундах
#define DEBOUNCE_US (20 * 1000)
// порог долгого нажатия: 1 с, выражен в микросекундах
#define LONG_PRESS_US (1000 * 1000)

typedef enum {
    BUTTON_STATE_RELEASED,
    BUTTON_STATE_PRESSING,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_RELEASING,
} button_state_t;

typedef struct {
    button_state_t         state;
    uint64_t               changed_us;
    uint64_t               pressed_us;
    uint32_t               press_count;
    button_task_callback_t on_press;
    button_task_callback_t on_long_press;   // NULL — долгого действия нет
    bool                   long_reported;   // долгое уже сообщено
} button_task_data_t;

// по одной машине на каждую кнопку платы
static button_task_data_t buttons[PLATFORM_BUTTON_COUNT];

void button_task_init(platform_button_t button,
                      button_task_callback_t on_press,
                      button_task_callback_t on_long_press)
{
    if (button >= PLATFORM_BUTTON_COUNT)
    {
        return;
    }

    buttons[button].state         = BUTTON_STATE_RELEASED;
    buttons[button].changed_us    = 0;
    buttons[button].pressed_us    = 0;
    buttons[button].press_count   = 0;
    buttons[button].on_press      = on_press;
    buttons[button].on_long_press = on_long_press;
    buttons[button].long_reported = false;
}

static void button_task_handle_one(platform_button_t button, uint64_t now)
{
    button_task_data_t *b = &buttons[button];
    bool pressed = platform_button_read(button);

    switch (b->state)
    {
    case BUTTON_STATE_RELEASED:
        if (pressed)
        {
            b->changed_us = now;
            b->state = BUTTON_STATE_PRESSING;
        }
        break;

    case BUTTON_STATE_PRESSING:
        if (!pressed)
        {
            b->state = BUTTON_STATE_RELEASED;
        }
        else if (now - b->changed_us >= DEBOUNCE_US)
        {
            b->state       = BUTTON_STATE_PRESSED;
            b->pressed_us  = now;
            b->long_reported = false;
            b->press_count++;
            // у кнопки нет долгого действия — сообщаем о нажатии сразу
            if (b->on_long_press == NULL && b->on_press != NULL)
            {
                b->on_press();
            }
        }
        break;

    case BUTTON_STATE_PRESSED:
        if (!pressed)
        {
            b->changed_us = now;
            b->state = BUTTON_STATE_RELEASING;
        }
        else if (b->on_long_press != NULL
                 && !b->long_reported
                 && now - b->pressed_us >= LONG_PRESS_US)
        {
            // держат дольше 1 с — один раз сообщаем о долгом нажатии
            b->long_reported = true;
            b->on_long_press();
        }
        break;

    case BUTTON_STATE_RELEASING:
        if (pressed)
        {
            b->state = BUTTON_STATE_PRESSED;
        }
        else if (now - b->changed_us >= DEBOUNCE_US)
        {
            b->state = BUTTON_STATE_RELEASED;

            // короткое нажатие сообщается при отпускании, если
            // долгого действия у кнопки нет или долгое не сработало
            if (b->on_long_press != NULL
                && !b->long_reported
                && b->on_press != NULL)
            {
                b->on_press();
            }
        }
        break;
    }
}

void button_task_handle(void)
{
    uint64_t now = systime_us();

    for (uint32_t i = 0; i < PLATFORM_BUTTON_COUNT; i++)
    {
        button_task_handle_one((platform_button_t)i, now);
    }
}

bool button_task_is_pressed(platform_button_t button)
{
    if (button >= PLATFORM_BUTTON_COUNT)
    {
        return false;
    }
    // кнопка нажата и в PRESSED, и в RELEASING
    return buttons[button].state == BUTTON_STATE_PRESSED
        || buttons[button].state == BUTTON_STATE_RELEASING;
}

uint32_t button_task_get_press_count(platform_button_t button)
{
    if (button >= PLATFORM_BUTTON_COUNT)
    {
        return 0;
    }
    return buttons[button].press_count;
}