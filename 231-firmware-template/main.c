#include <stdio.h>
#include "pico/stdlib.h"
#include "device/device.h"
#include "profiling/profiling.h"
#include "stdio-text-protocol/stdio-text-protocol.h"
#include "api/api.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"

int main(void)
{
    stdio_init_all();

    led_task_init();
    button_task_init();
    stdio_text_protocol_init();
    profiling_init();

    while (1)
    {
        profiling_iteration();
        led_task_handle();
        button_task_handle();
        pi_task_handle();       // <-- НОВОЕ

        const command_t *command = stdio_text_protocol_handle();
        if (command != NULL)
        {
            api_handle(command);
        }
    }
}