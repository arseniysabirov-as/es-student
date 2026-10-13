#include "pi-task.h"
#include "pico/stdlib.h"
#include "systime/systime.h"

// член ряда стоит около 585 тактов, при 125 МГц это 4,7 мкс;
// обработчик должен уложиться в 0,5 мс — это около 100 членов
#define PI_TERMS_PER_CALL  100u

static pi_state_t pi_state      = PI_STATE_IDLE;
static uint32_t   pi_terms      = 0;    // сколько всего членов
static uint32_t   pi_k          = 0;    // номер следующего члена
static double     pi_sum        = 0.0;
static double     pi_sign       = 1.0;
static uint64_t   pi_start_us   = 0;
static uint32_t   pi_elapsed_ms = 0;

bool pi_task_start(uint32_t terms)
{
    if (pi_state == PI_STATE_RUNNING)
    {
        return false;
    }

    if (terms == 0)
    {
        return false;
    }

    pi_terms      = terms;
    pi_k          = 0;
    pi_sum        = 0.0;
    pi_sign       = 1.0;
    pi_start_us   = systime_us();
    pi_elapsed_ms = 0;
    pi_state      = PI_STATE_RUNNING;

    return true;
}

void pi_task_handle(void)
{
    if (pi_state != PI_STATE_RUNNING)
    {
        return;
    }

    // конец порции: k + PI_TERMS_PER_CALL может переполниться возле
    // 4 294 967 295. Если сумма стала меньше k — переполнилась,
    // значит считаем до последнего члена.
    uint32_t next_k = pi_k + PI_TERMS_PER_CALL;
    uint32_t end_k  = (next_k < pi_k) ? pi_terms : next_k;
    if (end_k > pi_terms)
    {
        end_k = pi_terms;
    }

    for (uint32_t i = pi_k; i < end_k; i++)
    {
        pi_sum += pi_sign / (2.0 * i + 1.0);
        pi_sign = -pi_sign;
    }

    pi_k = end_k;

    if (pi_k >= pi_terms)
    {
        pi_sum *= 4.0;
        pi_elapsed_ms = (uint32_t)((systime_us() - pi_start_us) / 1000);
        pi_state = PI_STATE_DONE;
    }
}

pi_state_t pi_task_get_state(void)
{
    return pi_state;
}

void pi_task_get_result(double *pi, uint32_t *elapsed_ms, uint32_t *terms)
{
    if (pi != NULL)
    {
        *pi = pi_sum;
    }
    if (elapsed_ms != NULL)
    {
        *elapsed_ms = pi_elapsed_ms;
    }
    if (terms != NULL)
    {
        *terms = pi_terms;
    }
}