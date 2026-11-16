#include "platform/platform.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/dma.h"

#define LED_PIN     25u
#define BUTTON_PIN  15u

// АЦП
#define ADC_PIN 26
#define ADC_CHANNEL 0
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096

//UART
#define UART0_TX_PIN 0
#define UART0_RX_PIN 1
#define UART_BAUD 115200
// Выводы UART1
#define UART1_TX_PIN 8
#define UART1_RX_PIN 9

// Вывод линии
#define LINE_PIN 22

 // Буфер для прерывания ---
#define RX_BUFFER_SIZE 128


static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint32_t rx_count = 0;

// (в enum adc_user_t)
typedef enum {
    ADC_FREE,
    ADC_CAPTURE,
    ADC_STREAM,
    ADC_BLOCK,    
} adc_user_t;

static volatile adc_user_t adc_user = ADC_FREE;

static int block_channel;
static uint16_t block_samples[PLATFORM_BLOCK_MAX];
static uint32_t block_start_us;
static volatile uint32_t block_end_us;
static volatile bool block_ready;

// АЦП в непрерывном режиме с шагом 44,1 кГц, буфер АЦП с запросом DMA
// на каждый отсчёт
// АЦП в непрерывном режиме
static void adc_free_running_fast(void)
{
    adc_select_input(ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, false); // 12-бит без сдвига
    adc_set_clkdiv(0); // <-- МАКСИМАЛЬНАЯ СКОРОСТЬ (500 кГц)
}

static void adc_one_shot(void)
{
    adc_run(false);
    adc_fifo_drain();
    adc_fifo_setup(false, false, 0, false, false);
    adc_set_clkdiv(0);
}

static void on_block_done(void)
{
    dma_channel_acknowledge_irq1(block_channel);
    block_end_us = time_us_32();
    adc_one_shot();
    block_ready = true;
    adc_user = ADC_FREE;
}

bool platform_block_start(uint32_t count)
{
    if (adc_user != ADC_FREE || count == 0 || count > PLATFORM_BLOCK_MAX) return false;
    
    adc_free_running_fast();

    dma_channel_config cfg = dma_channel_get_default_config(block_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    block_ready = false;
    adc_user = ADC_BLOCK;

    dma_channel_configure(block_channel, &cfg, block_samples, &adc_hw->fifo, count, true);
    block_start_us = time_us_32();
    adc_run(true);
    return true;
}

bool platform_block_done(void) { return block_ready; }
const uint16_t *platform_block_samples(void) { return block_samples; }
uint32_t platform_block_time_us(void) { return block_end_us - block_start_us; }


// обработчик UART0_IRQ: забирает из очереди приёма всё, что там есть
static void on_uart0_rx(void)
{
    while (uart_is_readable(uart0))
    {
        uint8_t byte = uart_get_hw(uart0)->dr & 0xff;
        if (rx_count < RX_BUFFER_SIZE)
        {
            rx_buffer[rx_count++] = byte;
        }
    }
}

void platform_uart_rx_irq_set(bool on)
{
    // Разрешаем прерывание по заполнению очереди (4 байта) и по паузе
    uart_set_irqs_enabled(uart0, on, false);
}

static uart_inst_t *uart_of(platform_uart_t uart)
{
    return uart == PLATFORM_UART0 ? uart0 : uart1;
}

void platform_init(void)
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    adc_init();
    adc_gpio_init(ADC_PIN);

    gpio_init(LINE_PIN);
    platform_line_set(PLATFORM_LINE_PUSH_PULL, false);

    uart_init(uart0, UART_BAUD);
    uart_init(uart1, UART_BAUD);
    gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    
    rx_count = 0; // <-- ОБНУЛЯЕМ БУФЕР ПРИ СТАРТЕ

    irq_set_exclusive_handler(UART0_IRQ, on_uart0_rx);
    irq_set_enabled(UART0_IRQ, true);

    // канал блока сообщает о конце через DMA_IRQ_1
    block_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq1_enabled(block_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, on_block_done);
    irq_set_enabled(DMA_IRQ_1, true);
    
    // stream_channels и platform_tone_init() удалены, так как они не нужны в 331-uart
}

void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(void)
{
    // нажатая кнопка замыкает вывод на землю и даёт ноль
    return !gpio_get(BUTTON_PIN);
}

void platform_line_set(platform_line_mode_t mode, bool high)
{
    if (mode == PLATFORM_LINE_PUSH_PULL) {
        gpio_disable_pulls(LINE_PIN);
        gpio_put(LINE_PIN, high);
        gpio_set_dir(LINE_PIN, GPIO_OUT);
        return;
    }

    // Открытый сток: в регистре выхода всегда 0, переключаем только направление
    gpio_pull_up(LINE_PIN);
    gpio_put(LINE_PIN, 0);
    gpio_set_dir(LINE_PIN, high ? GPIO_IN : GPIO_OUT);
}

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return adc_read() * ADC_VREF_V / ADC_LEVELS;
}

void platform_uart_loop_set(bool internal)
{
    uint function = internal ? GPIO_FUNC_NULL : GPIO_FUNC_UART;
    gpio_set_function(UART0_TX_PIN, function);
    gpio_set_function(UART0_RX_PIN, function);

    if (internal)
    {
        hw_set_bits(&uart_get_hw(uart0)->cr, UART_UARTCR_LBE_BITS);
    }
    else
    {
        hw_clear_bits(&uart_get_hw(uart0)->cr, UART_UARTCR_LBE_BITS);
    }
}

void platform_uart_clear(platform_uart_t uart)
{
    uart_inst_t *u = uart_of(uart);
    while (uart_is_readable(u)) {
        (void)uart_get_hw(u)->dr; // чистим аппаратную очередь
    }
    uart_get_hw(u)->rsr = 0; // сброс ошибок

    // Очищаем наш программный буфер для UART0
    if (uart == PLATFORM_UART0) {
        rx_count = 0;
    }
}

bool platform_uart_put(platform_uart_t uart, uint8_t byte)
{
    uart_inst_t *u = uart_of(uart);

    if (!uart_is_writable(u))
    {
        return false;
    }

    // место в очереди есть, поэтому uart_putc_raw() не ждёт
    uart_putc_raw(u, byte);
    return true;
}

bool platform_uart_busy(platform_uart_t uart)
{
    uart_hw_t *hw = uart_get_hw(uart_of(uart));
    // TXFE (1 - FIFO пуст, 0 - есть данные) и BUSY (1 - идет передача)
    // Считаем занятым, если в очереди есть данные ИЛИ передатчик сейчас работает
    return ((hw->fr & UART_UARTFR_TXFE_BITS) == 0) || ((hw->fr & UART_UARTFR_BUSY_BITS) != 0);
}

size_t platform_uart_read(platform_uart_t uart, uint8_t *data, size_t size)
{
    uart_inst_t *u = uart_of(uart);
    size_t n = 0;

    // 1. Сначала забираем данные из буфера прерывания (только для UART0)
    if (uart == PLATFORM_UART0) {
        for (uint32_t i = 0; i < rx_count && n < size; i++) {
            data[n++] = rx_buffer[i];
        }
    }

    // 2. Затем добираем то, что осталось в аппаратной очереди
    while (uart_is_readable(u) && n < size) {
        data[n++] = uart_get_hw(u)->dr & 0xff;
    }

    return n;
}

uint32_t platform_uart_errors(platform_uart_t uart)
{
    return uart_get_hw(uart_of(uart))->rsr & 0xf;
}

uint32_t platform_uart_baud_set(platform_uart_t uart, uint32_t baud)
{
    return uart_set_baudrate(uart_of(uart), baud);
}

void platform_uart_parity_set(platform_uart_t uart, platform_parity_t parity)
{
    static const uart_parity_t parities[] = {
        UART_PARITY_NONE,
        UART_PARITY_EVEN,
        UART_PARITY_ODD,
    };
    uart_set_format(uart_of(uart), 8, 1, parities[parity]);
}