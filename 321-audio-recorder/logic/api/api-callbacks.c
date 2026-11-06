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
#include "platform/platform.h"
#include "led-task/led-task.h"
#include "block-task/block-task.h"

// период телеметрии по умолчанию
#define TELEMETRY_DEFAULT_PERIOD_MS 100

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

// напряжение на GP26 у platform, ответ voltage: <напряжение> V
static void cmd_get_voltage(const command_t *command)
{
     if (platform_adc_busy()) {
        printf("error: adc busy\n");
        return;
    }
    (void)command;
    float voltage = platform_voltage_read();
    printf("voltage: %.3f V\n", (double)voltage);
}

// температура у platform, ответ temp: <температура> C
static void cmd_get_temp(const command_t *command)
{
     if (platform_adc_busy()) {
        printf("error: adc busy\n");
        return;
    }
    (void)command;
    float temp = platform_temp_read();
    printf("temp: %.1f C\n", (double)temp);
}
void api_print_telemetry(const telemetry_record_t *record)
{
    printf("tm: %llu %.3f %.1f\n",
           (unsigned long long)record->time_us,
           (double)record->voltage_v,
           (double)record->temp_c);
}

static void cmd_tm_start(const command_t *command)
{
    uint32_t period = TELEMETRY_DEFAULT_PERIOD_MS; 

     if (platform_adc_busy()) {
        printf("error: adc busy\n");
        return;
    }

    if (command->argc > 1)
    {
        printf("error: usage tm_start [<period_ms>]\n");
        return;
    }

    if (command->argc == 1)
    {
        if (!parse_u32(command->argv[0], &period))
        {
            printf("error: usage tm_start [<period_ms>]\n");
            return;
        }
    }

    if (!telemetry_task_start(period))
    {
        printf("error: period_ms must be at least 10\n");
        return;
    }

    printf("telemetry: started, period %u ms\n", (unsigned)period);
}

static void cmd_tm_stop(const command_t *command)
{
    (void)command;
    telemetry_task_stop();
    printf("telemetry: stopped\n");
}

static void cmd_charge_on(const command_t *command)
{
    (void)command;
    platform_charge_set(true);
    printf("charge: on\n");
}

static void cmd_charge_off(const command_t *command)
{
    (void)command;
    platform_charge_set(false);
    printf("charge: off\n");
}

static void cmd_timer_blink_start(const command_t *command)
{
    uint32_t period_ms;

    if (command->argc != 1 || !parse_u32(command->argv[0], &period_ms))
    {
        printf("error: usage timer_blink_start <period_ms>\n");
        return;
    }

    if (!platform_blink_start(period_ms))
    {
        printf("error: period_ms must be 10..60000\n");
        return;
    }

    // Чтобы задача светодиода не воевала с будильником за один пин,
    // переводим её в состояние "выключен"
    led_task_set_state(LED_STATE_OFF);

    printf("timer_blink: started, period %u ms\n", (unsigned)period_ms);
}

static void cmd_timer_blink_stop(const command_t *command)
{
    (void)command;
    uint32_t toggles = platform_blink_stop();
    printf("timer_blink: stopped, %u toggles\n", (unsigned)toggles);
}

// cmd_cap_start(): аргументов не два или не числа — error: usage cap_start <N> <period_us>
// N = 0, N > PLATFORM_CAPTURE_MAX или период меньше 10 мкс —
//     error: N must be 1..2000, period_us at least 10
// идёт телеметрия или platform не начал захват — error: adc busy
// ответ capture: started, <N> samples, period <период> us
static void cmd_cap_start(const command_t *command)
{
    uint32_t count, period_us;

    if (command->argc != 2) {
        printf("error: usage cap_start <N> <period_us>\n");
        return;
    }

    if (!parse_u32(command->argv[0], &count) || !parse_u32(command->argv[1], &period_us)) {
        printf("error: usage cap_start <N> <period_us>\n");
        return;
    }

    if (count == 0 || count > PLATFORM_CAPTURE_MAX || period_us < 10) {
        printf("error: N must be 1..2000, period_us at least 10\n");
        return;
    }

    // Проверка на телеметрию (если она использует АЦП, мы не должны начинать)
    if (platform_adc_busy() || telemetry_task_get_state() == TELEMETRY_STATE_RUN) {
        printf("error: adc busy\n");
        return;
    }

    if (!platform_capture_start(count, period_us)) {
        printf("error: adc busy\n");
        return;
    }

    printf("capture: started, %u samples, period %u us\n", (unsigned)count, (unsigned)period_us);
}

// cmd_cap_dump(): захвата не было — error: no capture
// строка capture: <N> samples, period <период> us, interval <мин>..<макс> us, handler <доля> %
// коды строками cap: по 16
static void cmd_cap_dump(const command_t *command)
{
    (void)command;

    if (!platform_capture_done()) {
        printf("error: no capture\n");
        return;
    }

    platform_capture_t cap;
    platform_capture_get(&cap);

    printf("capture: %u samples, period %u us, interval %u..%u us, handler %.1f %%\n",
           (unsigned)cap.count, (unsigned)cap.period_us,
           (unsigned)cap.min_interval_us, (unsigned)cap.max_interval_us,
           (double)cap.handler_percent);

    for (uint32_t i = 0; i < cap.count; i++) {
        if (i % 16 == 0) {
            if (i > 0) printf("\n");
            printf("cap:");
        }
        printf(" %u", (unsigned)cap.samples[i]);
    }
    printf("\n");
}

// print_dma_cap(): наименьший, наибольший и средний код блока;
// расчёт времени — count * 1000000 / PLATFORM_SAMPLE_RATE_HZ, в uint64_t;
// строка dma_cap: <N> samples in <мкс> us, expected <мкс> us, min <код> max <код> mean <код, 1 знак>
static void print_dma_cap(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    uint32_t min_code = 4095, max_code = 0;
    uint64_t sum = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (samples[i] < min_code) min_code = samples[i];
        if (samples[i] > max_code) max_code = samples[i];
        sum += samples[i];
    }

    float mean = count > 0 ? (float)sum / count : 0.0f;
    uint32_t expected_us = (uint32_t)((uint64_t)count * 1000000ULL / PLATFORM_SAMPLE_RATE_HZ);

    printf("dma_cap: %u samples in %u us, expected %u us, min %u max %u mean %.1f\n",
           (unsigned)count, (unsigned)time_us, (unsigned)expected_us,
           (unsigned)min_code, (unsigned)max_code, (double)mean);
}

// cmd_dma_cap(): аргумент не один или не число — error: usage dma_cap <N>
// N = 0 или больше PLATFORM_BLOCK_MAX — error: N must be 1..8192
// идёт телеметрия или задача не приняла захват — error: adc busy
// иначе захват с функцией print_dma_cap, ответ придёт, когда блок будет готов
static void cmd_dma_cap(const command_t *command)
{
    uint32_t count;

    if (command->argc != 1 || !parse_u32(command->argv[0], &count)) {
        printf("error: usage dma_cap <N>\n");
        return;
    }

    if (count == 0 || count > PLATFORM_BLOCK_MAX) {
        printf("error: N must be 1..8192\n");
        return;
    }

    if (platform_adc_busy() || telemetry_task_get_state() == TELEMETRY_STATE_RUN) {
        printf("error: adc busy\n");
        return;
    }

    if (!block_task_start(count, print_dma_cap)) {
        printf("error: adc busy\n");
        return;
    }
    // Ответ придёт, когда блок будет готов
}

// ─── таблица команд ─────────────────────────────────────────
const api_command_t api_commands[] = {
    { "info",            "device passport",                       cmd_info              },
    { "uptime",          "time since reset",                      cmd_uptime            },
    { "led_enable",      "turn led on",                           cmd_led_enable        },
    { "led_disable",     "turn led off",                          cmd_led_disable       },
    { "led_blink",       "make led blink",                        cmd_led_blink         },
    { "led_period",      "set blink period: led_period <ms>",     cmd_led_period        },
    { "button",          "button state and press count",          cmd_button            },
    { "pi_start",        "start pi calculation: pi_start [terms]",cmd_pi_start          },
    { "pi",              "pi result or state",                    cmd_pi                },
    { "profiling",       "stopwatch results",                     cmd_profiling         },
    { "profiling_reset", "reset stopwatches",                     cmd_profiling_reset   },
    { "get_voltage",     "voltage on GP26, ADC input 0",          cmd_get_voltage       },
    { "get_temp",        "chip temperature, degrees Celsius",     cmd_get_temp          },
     { "tm_start", "start telemetry: tm_start [period_ms]",       cmd_tm_start          },
    { "tm_stop",  "stop telemetry",                               cmd_tm_stop           },
    { "charge_on",  "turn on RC charge",                            cmd_charge_on  },
    { "charge_off", "turn off RC charge",                           cmd_charge_off },
    { "timer_blink_start", "blink led by timer: timer_blink_start <period_ms>", cmd_timer_blink_start },
    { "timer_blink_stop", "stop timer blink, number of toggles", cmd_timer_blink_stop },
    { "cap_start", "capture ADC via timer: cap_start <N> <period_us>", cmd_cap_start },
    { "cap_dump",  "dump capture results",                             cmd_cap_dump  },
    { "dma_cap", "capture block via DMA: dma_cap <N>", cmd_dma_cap },
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);