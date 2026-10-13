#pragma once

#include <stdbool.h>
#include <stdint.h>

// Максимум секундомеров в приборе.
// Сумма замеров хранится в uint32_t: один секундомер переполнится
// только через 71 минуту при замыкании на 1 мс.
#define PROFILING_STOPWATCH_COUNT 8

typedef struct
{
    const char *name;
    uint32_t    count;      // число замеров
    float       mean_us;    // среднее в микросекундах
    uint32_t    max_us;     // максимум в микросекундах
} profiling_result_t;

void profiling_stopwatch_init(uint32_t id, const char *name);
void profiling_start(uint32_t id);
void profiling_stop(uint32_t id);

// false — секундомер с таким номером не заведён
bool profiling_get(uint32_t id, profiling_result_t *result);

void profiling_reset(void);