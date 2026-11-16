#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    PI_STATE_IDLE,
    PI_STATE_RUNNING,
    PI_STATE_DONE,
} pi_state_t;

bool       pi_task_start(uint32_t terms);
void       pi_task_handle(void);
pi_state_t pi_task_get_state(void);
void       pi_task_get_result(double *pi, uint32_t *elapsed_ms, uint32_t *terms);