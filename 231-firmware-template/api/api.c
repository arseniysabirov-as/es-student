#include "api.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pico/stdlib.h"
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

static void cmd_info(const command_t *command)
{
    (void)command;
    device_info();
}

static void cmd_uptime(const command_t *command)
{
    (void)command;
    printf("uptime: %llu ms\n",
           (unsigned long long)(time_us_64() / 1000));
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

static void cmd_main_time_exec(const command_t *command)
{
    (void)command;
    printf("iteration avg %.2f us, max %u us\n",
           profiling_avg_us(),
           (unsigned)profiling_max_us());
}

static void cmd_main_time_reset(const command_t *command)
{
    (void)command;
    profiling_reset_max();
    printf("max reset\n");
}

// ─── таблица команд ────────────────────────────────────────
static const api_command_t commands[] = {
    { "info",            "device passport",                          cmd_info            },
    { "uptime",          "time since reset",                         cmd_uptime          },
    { "calc_pi",         "calculate pi: calc_pi [terms]",            cmd_calc_pi         },
    { "main_time_exec",  "superloop iteration: average and maximum", cmd_main_time_exec  },
    { "main_time_reset", "reset maximum iteration",                  cmd_main_time_reset },
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