#include <stdio.h>
#include <stdint.h>
#include "hardware/regs/addressmap.h"
#include "pico/stdlib.h"

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