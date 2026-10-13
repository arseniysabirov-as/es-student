#include "api.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "led-task/led-task.h"
#include "pico/stdlib.h"
#include "device/device.h"
#include "profiling/profiling.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "device/device.h"
#include "profiling/profiling.h"

typedef void (*api_callback_t)(const command_t *command);

typedef struct
{
    const char    *name;
    const char    *help;
    api_callback_t callback;
} api_command_t;

// ─── разбор числа ──────────────────────────────────────────
// Число без знака из десятичной записи. false — в записи не только цифры
// или число не помещается в uint32_t.
static bool parse_u32(const char *text, uint32_t *value)
{
    // strtoul приняла бы и знак: из "-1" получилось бы 4294967295
    if (text == NULL || !isdigit((unsigned char)text[0]))
    {
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long number = strtoul(text, &end, 10);

    // после числа остались символы или число не поместилось
    if (*end != '\0' || errno == ERANGE || number > UINT32_MAX)
    {
        return false;
    }

    *value = (uint32_t)number;
    return true;
}

// ─── обработчики команд ────────────────────────────────────

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

static void cmd_button(const command_t *command)
{
    (void)command;

    printf("button: %s, presses %u\n",
           button_task_is_pressed() ? "pressed" : "released",
           (unsigned)button_task_get_press_count());
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

static void cmd_uptime(const command_t *command)
{
    (void)command;
    printf("uptime: %llu ms\n", (unsigned long long)(time_us_64() / 1000));
}

static void cmd_calc_pi(const command_t *command)
{
    uint32_t terms = 1000000;

    if (command->argc > 1)
    {
        printf("error: usage calc_pi [terms]\n");
        return;
    }

    if (command->argc == 1)
    {
        if (!parse_u32(command->argv[0], &terms))
        {
            printf("error: usage calc_pi [terms]\n");
            return;
        }
    }

    if (terms == 0)
    {
        printf("error: terms must be greater than 0\n");
        return;
    }

    uint64_t start = time_us_64();

    double pi = 0.0;
    double sign = 1.0;
    for (uint32_t i = 0; i < terms; i++)
    {
        pi += sign / (2.0 * i + 1.0);
        sign = -sign;
    }
    pi *= 4.0;

    uint64_t elapsed_ms = (time_us_64() - start) / 1000;

    printf("pi: %.9f\n", pi);
    printf("time: %llu ms\n", (unsigned long long)elapsed_ms);
}

static void cmd_pi_start(const command_t *command)
{
    uint32_t terms = 1000000;   // по умолчанию

    if (command->argc > 1)
    {
        printf("error: usage pi_start [<terms>]\n");
        return;
    }

    if (command->argc == 1)
    {
        if (!parse_u32(command->argv[0], &terms))
        {
            printf("error: usage pi_start [<terms>]\n");
            return;
        }
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
    case PI_STATE_IDLE:
        printf("pi: not started\n");
        break;

    case PI_STATE_RUNNING:
        printf("pi: running\n");
        break;

    case PI_STATE_DONE:
    {
        double   value;
        uint32_t elapsed_ms;
        uint32_t terms;
        pi_task_get_result(&value, &elapsed_ms, &terms);
        printf("pi: %.9f in %u ms\n", value, (unsigned)elapsed_ms);
        break;
    }
    }
}

static void cmd_info(const command_t *command)
{
    (void)command;

    device_info_t info;
    device_get_info(&info);

    printf("name: %s\n",     DEVICE_NAME);
    printf("version: %s\n",  FIRMWARE_VERSION);
    printf("project: %s\n",  DEVICE_PROJECT);
    printf("repo: %s\n",     DEVICE_REPO);
    printf("board: %s\n",    info.board);
    printf("serial: %s\n",   info.serial);
    printf("chip: manufacturer 0x%03x, part 0x%04x, revision %u\n",
           (unsigned)info.manufacturer,
           (unsigned)info.part,
           (unsigned)info.revision);
    printf("pico-sdk: %s\n", info.sdk_version);
}

static void cmd_profiling(const command_t *command)
{
    (void)command;

    for (uint32_t i = 0; i < PROFILING_STOPWATCH_COUNT; i++)
    {
        profiling_result_t r;
        if (!profiling_get(i, &r))
        {
            continue;
        }

        printf("%-10s count %-10u mean %.2f us  max %u us\n",
               r.name,
               (unsigned)r.count,
               (double)r.mean_us,
               (unsigned)r.max_us);
    }
}

static void cmd_profiling_reset(const command_t *command)
{
    (void)command;
    profiling_reset();
    printf("profiling reset\n");
}

// ─── таблица команд ────────────────────────────────────────
static const api_command_t commands[] = {
    { "info",           "device passport",                             cmd_info        },
    { "uptime",         "time since reset",                            cmd_uptime      },
    { "calc_pi",        "calculate pi: calc_pi [terms]",               cmd_calc_pi     },
    { "led_enable",     "turn led on",                                 cmd_led_enable  },
    { "led_disable",    "turn led off",                                cmd_led_disable },
    { "led_blink",      "make led blink",                              cmd_led_blink   },
    { "led_period",     "set blink period: led_period <ms>",           cmd_led_period  },
    { "button",         "button state and press count",                cmd_button      },
    { "pi_start",       "start pi calculation: pi_start [terms]",      cmd_pi_start    },
    { "pi",             "pi result or state",                          cmd_pi          },
    { "profiling",      "stopwatch results",                           cmd_profiling       },
    { "profiling_reset","reset stopwatches",                           cmd_profiling_reset },
};

#define COMMANDS_COUNT (sizeof(commands) / sizeof(commands[0]))

// ─── help ──────────────────────────────────────────────────
static void print_help(void)
{
    printf("%-16s %s\n", "help", "list of commands");

    for (uint32_t i = 0; i < COMMANDS_COUNT; i++)
    {
        printf("%-16s %s\n", commands[i].name, commands[i].help);
    }
}

// ─── точка входа ───────────────────────────────────────────
void api_handle(const command_t *command)
{
    if (command->truncated)
    {
        printf("error: command longer than 63 characters or 4 arguments\n");
        return;
    }

    if (strcmp(command->name, "help") == 0)
    {
        print_help();
        return;
    }

    for (uint32_t i = 0; i < COMMANDS_COUNT; i++)
    {
        if (strcmp(command->name, commands[i].name) == 0)
        {
            commands[i].callback(command);
            return;
        }
    }

    printf("error: unknown command '%s', try help\n", command->name);
}

