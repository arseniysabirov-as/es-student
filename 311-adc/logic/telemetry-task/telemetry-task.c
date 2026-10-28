#include "telemetry-task/telemetry-task.h"
#include "platform/platform.h"
#include "systime/systime.h"

#define MIN_PERIOD_MS 10

static telemetry_state_t         state = TELEMETRY_STATE_IDLE;
static telemetry_task_callback_t on_record_cb = 0;
static uint32_t                  current_period_ms = 100;
static uint64_t                  last_time_us = 0;

void telemetry_task_init(telemetry_task_callback_t on_record)
{
    on_record_cb = on_record;
    state = TELEMETRY_STATE_IDLE;
}

bool telemetry_task_start(uint32_t period_ms)
{
    if (period_ms < MIN_PERIOD_MS)
    {
        return false;
    }

    current_period_ms = period_ms;
    // Сдвигаем метку в прошлое, чтобы первая запись случилась сразу
    last_time_us = systime_us() - (uint64_t)period_ms * 1000;
    state = TELEMETRY_STATE_RUN;
    return true;
}

void telemetry_task_stop(void)
{
    state = TELEMETRY_STATE_IDLE;
}

void telemetry_task_handle(void)
{
    if (state != TELEMETRY_STATE_RUN)
    {
        return;
    }

    if (systime_period_elapsed(&last_time_us, (uint64_t)current_period_ms * 1000))
    {
        telemetry_record_t record;
        record.time_us   = systime_us();
        record.voltage_v = platform_voltage_read();
        record.temp_c    = platform_temp_read();

        if (on_record_cb != 0)
        {
            on_record_cb(&record);
        }
    }
}

telemetry_state_t telemetry_task_get_state(void)
{
    return state;
}

uint32_t telemetry_task_get_period_ms(void)
{
    return current_period_ms;
}