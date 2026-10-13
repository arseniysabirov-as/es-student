#pragma once

#include <stdint.h>

#define DEVICE_NAME "firmware_template"
#define FIRMWARE_VERSION "2.0.0"

#define DEVICE_PROJECT "231_firmware_template"
#define DEVICE_REPO "https://github.com/arseniysabirov-as/es-student"

#ifndef DEVICE_BOARD
#define DEVICE_BOARD "unknown"
#endif


void device_info(void);