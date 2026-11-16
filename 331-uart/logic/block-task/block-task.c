
#include "block-task/block-task.h"
#include "platform/platform.h"


static block_task_callback_t callback = 0;
static uint32_t current_count = 0;

bool block_task_start(uint32_t count, block_task_callback_t on_block)
{
    if (callback != 0) return false;
    if (!platform_block_start(count)) return false;

    callback = on_block;
    current_count = count;
    return true;
}

void block_task_handle(void)
{
    if (callback == 0 || !platform_block_done()) {
        return;
    }

    block_task_callback_t cb = callback;
    uint32_t cnt = current_count;
    callback = 0;

    cb(platform_block_samples(), cnt, platform_block_time_us());
}