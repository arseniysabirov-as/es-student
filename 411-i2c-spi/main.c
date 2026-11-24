#include "api/api.h"
#include "platform/platform.h"
#include "stdio-text-protocol/stdio-text-protocol.h"
#include "slave/slave.h"

#include <stddef.h>

// --- Точка входа для второго ядра 1---
static void slave_core(void)
{
    slave_device_init();

    while (1)
    {
        slave_device_handle();
    }
}

int main(void)
{
    platform_master_init();
    stdio_text_protocol_init();

    // Запускаем второе ядро
    platform_start_core1(slave_core); 

    while (1)
    {
        const command_t *command = stdio_text_protocol_handle();
        if (command != NULL)
        {
            api_handle(command);
        }
    }
}