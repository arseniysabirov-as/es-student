#define LINE_SIZE 32
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sio.h"
#include "led.h"
#include "log.h"
#include "device.h"

char line[LINE_SIZE];
uint line_length = 0;

const uint BUTTON_PIN = 15;
const uint DEBOUNCE_MS = 20;

bool get_button_debounce(uint pin)
{
    bool state = gpio_get(pin);
    sleep_ms(DEBOUNCE_MS);
    return state && gpio_get(pin);
}

void handle_command(int command)
{
    if (strcmp(command, "enable") == 0)
    {
        led_set(true);
        LOG_INF("led %s\n", led_is_on() ? "on" : "off");
    }
    else if (strcmp(command, "disable") == 0)
    {
        led_set(false);
        LOG_INF("led %s\n", led_is_on() ? "on" : "off");
    }
    else if (strcmp(command, "version") == 0)
    {
        log_version();
    }
    else if (strcmp(command, "info") == 0)
    {
        device_info();
    }
    else
    {
        LOG_ERR("unknown command: %s\n", command);
    }
    
}

void read_line(void)
{
    int symbol = getchar_timeout_us(0);

    if (symbol == PICO_ERROR_TIMEOUT)
    {
        return;
    }

    if (symbol == '\r' || symbol == '\n')
    {
        putchar('\n');
        line[line_length] = '\0';

        if (line_length > 0)
        {
            LOG_DBG("got %s\n", line);
            handle_command(line);
        }

        line_length = 0;
        return;
    }

    if (line_length + 1 < LINE_SIZE)
    {
        line[line_length] = (char)symbol;
        line_length = line_length + 1;
        putchar(symbol);
    }
}

int main()
{
    stdio_init_all();

    led_init();

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    bool led = false;
    bool previous = false;

   volatile uint32_t *gpio_out_set = (uint32_t *)(SIO_BASE + SIO_GPIO_OUT_SET_OFFSET);
   volatile uint32_t *gpio_out_clr = (uint32_t *)(SIO_BASE + SIO_GPIO_OUT_CLR_OFFSET);



  while (1)
    {
        bool current = get_button_debounce(BUTTON_PIN);

       if (previous == true && current == false)
        {
            led_toggle();
            LOG_INF("led %s\n", led_is_on() ? "on" : "off");
        }

        previous = current;

        void read_line(void)
    }
}