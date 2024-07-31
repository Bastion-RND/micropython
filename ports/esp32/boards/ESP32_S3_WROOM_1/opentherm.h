//
// Created by root on 29.07.2024.
//

#ifndef MICROPYTHON_OPENTHERM_H
#define MICROPYTHON_OPENTHERM_H

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "hal/gpio_ll.h"

#include "esp_log.h"

// Include MicroPython API.
#include "py/runtime.h"
#include "py/obj.h"
// Used to get the time in the Timer class example.
#include "py/mphal.h"
#include "hal/timer_hal.h"
#include "hal/timer_ll.h"
#include "soc/timer_periph.h"

typedef enum {
    OT_NOT_INITIALIZED,
    OT_READY,
    OT_DELAY,
    OT_REQUEST_SENDING,
    OT_RESPONSE_WAITING,
    OT_RESPONSE_START_BIT,
    OT_RESPONSE_RECEIVING,
    OT_RESPONSE_READY,
    OT_RESPONSE_INVALID
} OpenThermStatus;

#define OT_DEFAULT_TIMEOUT 100
#define OT_DEFAULT_INVERSION 0

typedef struct _opentherm_obj_t {
    mp_obj_base_t base;
    OpenThermStatus state;
    gpio_num_t in;
    gpio_num_t out;
    mp_int_t isSlave;
    gpio_num_t debug;
    mp_int_t invert;
    mp_int_t timeout;
    uint32_t responseTimestamp;
    uint8_t responseBitIndex;
    uint32_t response;
    ringbuf_t rx_buf;
} opentherm_obj_t;

#endif //MICROPYTHON_OPENTHERM_H
