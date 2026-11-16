#include "api/api.h"
#include "api/api-commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "device/device.h"
#include "firmware.h"
#include "profiling/profiling.h"
#include "systime/systime.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "edge-task/edge-task.h"
#include "uart-task/uart-task.h"
#include <ctype.h> // для isprint()

static bool parse_u32(const char *text, uint32_t *value)
{
    if (text == NULL || !isdigit((unsigned char)text[0])) return false;

    errno = 0;
    char *end = NULL;
    unsigned long number = strtoul(text, &end, 10);

    if (*end != '\0' || errno == ERANGE || number > UINT32_MAX) return false;

    *value = (uint32_t)number;
    return true;
}

// print_bytes(): байты подряд, непечатные (isprint() == 0) — точкой
static void print_bytes(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        char c = (char)data[i];
        putchar(isprint((unsigned char)c) ? c : '.');
    }
}

// print_errors(): err: none или err: и через пробел FE, PE, BE, OE по битам признаков
static void print_errors(uint32_t errors)
{
    if (errors == 0) {
        printf("err: none");
        return;
    }
    printf("err:");
    if (errors & PLATFORM_UART_FE) printf(" FE");
    if (errors & PLATFORM_UART_PE) printf(" PE");
    if (errors & PLATFORM_UART_BE) printf(" BE");
    if (errors & PLATFORM_UART_OE) printf(" OE");
}

// ─── info ───────────────────────────────────────────────────
static void cmd_info(const command_t *command)
{
    (void)command;

    device_info_t info;
    device_get_info(&info);

    printf("name: %s\n",     FIRMWARE_NAME);
    printf("version: %s\n",  FIRMWARE_VERSION);
    printf("project: %s\n",  FIRMWARE_PROJECT);
    printf("repo: %s\n",     FIRMWARE_REPO);
    printf("board: %s\n",    info.board);
    printf("serial: %s\n",   info.serial);
    printf("chip: manufacturer 0x%03x, part 0x%04x, revision %u\n",
           (unsigned)info.manufacturer, (unsigned)info.part,
           (unsigned)info.revision);
    printf("pico-sdk: %s\n", info.sdk_version);
}

// ─── uptime ─────────────────────────────────────────────────
static void cmd_uptime(const command_t *command)
{
    (void)command;
    printf("uptime: %u ms\n", (unsigned)systime_ms());
}

// ─── led ────────────────────────────────────────────────────
static void print_led_state(void)
{
    const char *name;
    switch (led_task_get_state())
    {
    case LED_STATE_OFF:   name = "off";   break;
    case LED_STATE_ON:    name = "on";    break;
    case LED_STATE_BLINK: name = "blink"; break;
    default:              name = "?";     break;
    }
    printf("led: %s, period %u ms\n",
           name, (unsigned)led_task_get_period_ms());
}

static void cmd_led_enable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_ON);
    print_led_state();
}

static void cmd_led_disable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_OFF);
    print_led_state();
}

static void cmd_led_blink(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_BLINK);
    print_led_state();
}

static void cmd_led_period(const command_t *command)
{
    uint32_t period_ms;

    if (command->argc != 1 || !parse_u32(command->argv[0], &period_ms))
    {
        printf("error: usage led_period <period_ms>\n");
        return;
    }
    if (!led_task_set_period_ms(period_ms))
    {
        printf("error: period_ms must be greater than 0\n");
        return;
    }
    print_led_state();
}

// ─── button ─────────────────────────────────────────────────
static void cmd_button(const command_t *command)
{
    (void)command;
    printf("button: %s, presses %u\n",
           button_task_is_pressed() ? "pressed" : "released",
           (unsigned)button_task_get_press_count());
}

// ─── pi ─────────────────────────────────────────────────────
static void cmd_pi_start(const command_t *command)
{
    uint32_t terms = 1000000;

    if (command->argc > 1)
    {
        printf("error: usage pi_start [<terms>]\n");
        return;
    }
    if (command->argc == 1 && !parse_u32(command->argv[0], &terms))
    {
        printf("error: usage pi_start [<terms>]\n");
        return;
    }
    if (terms == 0)
    {
        printf("error: terms must be greater than 0\n");
        return;
    }
    if (!pi_task_start(terms))
    {
        printf("error: pi is busy\n");
        return;
    }
    printf("pi: started, %u terms\n", (unsigned)terms);
}

static void cmd_pi(const command_t *command)
{
    (void)command;

    switch (pi_task_get_state())
    {
    case PI_STATE_IDLE:    printf("pi: not started\n"); break;
    case PI_STATE_RUNNING: printf("pi: running\n");     break;
    case PI_STATE_DONE:
    {
        double   value;
        uint32_t elapsed_ms, terms;
        pi_task_get_result(&value, &elapsed_ms, &terms);
        printf("pi: %.9f in %u ms\n", value, (unsigned)elapsed_ms);
        break;
    }
    }
}

// ─── profiling ──────────────────────────────────────────────
static void cmd_profiling(const command_t *command)
{
    (void)command;

#if !PROFILING_ENABLED
    printf("profiling is disabled in this build\n");
    return;
#else
    for (uint32_t i = 0; i < PROFILING_STOPWATCH_COUNT; i++)
    {
        profiling_result_t r;
        if (!profiling_get(i, &r)) continue;

        printf("%-10s count %-10u mean %.2f us  max %u us\n",
               r.name, (unsigned)r.count,
               (double)r.mean_us, (unsigned)r.max_us);
    }
#endif
}

static void cmd_profiling_reset(const command_t *command)
{
    (void)command;
    profiling_reset();
    printf("profiling reset\n");
}

// print_edge(): ok == false — edge <pp|od>: timeout;
// иначе edge <pp|od>: rise <мс> ms, fall <мс> ms
static void print_edge(platform_line_mode_t mode, bool ok, uint32_t rise_us, uint32_t fall_us)
{
    const char *m = (mode == PLATFORM_LINE_PUSH_PULL) ? "pp" : "od";
    if (!ok) {
        printf("edge %s: timeout\n", m);
    } else {
        printf("edge %s: rise %.1f ms, fall %.1f ms\n", m, 
               (double)(rise_us / 1000.0f), (double)(fall_us / 1000.0f));
    }
}

// cmd_edge(): аргумент pp или od выбирает режим, иначе error: usage edge pp|od
// задача не приняла замер — error: edge is busy
static void cmd_edge(const command_t *command)
{
    if (command->argc != 1) {
        printf("error: usage edge pp|od\n");
        return;
    }

    platform_line_mode_t mode;
    if (strcmp(command->argv[0], "pp") == 0) {
        mode = PLATFORM_LINE_PUSH_PULL;
    } else if (strcmp(command->argv[0], "od") == 0) {
        mode = PLATFORM_LINE_OPEN_DRAIN;
    } else {
        printf("error: usage edge pp|od\n");
        return;
    }

    if (!edge_task_start(mode, print_edge)) {
        printf("error: edge is busy\n");
    }
}

// print_echo(): rx: <принятое> <признаки>, например rx: ping err: none
static void print_echo(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("rx: ");
    print_bytes(data, size);
    printf(" ");
    print_errors(errors);
    printf("\n");
}

// cmd_loop(): аргумент internal или pins, иначе error: usage loop internal|pins
// platform_uart_loop_set(), ответ loop: <аргумент>
static void cmd_loop(const command_t *command)
{
    if (command->argc != 1) {
        printf("error: usage loop internal|pins\n");
        return;
    }

    if (strcmp(command->argv[0], "internal") == 0) {
        platform_uart_loop_set(true);
    } else if (strcmp(command->argv[0], "pins") == 0) {
        platform_uart_loop_set(false);
    } else {
        printf("error: usage loop internal|pins\n");
        return;
    }
    printf("loop: %s\n", command->argv[0]);
}


// cmd_echo(): аргумент не один — error: usage echo <text>
// строка длиннее 32 символов — error: text longer than 32
// задача не приняла обмен — error: uart is busy
// иначе обмен UART0 → UART0 с функцией print_echo
static void cmd_echo(const command_t *command)
{
    if (command->argc != 1) {
        printf("error: usage echo <text>\n");
        return;
    }

    const char *text = command->argv[0];
    size_t len = strlen(text);

    if (len > 32) {
        printf("error: text longer than 32\n");
        return;
    }

    if (!uart_task_start(PLATFORM_UART0, PLATFORM_UART0, (const uint8_t *)text, len, print_echo)) {
        printf("error: uart is busy\n");
    }
}


// ─── таблица команд ─────────────────────────────────────────
const api_command_t api_commands[] = {
    { "info",            "device passport",                       cmd_info            },
    { "uptime",          "time since reset",                      cmd_uptime          },
    { "led_enable",      "turn led on",                           cmd_led_enable      },
    { "led_disable",     "turn led off",                          cmd_led_disable     },
    { "led_blink",       "make led blink",                        cmd_led_blink       },
    { "led_period",      "set blink period: led_period <ms>",     cmd_led_period      },
    { "button",          "button state and press count",          cmd_button          },
    { "pi_start",        "start pi calculation: pi_start [terms]",cmd_pi_start        },
    { "pi",              "pi result or state",                    cmd_pi              },
    { "profiling",       "stopwatch results",                     cmd_profiling       },
    { "profiling_reset", "reset stopwatches",                     cmd_profiling_reset },
    { "edge",               "rise and fall of the RC line: edge pp|od", cmd_edge },
    { "loop",               "UART0 loopback: loop internal|pins",       cmd_loop },
    { "echo",               "text through UART0 and back: echo <text>", cmd_echo },
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);