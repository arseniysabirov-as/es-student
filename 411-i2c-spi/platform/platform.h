#pragma once

#include <stdbool.h>
#include <stdint.h>

void platform_master_init(void);

// Запись байта в регистр
bool platform_i2c_write(uint8_t address, uint8_t reg, uint8_t value);

// Чтение байта из регистра (с повторным START)
bool platform_i2c_read(uint8_t address, uint8_t reg, uint8_t *value);