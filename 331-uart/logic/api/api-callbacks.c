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
#include "block-task/block-task.h"
#include <stdlib.h> // для strtol
#include <ctype.h> // для isprint()
#include <errno.h>
#include <string.h>

#define SCOPE_SAMPLES PLATFORM_BLOCK_MAX
#define SCOPE_BAUD_MIN 9600
#define SCOPE_TEXT_MAX 4

static uint8_t  last_uart_rx[32];
static size_t   last_uart_rx_size = 0;
static uint32_t last_uart_rx_errors = 0;

#define BLOCK_SIZE 64

static uint32_t scope_baud = 115200;

static const char text_line[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";

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

static void print_scope_rx(const uint8_t *data, size_t size, uint32_t errors)
{
    // Сохраняем данные от UART, чтобы потом напечатать их вместе с осциллограммой
    last_uart_rx_size = (size > sizeof(last_uart_rx)) ? sizeof(last_uart_rx) : size;
    memcpy(last_uart_rx, data, last_uart_rx_size);
    last_uart_rx_errors = errors;
}

static void print_scope(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    (void)time_us;
    // Возвращаем UART на стандартную скорость
    platform_uart_baud_set(PLATFORM_UART0, 115200);
    platform_uart_baud_set(PLATFORM_UART1, 115200);

    // Печатаем заголовок
    printf("scope: %u samples, %u Hz, %u baud, uart1 ", 
           (unsigned)count, (unsigned)PLATFORM_SAMPLE_RATE_HZ, (unsigned)scope_baud);
    print_bytes(last_uart_rx, last_uart_rx_size);
    printf(" ");
    print_errors(last_uart_rx_errors);
    printf("\n");

    // Печатаем отсчёты по 16 в строке
    for (uint32_t i = 0; i < count; i++) {
        if (i % 16 == 0) {
            if (i > 0) printf("\n");
            // Опционально: можно выводить префикс, но задание просит просто числа
        }
        printf("%u", (unsigned)samples[i]);
        if (i % 16 != 15 && i != count - 1) printf(" ");
    }
    printf("\nscope: end\n");
}

// print_block(): прерывание приёма запретить;
// match — байты есть и совпадают с первыми байтами блока (memcmp);
// block <irq|poll>: <принято>/64 <match|mismatch>, <признаки>
static void print_block(const uint8_t *data, size_t size, uint32_t errors)
{
    // Сразу выключаем прерывание, чтобы буфер не менялся, пока мы его читаем
    platform_uart_rx_irq_set(false);

    // Проверяем, совпадает ли принятое с началом блока (0, 1, 2...)
    bool match = false;
    if (size > 0) {
        uint8_t expected[BLOCK_SIZE];
        for (int i = 0; i < BLOCK_SIZE; i++) expected[i] = i;
        match = (memcmp(data, expected, size) == 0);
    }

    printf("block %s: %u/%u %s, ", 
           (size == 64) ? "irq" : "poll",
           (unsigned)size, (unsigned)BLOCK_SIZE,
           match ? "match" : "mismatch");
    print_errors(errors);
    printf("\n");
}

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

//парсер целых чисел со знаком (для baud_shift)
static bool parse_int(const char *text, int32_t *value)
{
    if (!text || !*text) return false;
    char *end;
    errno = 0;
    long val = strtol(text, &end, 10);
    if (*end != '\0' || errno == ERANGE || val < -32768 || val > 32767) return false;
    *value = (int32_t)val;
    return true;
}

// print_link_back(): 1->0: <принятое> <признаки>
static void print_link_back(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("1->0: ");
    print_bytes(data, size);
    printf(" ");
    print_errors(errors);
    printf("\n");
}

// print_link_forth(): 0->1: <принятое> <признаки>, затем обмен UART1 → UART0
// с принятыми байтами и функцией print_link_back
static void print_link_forth(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("0->1: ");
    print_bytes(data, size);
    printf(" ");
    print_errors(errors);
    printf("\n");

    // Запускаем обратный обмен
    uart_task_start(PLATFORM_UART1, PLATFORM_UART0, data, size, print_link_back);
}


// print_text(): text: <число байтов> chars <принятое> <признаки>
static void print_text(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("text: %u chars ", (unsigned)size);
    print_bytes(data, size);
    printf(" ");
    print_errors(errors);
    printf("\n");
}

static void cmd_scope(const command_t *command)
{
    if (command->argc != 2) {
        printf("error: usage scope <baud> <text>\n");
        return;
    }

    uint32_t baud;
    if (!parse_u32(command->argv[0], &baud) || baud < SCOPE_BAUD_MIN || baud > 115200) {
        printf("error: usage scope <baud> <text>\n");
        return;
    }

    const char *text = command->argv[1];
    size_t len = strlen(text);
    if (len == 0 || len > SCOPE_TEXT_MAX) {
        printf("error: usage scope <baud> <text>\n");
        return;
    }

    // 1. Меняем скорость обоим модулям
    platform_uart_baud_set(PLATFORM_UART0, baud);
    platform_uart_baud_set(PLATFORM_UART1, baud);
    scope_baud = baud;

    // 2. Запускаем захват (сначала запускаем захват, чтобы поймать линию до старта)
    if (!block_task_start(SCOPE_SAMPLES, print_scope)) {
        platform_uart_baud_set(PLATFORM_UART0, 115200);
        platform_uart_baud_set(PLATFORM_UART1, 115200);
        printf("error: adc busy\n");
        return;
    }

    // 3. Запускаем передачу UART
    if (!uart_task_start(PLATFORM_UART0, PLATFORM_UART1, (const uint8_t *)text, len, print_scope_rx)) {
        // Если UART занят (маловероятно), сбрасываем всё
        printf("error: uart is busy\n");
    }
}

// cmd_block(): аргумент irq или poll, иначе error: usage block irq|poll
// заполнить блок номерами, прерывание приёма по аргументу,
// обмен UART1 → UART0 с функцией print_block;
// задача не приняла обмен — запретить прерывание, error: uart is busy
static void cmd_block(const command_t *command)
{
    if (command->argc != 1) {
        printf("error: usage block irq|poll\n");
        return;
    }

    bool use_irq;
    if (strcmp(command->argv[0], "irq") == 0) {
        use_irq = true;
    } else if (strcmp(command->argv[0], "poll") == 0) {
        use_irq = false;
    } else {
        printf("error: usage block irq|poll\n");
        return;
    }

    // Формируем блок 0, 1, 2, ..., 63
    uint8_t block_data[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++) {
        block_data[i] = (uint8_t)i;
    }

    // Включаем или выключаем прерывание приёма
    platform_uart_rx_irq_set(use_irq);

    // Запускаем передачу UART1 -> UART0
    if (!uart_task_start(PLATFORM_UART1, PLATFORM_UART0, block_data, BLOCK_SIZE, print_block)) {
        platform_uart_rx_irq_set(false);
        printf("error: uart is busy\n");
    }
}

// cmd_baud_shift(): аргумент не одно целое число — error: usage baud_shift <percent>
// процент вне −50..50 — error: percent must be -50..50
// скорость UART1 = 115200 * (100 + процент) / 100,
// ответ baud_shift: uart1 <скорость из platform_uart_baud_set()> baud
static void cmd_baud_shift(const command_t *command)
{
    int32_t percent;
    if (command->argc != 1 || !parse_int(command->argv[0], &percent)) {
        printf("error: usage baud_shift <percent>\n");
        return;
    }
    if (percent < -50 || percent > 50) {
        printf("error: percent must be -50..50\n");
        return;
    }

    uint32_t new_baud = (uint32_t)(115200 * (100 + percent) / 100);
    uint32_t actual = platform_uart_baud_set(PLATFORM_UART1, new_baud);
    printf("baud_shift: uart1 %u baud\n", (unsigned)actual);
}

// cmd_parity(): два аргумента из none, even, odd, иначе
// error: usage parity none|even|odd none|even|odd
// первый — UART0, приёмник text, второй — UART1, передатчик text,
// ответ parity: uart0 <первый>, uart1 <второй>
static void cmd_parity(const command_t *command)
{
    if (command->argc != 2) {
        printf("error: usage parity none|even|odd none|even|odd\n");
        return;
    }

    platform_parity_t p0, p1;
    if (strcmp(command->argv[0], "none") == 0) p0 = PLATFORM_PARITY_NONE;
    else if (strcmp(command->argv[0], "even") == 0) p0 = PLATFORM_PARITY_EVEN;
    else if (strcmp(command->argv[0], "odd") == 0) p0 = PLATFORM_PARITY_ODD;
    else { printf("error: usage parity none|even|odd none|even|odd\n"); return; }

    if (strcmp(command->argv[1], "none") == 0) p1 = PLATFORM_PARITY_NONE;
    else if (strcmp(command->argv[1], "even") == 0) p1 = PLATFORM_PARITY_EVEN;
    else if (strcmp(command->argv[1], "odd") == 0) p1 = PLATFORM_PARITY_ODD;
    else { printf("error: usage parity none|even|odd none|even|odd\n"); return; }

    platform_uart_parity_set(PLATFORM_UART0, p0);
    platform_uart_parity_set(PLATFORM_UART1, p1);
    printf("parity: uart0 %s, uart1 %s\n", command->argv[0], command->argv[1]);
}

// cmd_text(): обмен UART1 → UART0 со строкой text_line и функцией print_text
static void cmd_text(const command_t *command)
{
    (void)command;
    // text_line имеет длину 32, поэтому передаем 32
    if (!uart_task_start(PLATFORM_UART1, PLATFORM_UART0, (const uint8_t *)text_line, 32, print_text)) {
        printf("error: uart is busy\n");
    }
}

// cmd_link(): проверки аргумента как у echo, обмен UART0 → UART1 с print_link_forth
static void cmd_link(const command_t *command)
{
    if (command->argc != 1) {
        printf("error: usage link <text>\n");
        return;
    }
    const char *text = command->argv[0];
    size_t len = strlen(text);
    if (len > 32) {
        printf("error: text longer than 32\n");
        return;
    }
    if (!uart_task_start(PLATFORM_UART0, PLATFORM_UART1, (const uint8_t *)text, len, print_link_forth)) {
        printf("error: uart is busy\n");
    }
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
    { "edge",               "rise and fall of the RC line: edge pp|od",         cmd_edge        },
    { "loop",               "UART0 loopback: loop internal|pins",               cmd_loop        },
    { "echo",               "text through UART0 and back: echo <text>",         cmd_echo        },
    { "link",               "text from UART0 to UART1 and back: link <text>",   cmd_link        },
    { "baud_shift",         "shift UART1 baud rate: baud_shift <percent>",      cmd_baud_shift  },
    { "text",               "32 characters from UART1 to UART0",                cmd_text        },
    { "parity",             "parity of UART0 and UART1: parity <rx> <tx>",      cmd_parity      },
    { "block",              "64 bytes from UART1 to UART0: block irq|poll",     cmd_block       },
    { "scope",              "UART0 line on ADC: scope <baud> <text>",           cmd_scope       },
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);