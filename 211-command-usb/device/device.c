#include "device.h"
#include "pico/version.h"
#include <stdio.h>
#include <stddef.h>
#include "pico/unique_id.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sysinfo.h"

// ─── определение переменной (память отводится здесь) ──────
struct info_t device_card = {
    .version  = 0x00010000,
    .name     = "es-cmd-usb",
    .revision = 2,
};


void device_info(void) {

    // читаем серийный номер платы функцией SDK
    char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    pico_get_unique_board_id_string(board_id, sizeof(board_id));
    // читаем регистр CHIP_ID по адресу и разбираем три поля
    volatile uint32_t *chip_id = (uint32_t *)(SYSINFO_BASE + SYSINFO_CHIP_ID_OFFSET);
    uint32_t id = *chip_id;
    uint32_t manufacturer = id & 0xFFFu;              // биты 11:0
    uint32_t part         = (id >> 16) & 0xFFFu;      // биты 27:16
    uint32_t revision     = (id >> 28) & 0xFu;        // биты 31:28
    // печатаем пять строк паспорта
    printf("project: %s\n", DEVICE_PROJECT);
    printf("repo: %s\n", DEVICE_REPO);
    printf("board: %s\n", DEVICE_BOARD);
    printf("serial: %s\n", board_id);
    printf("chip: manufacturer 0x%03x, part 0x%04x, revision %u\n", manufacturer, part, revision);
    printf("pico-sdk: %s\n", PICO_SDK_VERSION_STRING);
}

void dev_info(void)
{
    // ─── шапка ────────────────────────────────────────────
    printf("%-14s %-11s %6s %6s %s\n", "struct", "address", "size", "offset", "value");
    printf("%-14s 0x%08x %6u\n", "device_card",
           (unsigned)(uintptr_t)&device_card,
           (unsigned)sizeof(device_card));

    // ─── поле revision ─────────────────────────────────────
    printf("- %-12s 0x%08x %6u %6u %u\n", "revision",
           (unsigned)(uintptr_t)&device_card.revision,
           (unsigned)sizeof(device_card.revision),
           (unsigned)offsetof(struct info_t, revision),
           device_card.revision);

    // ─── поле version ──────────────────────────────────────
    printf("- %-12s 0x%08x %6u %6u 0x%08x\n", "version",
           (unsigned)(uintptr_t)&device_card.version,
           (unsigned)sizeof(device_card.version),
           (unsigned)offsetof(struct info_t, version),
           device_card.version);

    // ─── поле name: имя массива УЖЕ адрес, & не нужен ──────
    printf("- %-12s 0x%08x %6u %6u %s\n", "name",
           (unsigned)(uintptr_t)device_card.name,
           (unsigned)sizeof(device_card.name),
           (unsigned)offsetof(struct info_t, name),
           device_card.name);

    // ─── итог: сумма полей vs sizeof структуры ─────────────
    unsigned fields = sizeof(device_card.revision)
                    + sizeof(device_card.version)
                    + sizeof(device_card.name);
    unsigned padding = sizeof(device_card) - fields;

    printf("fields %u, sizeof %u, padding %u\n", fields,
           (unsigned)sizeof(device_card), padding);
}