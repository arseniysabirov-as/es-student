#include "platform/platform.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/i2c_slave.h"

#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#define BUS_BAUD 100000
#define I2C_TIMEOUT_US 10000
#define SPI_CS_HOLD_US 2

// Пин CS для SPI-ведомого
#define SPI_SLAVE_CS 13

// Пины CS для SPI-ведущего
static const uint spi_cs_pins[PLATFORM_SPI_CS_COUNT] = {16, 5};

// ==========================================
// ИНИЦИАЛИЗАЦИЯ ВЕДУЩЕГО (вызывается на Core 0)
// ==========================================
void platform_master_init(void)
{
    stdio_init_all();

    // 1. Настройка I2C0 (Master)
    i2c_init(i2c0, BUS_BAUD);
    gpio_disable_pulls(8);
    gpio_set_function(8, GPIO_FUNC_I2C);   // SDA Master
    gpio_disable_pulls(9);
    gpio_set_function(9, GPIO_FUNC_I2C);   // SCL Master

    // 2. Настройка SPI0 (Master)
    for (uint cs = 0; cs < PLATFORM_SPI_CS_COUNT; cs++) {
        uint pin = spi_cs_pins[cs];
        gpio_init(pin);
        gpio_put(pin, 1);                  // CS неактивен
        gpio_set_dir(pin, GPIO_OUT);
    }

    spi_init(spi0, BUS_BAUD);
    spi_set_format(spi0, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(2, GPIO_FUNC_SPI);   // SCK Master
    gpio_set_function(3, GPIO_FUNC_SPI);   // MOSI Master
    gpio_pull_down(4);                     // MISO Master (подтяжка к земле)
    gpio_set_function(4, GPIO_FUNC_SPI);   // MISO Master
}

bool platform_i2c_write(uint8_t address, uint8_t reg, uint8_t value)
{
    const uint8_t bytes[] = {reg, value};
    return i2c_write_timeout_us(i2c0, address, bytes, sizeof(bytes), false,
                                I2C_TIMEOUT_US) == sizeof(bytes);
}

bool platform_i2c_read(uint8_t address, uint8_t reg, uint8_t *value)
{
    // Повторный START (nostop = true) для чтения
    if (i2c_write_timeout_us(i2c0, address, &reg, 1, true, I2C_TIMEOUT_US) != 1) {
        return false;
    }
    return i2c_read_timeout_us(i2c0, address, value, 1, false, I2C_TIMEOUT_US) == 1;
}

void platform_spi_write(uint8_t cs, uint8_t command, uint8_t value)
{
    const uint8_t bytes[] = {command, value};
    gpio_put(spi_cs_pins[cs], 0);
    spi_write_blocking(spi0, bytes, sizeof(bytes));
    busy_wait_us_32(SPI_CS_HOLD_US);
    gpio_put(spi_cs_pins[cs], 1);
}

uint8_t platform_spi_read(uint8_t cs, uint8_t command, uint8_t pause_us)
{
    uint8_t value;
    gpio_put(spi_cs_pins[cs], 0);
    spi_write_blocking(spi0, &command, 1);
    if (pause_us != 0) {
        busy_wait_us_32(pause_us);
    }
    spi_read_blocking(spi0, 0x00, &value, 1);
    busy_wait_us_32(SPI_CS_HOLD_US);
    gpio_put(spi_cs_pins[cs], 1);
    return value;
}

// ==========================================
// ИНИЦИАЛИЗАЦИЯ ВЕДОМОГО (вызывается на Core 1)
// ==========================================

void platform_start_core1(void (*entry)(void))
{
    multicore_launch_core1(entry);
}

void platform_led_set(bool on)
{
    gpio_put(PICO_DEFAULT_LED_PIN, on);
}

// --- I2C Slave (Core 1) ---
static platform_register_read_t i2c_read_register;
static platform_register_write_t i2c_write_register;
static uint8_t i2c_register;
static enum { I2C_REGISTER, I2C_VALUE, I2C_DONE } i2c_phase;

static void i2c_slave_event(i2c_inst_t *i2c, i2c_slave_event_t event)
{
    switch (event) {
    case I2C_SLAVE_RECEIVE:
        while (i2c_get_read_available(i2c)) {
            uint8_t byte = i2c_read_byte_raw(i2c);
            if (i2c_phase == I2C_REGISTER) {
                i2c_register = byte;
                i2c_phase = I2C_VALUE;
            } else if (i2c_phase == I2C_VALUE) {
                i2c_write_register(i2c_register, byte);
                i2c_phase = I2C_DONE;
            }
        }
        break;
    case I2C_SLAVE_REQUEST:
        i2c_write_byte_raw(i2c, i2c_read_register(i2c_register));
        break;
    case I2C_SLAVE_FINISH:
        i2c_phase = I2C_REGISTER;
        break;
    }
}

// --- SPI Slave (Core 1) ---
void platform_spi_slave_reset(void)
{
    spi_init(spi1, BUS_BAUD);
    spi_set_format(spi1, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    spi_set_slave(spi1, true);
    spi_get_hw(spi1)->dr = 0x00; // Очистка буфера
}

bool platform_spi_slave_selected(void)
{
    return !gpio_get(SPI_SLAVE_CS);
}

bool platform_spi_slave_receive(uint8_t *byte)
{
    if (!spi_is_readable(spi1)) {
        return false;
    }
    *byte = (uint8_t)spi_get_hw(spi1)->dr;
    return true;
}

void platform_spi_slave_reply(uint8_t byte)
{
    spi_get_hw(spi1)->dr = byte;
}

void platform_slave_init(uint8_t address, 
                         platform_register_read_t read_register,
                         platform_register_write_t write_register)
{
    // 1. Настройка светодиода
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // 2. Настройка I2C1 (Slave)
    i2c_read_register = read_register;
    i2c_write_register = write_register;
    i2c_init(i2c1, BUS_BAUD);
    gpio_disable_pulls(6);
    gpio_set_function(6, GPIO_FUNC_I2C);   // SDA Slave
    gpio_disable_pulls(7);
    gpio_set_function(7, GPIO_FUNC_I2C);   // SCL Slave
    i2c_slave_init(i2c1, address, i2c_slave_event);

    // 3. Настройка SPI1 (Slave)
    gpio_set_function(10, GPIO_FUNC_SPI);  // SCK Slave (вход)
    gpio_set_function(11, GPIO_FUNC_SPI);  // TX Slave (MISO, выход)
    gpio_set_function(12, GPIO_FUNC_SPI);  // RX Slave (MOSI, вход)
    gpio_set_function(13, GPIO_FUNC_SPI);  // CS Slave (вход)
    gpio_pull_up(SPI_SLAVE_CS);            // Подтяжка CS к питанию

    platform_spi_slave_reset();
}