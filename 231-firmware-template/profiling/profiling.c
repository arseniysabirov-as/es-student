#include "profiling.h"
#include "pico/stdlib.h"
#include <string.h>

typedef struct
{
    const char *name;
    uint32_t    count;
    uint32_t    sum_us;     // сумма замеров, 32 бита
    uint32_t    max_us;
    uint64_t    start_us;   // для текущего замера
    bool        running;
} profiling_stopwatch_t;

static profiling_stopwatch_t stopwatches[PROFILING_STOPWATCH_COUNT];
static bool                  initialized = false;

void profiling_stopwatch_init(uint32_t id, const char *name)
{
    if (id >= PROFILING_STOPWATCH_COUNT)
    {
        return;
    }

    profiling_stopwatch_t *sw = &stopwatches[id];
    sw->name    = name;
    sw->count   = 0;
    sw->sum_us  = 0;
    sw->max_us  = 0;
    sw->start_us = 0;
    sw->running = false;

    initialized = true;
}

void profiling_start(uint32_t id)
{
    if (!initialized || id >= PROFILING_STOPWATCH_COUNT)
    {
        return;
    }

    profiling_stopwatch_t *sw = &stopwatches[id];
    sw->start_us = time_us_64();
    sw->running  = true;
}

void profiling_stop(uint32_t id)
{
    if (!initialized || id >= PROFILING_STOPWATCH_COUNT)
    {
        return;
    }

    profiling_stopwatch_t *sw = &stopwatches[id];
    if (!sw->running)
    {
        return;
    }

    uint32_t elapsed = (uint32_t)(time_us_64() - sw->start_us);
    sw->running = false;

    sw->count++;
    sw->sum_us += elapsed;

    if (elapsed > sw->max_us)
    {
        sw->max_us = elapsed;
    }
}

bool profiling_get(uint32_t id, profiling_result_t *result)
{
    if (!initialized || id >= PROFILING_STOPWATCH_COUNT)
    {
        return false;
    }

    profiling_stopwatch_t *sw = &stopwatches[id];
    if (sw->name == NULL)
    {
        return false;
    }

    result->name = sw->name;
    result->count = sw->count;
    result->mean_us = (sw->count == 0)
        ? 0.0f
        : (float)sw->sum_us / (float)sw->count;
    result->max_us = sw->max_us;
    return true;
}

void profiling_reset(void)
{
    for (uint32_t i = 0; i < PROFILING_STOPWATCH_COUNT; i++)
    {
        stopwatches[i].count  = 0;
        stopwatches[i].sum_us = 0;
        stopwatches[i].max_us = 0;
        stopwatches[i].running = false;
    }
}