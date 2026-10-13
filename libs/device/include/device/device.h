#pragma once

#include <stdint.h>

#define DEVICE_NAME       "231-firmware-template"   // имя прошивки (по приложению Б)
#define FIRMWARE_VERSION  "1.0.0"
#define DEVICE_PROJECT    "231-firmware-template"
#define DEVICE_REPO       "https://github.com/<ваш логин>/es-student"

#ifndef DEVICE_BOARD
#define DEVICE_BOARD "unknown"
#endif

// сведения о плате и кристалле
typedef struct
{
    const char *board;         // значение PICO_BOARD
    char        serial[17];    // 16 hex-цифр + '\0'
    uint32_t    manufacturer;
    uint32_t    part;
    uint32_t    revision;
    const char *sdk_version;
} device_info_t;

// заполняет структуру сведениями о плате и кристалле
void device_get_info(device_info_t *info);