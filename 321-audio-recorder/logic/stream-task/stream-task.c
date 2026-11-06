#include "stream-task/stream-task.h"
#include "platform/platform.h"

static stream_task_callback_t on_frame_cb = 0;
static stream_state_t state = STREAM_STATE_IDLE;
static uint8_t bits = 16;
static uint16_t frame_number = 0;
static uint32_t total_frames = 0;
static uint32_t total_samples = 0;

// stream_task_init(): запомнить функцию для кадров, состояние IDLE
void stream_task_init(stream_task_callback_t on_frame)
{
    on_frame_cb = on_frame;
    state = STREAM_STATE_IDLE;
}

// stream_task_start(): поток уже идёт или platform его не начал — false;
// запомнить разрядность, номер кадра и итог — с нуля, состояние RUN
bool stream_task_start(uint8_t b)
{
    if (state == STREAM_STATE_RUN) return false;
    if (!platform_stream_start(b)) return false;

    bits = b;
    frame_number = 0;
    total_frames = 0;
    total_samples = 0;
    state = STREAM_STATE_RUN;
    return true;
}

// stream_task_stop(): остановить поток в platform, состояние IDLE
void stream_task_stop(void)
{
    if (state != STREAM_STATE_RUN) return;
    platform_stream_stop();
    state = STREAM_STATE_IDLE;
}

// stream_task_handle(): в состоянии RUN, если platform отдал буфер:
//     заголовок — синхрослово STREAM_SYNC_WORD, номер, число отсчётов
//     байтов — отсчётов × 2 при 16 битах, отсчётов при 8
//     отдать заголовок и буфер функции, буфер вернуть platform
//     номер, число кадров и отсчётов — вперёд
void stream_task_handle(void)
{
    if (state != STREAM_STATE_RUN) return;

    uint32_t count;
    const uint8_t *buffer = platform_stream_next(&count);
    
    if (buffer != 0) {
        stream_header_t header;
        header.sync_word = STREAM_SYNC_WORD;
        header.number = frame_number;
        header.count = (uint16_t)count;

        uint32_t bytes = count * (bits / 8);
        if (on_frame_cb) {
            on_frame_cb(&header, buffer, bytes);
        }

        platform_stream_release();

        frame_number++;
        total_frames++;
        total_samples += count;
    }
}

stream_state_t stream_task_get_state(void) { return state; }
uint8_t stream_task_get_bits(void) { return bits; }
uint32_t stream_task_get_frames(void) { return total_frames; }
uint32_t stream_task_get_samples(void) { return total_samples; }
uint32_t stream_task_get_overruns(void) { return platform_stream_overruns(); }