#include "device.h"
#include "pico/version.h"
#include <stdio.h>
#include "pico/unique_id.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sysinfo.h"

    
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