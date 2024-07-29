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

/* Declarations and definitions ----------------------------------------------*/

#define OPENTHERM_ENCODE_TIMER_PERIOD_US                            10
#define OPENTHERM_BIT_TIME_US                                       500

#define OPENTHERM_BYTES_NUM                                         4
#define OPENTHERM_SYNC_BYTES_NUM                                    2
#define OPENTHERM_DATA_BYTES_NUM                                    OPENTHERM_BYTES_NUM - OPENTHERM_SYNC_BYTES_NUM

#define OPENTHERM_BITS_IN_BYTE_NUM                                  8

#define OPENTHERM_SYNC_FIELD                                        0xAA55

#define OPENTHERM_DECODE_TIMER_PERIOD_US                            10

#define OPENTHERM_ENCODE_TIMER_MAX                                  OPENTHERM_BIT_TIME_US / OPENTHERM_ENCODE_TIMER_PERIOD_US

#define OPENTHERM_DECODE_TIMER_MAX                                  OPENTHERM_BIT_TIME_US / OPENTHERM_DECODE_TIMER_PERIOD_US
#define OPENTHERM_DECODE_TIMER_THRESHOLD                            OPENTHERM_DECODE_TIMER_MAX * 3 / 4



typedef enum
{
    NOT_SYNC,
    BIT_SYNC,
    DATA_SYNC,
    DATA_READY
} OPENTHERM_DecodeState;

typedef enum
{
    NONE,
    RAISING_EDGE,
    FALLING_EDGE
} OPENTHERM_DecodeEdge;

typedef struct OPENTHERM_Data
{
    uint8_t data[OPENTHERM_BYTES_NUM];
    uint16_t bitStream;
    uint16_t byteIdx;
    uint16_t bytesNum;
    uint8_t bitIdx;
    bool active;
} OPENTHERM_Data;



#define OT_DEFAULT_TIMEOUT 100
#define OT_DEFAULT_INVERSION 0

typedef struct _opentherm_obj_t {
    mp_obj_base_t base;
    gpio_num_t in;
    gpio_num_t out;
    gpio_num_t debug;
    mp_int_t invert;
    mp_int_t timeout;
    ringbuf_t rx_buf;
} opentherm_obj_t;


/* Functions -----------------------------------------------------------------*/

extern void OPENTHERM_Encode(uint8_t* data, uint8_t size);
extern void OPENTHERM_DecodeReset();


#endif //MICROPYTHON_OPENTHERM_H
