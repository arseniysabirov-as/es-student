#pragma once

#include <stdint.h>

#define DEVICE_NAME "es-led-module"
#define FIRMWARE_VERSION "1.1.0"

#define DEVICE_PROJECT "221-command-time"
#define DEVICE_REPO "https://github.com/arseniysabirov-as/es-student"

#ifndef DEVICE_BOARD
#define DEVICE_BOARD "unknown"
#endif


// ─── новый тип: раскладка паспорта в памяти ───────────────
struct info_t
{
    uint32_t version;
    uint8_t  revision;
    char     name[13];
};

// ─── extern: определение будет в device.c, здесь только объявление ─
extern struct info_t device_card;

void device_info(void);
void dev_info(void);