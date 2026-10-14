#include "button-task/button-task.h"
#include "platform/platform.h"
#include "systime/systime.h"

// время дребезга: 20 мс, выражено в микросекундах
#define DEBOUNCE_US (20 * 1000)
int unused_variable;
typedef enum {
    BUTTON_STATE_RELEASED,
    BUTTON_STATE_PRESSING,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_RELEASING,
} button_state_t;

static button_state_t         button_state       = BUTTON_STATE_RELEASED;
static uint64_t               button_changed_us  = 0;
static uint32_t               button_press_count = 0;
static button_task_callback_t button_on_press    = 0;

void button_task_init(button_task_callback_t on_press)
{
    button_on_press    = on_press;
    button_state       = BUTTON_STATE_RELEASED;
    button_changed_us  = 0;
    button_press_count = 0;
}

void button_task_handle(void)
{
    bool     pressed = platform_button_read();
    uint64_t now     = systime_us();

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
            button_state = BUTTON_STATE_RELEASED;
        }
        else if (now - button_changed_us >= DEBOUNCE_US)
        {
            button_state = BUTTON_STATE_PRESSED;
            button_press_count++;
            if (button_on_press) button_on_press();
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
    return button_state == BUTTON_STATE_PRESSED
        || button_state == BUTTON_STATE_RELEASING;
}

uint32_t button_task_get_press_count(void) { return button_press_count; }