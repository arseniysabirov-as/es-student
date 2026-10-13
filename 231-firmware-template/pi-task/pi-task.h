#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PI_STATE_IDLE,      // не запускался
    PI_STATE_RUNNING,   // считает
    PI_STATE_DONE,      // готово
} pi_state_t;

// запускает расчёт; false — расчёт уже идёт
bool pi_task_start(uint32_t terms);

// обработчик в суперцикле: считает одну порцию
void pi_task_handle(void);

// текущее состояние
pi_state_t pi_task_get_state(void);

// результат: значение π, время в миллисекундах, число членов
// имеет смысл только в состоянии PI_STATE_DONE
void pi_task_get_result(double *pi, uint32_t *elapsed_ms, uint32_t *terms);