#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "hardware/regs/addressmap.h"
#include "pico/stdlib.h"
#include "led.h"
#include "command.h"     // commands[], command_count
#include "device.h"      // DEVICE_NAME, FIRMWARE_VERSION, ...
#include "memory.h"

// Адрес таблицы векторов: сразу за 256-байтовым загрузчиком второй стадии
#define VECTOR_TABLE 0x10000100

// Адрес регистра GPIO_IN в блоке SIO
#define SIO_GPIO_IN 0xd0000004u

int main(void);
// Символы, которые расставляет компоновщик.
// Это адреса, поэтому берём их через &.
extern char __flash_binary_start;
extern char __flash_binary_end;
extern char __boot2_start__;
extern char __boot2_end__;
extern char __etext;
extern char __data_start__;
extern char __data_end__;
extern char __bss_start__;
extern char __bss_end__;
extern char __HeapLimit;
extern char __StackBottom;
extern char __StackTop;

uint32_t data_variable = 100;
uint32_t bss_variable;



static void row(const char *name, uintptr_t start, uintptr_t end)
{
    printf("%-10s 0x%08x 0x%08x %8u\n",
           name, (unsigned)start, (unsigned)end, (unsigned)(end - start));
}

void mem_info(void)
{
    // ─── шапка таблицы ─────────────────────────────────────
    printf("%-10s %10s %10s %8s\n", "region", "start", "end", "size");
    printf("----------------------------------------------\n");

    // ─── аппаратные области ────────────────────────────────
    row("flash", XIP_BASE, XIP_BASE + PICO_FLASH_SIZE_BYTES);
    row("sram",  SRAM_BASE, SRAM_BASE + 264 * 1024);
    row("rom",   ROM_BASE,  ROM_BASE  + 16  * 1024);
    printf("----------------------------------------------\n");

    // ─── разбор образа во флеш ─────────────────────────────
    row("image", (uintptr_t)&__flash_binary_start, (uintptr_t)&__flash_binary_end);
    row("free",  (uintptr_t)&__flash_binary_end,   XIP_BASE + PICO_FLASH_SIZE_BYTES);
    row("boot2", (uintptr_t)&__boot2_start__,      (uintptr_t)&__boot2_end__);
    row("text",  (uintptr_t)&__boot2_end__,        (uintptr_t)&__etext);
    printf("----------------------------------------------\n");

    // ─── .data: копия во флеш и рабочая копия в ОЗУ ────────
    uintptr_t data_size = (uintptr_t)&__data_end__ - (uintptr_t)&__data_start__;
    row("data flash", (uintptr_t)&__etext,        (uintptr_t)&__etext + data_size);
    row("data ram",   (uintptr_t)&__data_start__, (uintptr_t)&__data_end__);

    // ─── .bss, куча и стек ─────────────────────────────────
    row("bss",   (uintptr_t)&__bss_start__, (uintptr_t)&__bss_end__);
    row("heap",  (uintptr_t)&__bss_end__,   (uintptr_t)&__HeapLimit);
    row("stack", (uintptr_t)&__StackBottom, (uintptr_t)&__StackTop);
    printf("----------------------------------------------\n");

    // ─── размеры секций (считаем один раз) ─────────────────
    uintptr_t boot2_size = (uintptr_t)&__boot2_end__ - (uintptr_t)&__boot2_start__;
    uintptr_t text_size  = (uintptr_t)&__etext       - (uintptr_t)&__boot2_end__;
    uintptr_t bss_size   = (uintptr_t)&__bss_end__   - (uintptr_t)&__bss_start__;

    // data_size объявлена выше, повторно не объявляем
    uintptr_t image_size = (uintptr_t)&__flash_binary_end - (uintptr_t)&__flash_binary_start;
    uintptr_t flash_free = PICO_FLASH_SIZE_BYTES - image_size;
    uintptr_t ram_used   = data_size + bss_size;
    uintptr_t heap_size  = (uintptr_t)&__HeapLimit  - (uintptr_t)&__bss_end__;
    uintptr_t stack_size = (uintptr_t)&__StackTop   - (uintptr_t)&__StackBottom;

    // ─── итоги в требуемом формате ─────────────────────────
    printf("total\n");
    printf("  %-13s%7u = boot2 %u + text %u + data %u\n",
           "flash image", (unsigned)image_size,
           (unsigned)boot2_size, (unsigned)text_size, (unsigned)data_size);
    printf("  %-13s%7u of %u\n",
           "flash free",  (unsigned)flash_free,
           (unsigned)PICO_FLASH_SIZE_BYTES);
    printf("  %-13s%7u = data %u + bss %u\n",
           "ram used",    (unsigned)ram_used,
           (unsigned)data_size, (unsigned)bss_size);
    printf("  %-13s%7u for heap and %u for stack\n",
           "ram free",    (unsigned)heap_size,
           (unsigned)stack_size);
}

// ═════════════════════════════════════════════════════════
//  fw_info — карта прошивки: адреса и значения
// ═════════════════════════════════════════════════════════
void fw_info(void)
{
    // Считаем вызов: data_variable и bss_variable на единицу больше
    data_variable++;
    bss_variable++;
    // Адреса функций со сброшенным признаком Thumb
    uint16_t *main_code = (uint16_t *)((uintptr_t)main & ~1u);
    uint16_t *fw_code   = (uint16_t *)((uintptr_t)fw_info & ~1u);

    // Локальная переменная (стек) и блок из кучи
    uint32_t  stack_variable = 1946;
    uint32_t *heap_variable  = malloc(sizeof(uint32_t));
    if (heap_variable != NULL)
    {
        *heap_variable = 1951;
    }

    // ─── шапка ────────────────────────────────────────────
    printf("%-18s %-12s %s\n", "object", "address", "value");
    printf("--------------------------------------------------\n");

    // ─── функции ──────────────────────────────────────────
    printf("%-18s 0x%08x   0x%04x\n",
           "main",    (unsigned)(uintptr_t)main,    *main_code);
    printf("%-18s 0x%08x   0x%04x\n",
           "fw_info", (unsigned)(uintptr_t)fw_info, *fw_code);

    // ─── таблица команд ───────────────────────────────────
    printf("%-18s 0x%08x\n",
           "commands", (unsigned)(uintptr_t)commands);
    printf("%-18s 0x%08x   %u\n",
           "command_count", (unsigned)(uintptr_t)&command_count, command_count);
    for (uint i = 0; i < command_count; i++)
    {
        printf("  %-16s 0x%08x\n",
               commands[i].name,
               (unsigned)(uintptr_t)commands[i].handler);
    }

    // ─── константы паспорта (строки — их имена уже адреса) ─
    printf("%-18s 0x%08x   %s\n",
           "DEVICE_PROJECT", (unsigned)(uintptr_t)DEVICE_PROJECT, DEVICE_PROJECT);
    printf("%-18s 0x%08x   %s\n",
           "DEVICE_REPO",    (unsigned)(uintptr_t)DEVICE_REPO,    DEVICE_REPO);
    printf("%-18s 0x%08x   %s\n",
           "DEVICE_BOARD",   (unsigned)(uintptr_t)DEVICE_BOARD,   DEVICE_BOARD);

    // ─── переменные в .data и .bss ─────────────────────────
    printf("%-18s 0x%08x   %u\n",
           "data_variable", (unsigned)(uintptr_t)&data_variable, data_variable);
    printf("%-18s 0x%08x   %u\n",
           "bss_variable",  (unsigned)(uintptr_t)&bss_variable,  bss_variable);

    // ─── стек и куча ──────────────────────────────────────
    printf("%-18s 0x%08x   %u\n",
           "stack_variable", (unsigned)(uintptr_t)&stack_variable, stack_variable);
    if (heap_variable != NULL)
    {
        // Внимание: печатаем сам указатель (адрес блока), а не &heap_variable
        printf("%-18s 0x%08x   %u\n",
               "heap_variable", (unsigned)(uintptr_t)heap_variable, *heap_variable);
    }

    // Взяли у кучи — верните
    free(heap_variable);
}


void boot_info(void)
{
    // ─── таблица векторов ──────────────────────────────────
    const uint32_t *vectors = (const uint32_t *)VECTOR_TABLE;

    uint32_t stack_top     = vectors[0];   // начальное значение SP
    uint32_t reset_handler = vectors[1];   // точка входа прошивки

    // ─── регистр GPIO_IN ───────────────────────────────────
    volatile uint32_t *gpio_in = (volatile uint32_t *)SIO_GPIO_IN;
    uint pin = led_pin();

    // Сдвигаем нужный разряд к младшему и отрезаем маску
    uint32_t level = (*gpio_in >> pin) & 1u;

    // То же самое, но через SDK
    bool level_sdk = gpio_get(pin);

    // ─── шапка ─────────────────────────────────────────────
    printf("%-16s %-11s %s\n", "object", "address", "value");
    printf("--------------------------------------------------\n");

    // ─── vector table ─────────────────────────────────────
    printf("%-16s 0x%08x\n",
           "vector table", (unsigned)VECTOR_TABLE);
    printf("  %-14s             0x%08x\n",
           "stack top", (unsigned)stack_top);
    printf("  %-14s             0x%08x\n",
           "reset", (unsigned)reset_handler);
    printf("  %-14s             0x%08x\n",
           "reset (even)", (unsigned)(reset_handler & ~1u));

    // ─── GPIO_IN ──────────────────────────────────────────
    printf("%-16s 0x%08x\n",
           "gpio in", (unsigned)SIO_GPIO_IN);
    printf("  %-14s             0x%08x\n",
           "led bit", (unsigned)level);
    printf("  %-14s             0x%08x\n",
           "gpio_get", (unsigned)level_sdk);
}