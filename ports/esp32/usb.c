/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/runtime.h"
#include "py/mphal.h"
#include "usb.h"

#if CONFIG_USB_OTG_SUPPORTED && !CONFIG_ESP_CONSOLE_USB_CDC && !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG

#include "esp_timer.h"
#include "esp_mac.h"

#ifndef NO_QSTR
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#endif

#if MICROPY_PY_MACHINE_CDC
#include "machine_cdc.h"
#endif

#define CDC_ITF TINYUSB_CDC_ACM_0

static char serial_string[12 + 1] = {0};
static tusb_desc_strarray_device_t descriptor_str_custom = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},                       // 0: is supported language is English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING,    // 1: Manufacturer
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,         // 2: Product
    /*CONFIG_TINYUSB_DESC_SERIAL_STRING */
    serial_string,                              // 3: Serials, should use chip ID 

#if CONFIG_TINYUSB_CDC_ENABLED
    CONFIG_TINYUSB_DESC_CDC_STRING,             // 4: CDC Interface
#else
    "",
#endif

#if CONFIG_TINYUSB_MSC_ENABLED
    CONFIG_TINYUSB_DESC_MSC_STRING,             // 5: MSC Interface
#else
    "",
#endif
};


#if !MICROPY_PY_MACHINE_CDC
static uint8_t usb_rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE];

static void usb_callback_rx(int itf, cdcacm_event_t *event) {
    // TODO: what happens if more chars come in during this function, are they lost?
    for (;;) {
        size_t len = 0;
        esp_err_t ret = tinyusb_cdcacm_read(itf, usb_rx_buf, sizeof(usb_rx_buf), &len);
        if (ret != ESP_OK) {
            break;
        }
        if (len == 0) {
            break;
        }
        for (size_t i = 0; i < len; ++i) {
            #ifdef MICROPY_PY_MACHINE_CDC
            if (pcdc_rx_ringbuf) {
                ringbuf_put(pcdc_rx_ringbuf, usb_rx_buf[i]);
            }
            #else
            if (usb_rx_buf[i] == mp_interrupt_char) {
                mp_sched_keyboard_interrupt();
            } else {
                ringbuf_put(&stdin_ringbuf, usb_rx_buf[i]);
            }
            #endif
        }
    }
}
#endif


void usb_init(void) {
    uint8_t chipid[6];
    esp_efuse_mac_get_default(chipid);

   // convert chipid to hex
    int hexlen = sizeof(serial_string) - 1;
    for (int i = 0; i < hexlen; i += 2) {
        static const char *hexdig = "0123456789abcdef";
        serial_string[i] = hexdig[chipid[i / 2] >> 4];
        serial_string[i + 1] = hexdig[chipid[i / 2] & 0x0f];
    }
    serial_string[hexlen] = 0;

    // Initialise the USB with defaults.
    tinyusb_config_t tusb_cfg = {0};
    tusb_cfg.string_descriptor = descriptor_str_custom;
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // Initialise the USB serial interface.
    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = CDC_ITF,
        .rx_unread_buf_sz = 256,
        #if MICROPY_PY_MACHINE_CDC
        .callback_rx = &machine_cdc_usb_callback_rx,
        .callback_rx_wanted_char = &machine_cdc_cdcacm_callback,
        .callback_line_state_changed = &machine_cdc_cdcacm_callback,
        .callback_line_coding_changed = &machine_cdc_cdcacm_callback,
        #else
        .callback_rx = &usb_callback_rx,
        #ifdef MICROPY_HW_USB_CUSTOM_RX_WANTED_CHAR_CB
        .callback_rx_wanted_char = &MICROPY_HW_USB_CUSTOM_RX_WANTED_CHAR_CB,
        #endif
        #ifdef MICROPY_HW_USB_CUSTOM_LINE_STATE_CB
        .callback_line_state_changed = &MICROPY_HW_USB_CUSTOM_LINE_STATE_CB,
        #endif
        #ifdef MICROPY_HW_USB_CUSTOM_LINE_CODING_CB
        .callback_line_coding_changed = &MICROPY_HW_USB_CUSTOM_LINE_CODING_CB,
        #endif
        #endif //MICROPY_PY_MACHINE_CDC
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));

}

void usb_tx_strn(const char *str, size_t len) {
    // Write out the data to the CDC interface, but only while the USB host is connected.
    uint64_t timeout = esp_timer_get_time() + (uint64_t)(MICROPY_HW_USB_CDC_TX_TIMEOUT_MS * 1000);
    while (tud_cdc_n_connected(CDC_ITF) && len && esp_timer_get_time() < timeout) {
        size_t l = tinyusb_cdcacm_write_queue(CDC_ITF, (uint8_t *)str, len);
        str += l;
        len -= l;
        tud_cdc_n_write_flush(CDC_ITF);
    }
}

#endif // CONFIG_USB_OTG_SUPPORTED && !CONFIG_ESP_CONSOLE_USB_CDC && !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
