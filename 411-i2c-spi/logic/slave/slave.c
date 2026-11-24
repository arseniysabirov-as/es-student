#include "slave/slave.h"
#include "platform/platform.h"
#include <stdatomic.h>

// Состояние светодиода: общее для I2C и SPI
static _Atomic uint8_t led_state;

// --- Состояние кадра SPI ---
static enum
{
    SPI_COMMAND,
    SPI_DATA,
    SPI_DONE
} spi_phase;

static uint8_t spi_register;
static bool spi_reading;

// --- Общие функции доступа к регистрам ---

static uint8_t read_register(uint8_t reg)
{
    if (reg == SLAVE_REG_STATE)
    {
        return atomic_load_explicit(&led_state, memory_order_relaxed);
    }

    return 0xFF;
}

static void write_register(uint8_t reg, uint8_t value)
{
    if (reg == SLAVE_REG_CMD && (value == 'e' || value == 'd'))
    {
        atomic_store_explicit(&led_state, value == 'e', memory_order_relaxed);
    }
    else if (reg == SLAVE_REG_STATE && value <= 1)
    {
        atomic_store_explicit(&led_state, value, memory_order_relaxed);
    }
}

// --- Обработка байтов SPI ---

static void handle_spi_byte(uint8_t byte)
{
    switch (spi_phase)
    {
    case SPI_COMMAND:
        // Старший бит: 1 - чтение, 0 - запись. Младшие 7 бит - номер регистра.
        spi_register = byte & 0x7F;
        spi_reading = (byte & 0x80) != 0;

        if (spi_reading)
        {
            platform_spi_slave_reply(read_register(spi_register));
        }
        else
        {
            platform_spi_slave_reply(0x00);
        }

        spi_phase = SPI_DATA;
        break;

    case SPI_DATA:
        if (!spi_reading)
        {
            write_register(spi_register, byte);
        }

        spi_phase = SPI_DONE;
        break;

    case SPI_DONE:
        break;
    }
}

// --- Публичные функции задачи ---

void slave_device_init(void)
{
    platform_slave_init(SLAVE_I2C_ADDRESS, read_register, write_register);
}

void slave_device_handle(void)
{
    uint8_t byte;

    while (platform_spi_slave_receive(&byte))
    {
        handle_spi_byte(byte);
    }

    // CS поднялся — кадр закончился. Сбрасываем очереди и состояние.
    if (spi_phase != SPI_COMMAND && !platform_spi_slave_selected())
    {
        platform_spi_slave_reset();
        spi_phase = SPI_COMMAND;
    }

    platform_led_set(atomic_load_explicit(&led_state, memory_order_relaxed) != 0);
}