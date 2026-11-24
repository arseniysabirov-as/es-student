#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PLATFORM_SPI_CS_COUNT 2

void platform_master_init(void);

// Запись байта в регистр
bool platform_i2c_write(uint8_t address, uint8_t reg, uint8_t value);

// Чтение байта из регистра (с повторным START)
bool platform_i2c_read(uint8_t address, uint8_t reg, uint8_t *value);

void platform_spi_write(uint8_t cs, uint8_t command, uint8_t value);
uint8_t platform_spi_read(uint8_t cs, uint8_t command, uint8_t pause_us);

// ---  Второе ядро и светодиод ---
void platform_start_core1(void (*entry)(void));
void platform_slave_init(void);
void platform_led_set(bool on);
uint32_t platform_time_ms(void);