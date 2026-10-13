#include "stdio-text-protocol/stdio-text-protocol.h"
#include <stdio.h>
#include "pico/stdlib.h"

static char    line[STDIO_TEXT_PROTOCOL_LINE_SIZE];
static uint32_t line_length = 0;
static bool     line_overflow = false;
static command_t command;

void stdio_text_protocol_init(void)
{
    line_length = 0;
    line_overflow = false;
}

// Делит строку на слова на месте: пробелы заменяет на '\0',
// в name и argv кладёт указатели на начала слов.
static void split_line(void)
{
    command.name = NULL;
    command.argc = 0;
    command.truncated = line_overflow;

    char *p = line;

    // пропускаем ведущие пробелы
    while (*p == ' ')
    {
        p++;
    }

    if (*p == '\0')
    {
        return;   // строка из одних пробелов — не команда
    }

    command.name = p;
    bool in_word = true;

    while (*p != '\0')
    {
        if (*p == ' ')
        {
            *p = '\0';
            in_word = false;
        }
        else if (!in_word)
        {
            in_word = true;
            if (command.argc < STDIO_TEXT_PROTOCOL_MAX_ARGS)
            {
                command.argv[command.argc] = p;
                command.argc++;
            }
            else
            {
                command.truncated = true;
            }
        }
        p++;
    }
}

const command_t *stdio_text_protocol_handle(void)
{
    int symbol = getchar_timeout_us(0);

    if (symbol == PICO_ERROR_TIMEOUT)
    {
        return NULL;
    }

    // ─── конец строки ──────────────────────────────────────
    if (symbol == '\r' || symbol == '\n')
    {
        putchar('\r');
        putchar('\n');

        if (line_length == 0)
        {
            line_overflow = false;
            return NULL;      // пустой кадр — не команда
        }

        line[line_length] = '\0';
        split_line();

        line_length = 0;
        line_overflow = false;

        if (command.name == NULL)
        {
            return NULL;      // только пробелы — не команда
        }

        return &command;
    }

    // ─── Backspace ─────────────────────────────────────────
    if (symbol == 0x08 || symbol == 0x7f)
    {
        if (line_length > 0)
        {
            line_length--;
            line_overflow = false;
            putchar(0x08);
            putchar(' ');
            putchar(0x08);
        }
        return NULL;
    }

    // ─── печатный символ ───────────────────────────────────
    if (symbol >= 0x20 && symbol <= 0x7e)
    {
        if (line_length + 1 < STDIO_TEXT_PROTOCOL_LINE_SIZE)
        {
            line[line_length] = (char)symbol;
            line_length++;
            putchar(symbol);
        }
        else
        {
            line_overflow = true;
        }
        return NULL;
    }

    return NULL;
}