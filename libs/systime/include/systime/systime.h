#pragma once

#include <stdbool.h>
#include <stdint.h>

uint64_t systime_us(void);
uint64_t systime_ms(void);

// проверка периода: следующее срабатывание отсчитывается от прошлого,
// при отставании больше периода метка перескакивает в now
bool systime_period_elapsed(uint64_t *last_us, uint64_t period_us);