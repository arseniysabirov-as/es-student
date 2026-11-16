#pragma once

#include <stdbool.h>
#include <stddef.h> // для size_t
#include <stdint.h>

void platform_init(void);
void platform_led_set(bool on);
bool platform_button_read(void);

#define PLATFORM_UART_FE 0x1
#define PLATFORM_UART_PE 0x2
#define PLATFORM_UART_BE 0x4
#define PLATFORM_UART_OE 0x8

// режим вывода GP22, который ведёт линию RC-цепи
typedef enum {
    PLATFORM_LINE_PUSH_PULL,
    PLATFORM_LINE_OPEN_DRAIN,
} platform_line_mode_t;

// -- UART ---
typedef enum {
    PLATFORM_UART0,
    PLATFORM_UART1,
} platform_uart_t;

typedef enum {
    PLATFORM_PARITY_NONE,
    PLATFORM_PARITY_EVEN,
    PLATFORM_PARITY_ODD,
} platform_parity_t;

// управление линией GP22
void platform_line_set(platform_line_mode_t mode, bool high);

// чтение напряжения с GP26
float platform_voltage_read(void);

void platform_uart_loop_set(bool internal);
void platform_uart_clear(platform_uart_t uart);
bool platform_uart_put(platform_uart_t uart, uint8_t byte);
bool platform_uart_busy(platform_uart_t uart);
size_t platform_uart_read(platform_uart_t uart, uint8_t *data, size_t size);
uint32_t platform_uart_errors(platform_uart_t uart);

// возвращает скорость, которая получилась после округления делителя
uint32_t platform_uart_baud_set(platform_uart_t uart, uint32_t baud);
void platform_uart_parity_set(platform_uart_t uart, platform_parity_t parity);

// прерывание приёма UART0: принятые байты забирает обработчик
void platform_uart_rx_irq_set(bool on);