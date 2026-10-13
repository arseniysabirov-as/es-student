#include "device.h"

#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "pico/version.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sysinfo.h"

void device_get_info(device_info_t *info)
{
    // серийный номер платы
    pico_get_unique_board_id_string(info->serial, sizeof(info->serial));

    // CHIP_ID: manufacturer — биты 11:0, part — 27:16, revision — 31:28
    volatile uint32_t *chip_id = (uint32_t *)(SYSINFO_BASE + SYSINFO_CHIP_ID_OFFSET);
    uint32_t id = *chip_id;
    info->manufacturer =  id         & 0xFFFu;
    info->part         = (id >> 16)  & 0xFFFu;
    info->revision     = (id >> 28)  & 0xFu;

    info->board       = DEVICE_BOARD;
    info->sdk_version = PICO_SDK_VERSION_STRING;
}
