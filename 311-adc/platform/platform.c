#include "platform/platform.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define LED_PIN     25u
#define BUTTON_PIN  15u

// вход АЦП: вывод GP26, канал 0
#define ADC_PIN 26
#define ADC_CHANNEL 0
#define ADC_TEMP_CHANNEL 4

// опорное напряжение АЦП на плате Pico и число уровней 12-разрядного кода
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096

// вывод, через резистор заряжающий конденсатор RC-цепи
#define CHARGE_PIN 22 

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
    adc_set_temp_sensor_enabled(true);

    gpio_init(CHARGE_PIN);
    gpio_set_dir(CHARGE_PIN, GPIO_OUT);
    gpio_put(CHARGE_PIN, false); // Изначально ноль, конденсатор разряжен
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

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return adc_read() * ADC_VREF_V / ADC_LEVELS;
}

float platform_temp_read(void)
{
    adc_select_input(ADC_TEMP_CHANNEL);
    uint16_t raw = adc_read();
    float voltage = raw * ADC_VREF_V / ADC_LEVELS;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void platform_charge_set(bool on)
{
    gpio_put(CHARGE_PIN, on);
}