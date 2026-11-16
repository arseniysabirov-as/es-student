#include "edge-task/edge-task.h"
#include "systime/systime.h"

// пороги EMPTY_V 0.05f, RISE_V 2.0f, FULL_V 3.2f, FALL_V 1.3f, TIMEOUT_US 2000000
#define EMPTY_V    0.05f
#define RISE_V     2.0f
#define FULL_V     3.2f
#define FALL_V     1.3f
#define TIMEOUT_US 2000000

// состояние, режим линии, функция результата, начало фазы, время фронта вверх
typedef enum {
    STATE_IDLE,
    STATE_DISCHARGE,
    STATE_RISE,
    STATE_CHARGE,
    STATE_FALL,
} edge_state_t;

static edge_state_t         state = STATE_IDLE;
static platform_line_mode_t current_mode;
static edge_task_callback_t callback = 0;
static uint64_t             phase_start_us = 0;
static uint32_t             rise_time_us = 0;

// enter(): уровень на линии через platform_line_set(), начало фазы — сейчас, новое состояние
static void enter_state(edge_state_t new_state, bool high)
{
    state = new_state;
    platform_line_set(current_mode, high);
    phase_start_us = systime_us();
}
// finish(): состояние IDLE, затем вызов функции результата
static void finish(bool ok)
{
    state = STATE_IDLE;
    if (callback) {
        edge_task_callback_t cb = callback;
        callback = 0;
        uint32_t fall_time_us = (uint32_t)(systime_us() - phase_start_us);
        cb(current_mode, ok, rise_time_us, fall_time_us);
    }
}
// edge_task_start(): замер уже идёт — false; иначе запомнить режим и функцию,
// ноль на линии, состояние DISCHARGE
bool edge_task_start(platform_line_mode_t mode, edge_task_callback_t on_done)
{
    if (state != STATE_IDLE) {
        return false;
    }
    current_mode = mode;
    callback = on_done;
    rise_time_us = 0;
    enter_state(STATE_DISCHARGE, false); // Разряжаем линию
    return true;
}
// edge_task_handle(): в IDLE ничего; иначе прошедшее время фазы и напряжение узла,
// переходы по таблице, фаза дольше TIMEOUT_US — finish(false, 0)
void edge_task_handle(void)
{
    if (state == STATE_IDLE) {
        return;
    }

    uint64_t now = systime_us();
    uint64_t elapsed = now - phase_start_us;

    if (elapsed > TIMEOUT_US) {
        finish(false);
        return;
    }

    float v = platform_voltage_read();

    switch (state) {
    case STATE_DISCHARGE:
        if (v < EMPTY_V) enter_state(STATE_RISE, true);
        break;
    case STATE_RISE:
        if (v > RISE_V) {
            rise_time_us = (uint32_t)elapsed;
            enter_state(STATE_CHARGE, true);
        }
        break;
    case STATE_CHARGE:
        if (v > FULL_V) enter_state(STATE_FALL, false);
        break;
    case STATE_FALL:
        if (v < FALL_V) finish(true);
        break;
    default:
        break;
    }
}