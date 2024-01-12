#include "py/runtime.h"
#include "py/ringbuf.h"
#include "py/stream.h"

#if MICROPY_PY_MACHINE_CDC

#ifndef NO_QSTR
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tusb.h"
#endif

#include "esp_log.h"

#define LOG_LEVEL_MACHINE_CDC ESP_LOG_WARN
#define LOG_TAG_СDC "USBCDC"

#define INTERFACE TINYUSB_CDC_ACM_0

const mp_obj_type_t machine_cdc_type;

typedef struct _machine_cdc_obj_t {
    mp_obj_base_t base;
    ringbuf_t rx_buf;
} machine_cdc_obj_t;

STATIC machine_cdc_obj_t cdc_obj = {
    {&machine_cdc_type}, {0}
};
STATIC machine_cdc_obj_t *instance = &cdc_obj;


static bool is_initialized(machine_cdc_obj_t *self) {
    return self->rx_buf.buf != NULL;
}

/******************************************************************************/
// USB CDC Callbacks

void machine_cdc_callback(int itf, cdcacm_event_t *event) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(instance);
    switch (event->type)
    {
        case CDC_EVENT_RX:
            for (;;) {
                uint8_t buf[64];
                size_t len;
                esp_err_t ret = tinyusb_cdcacm_read(itf, buf, sizeof(buf), &len);
                if (ret != ESP_OK) {
                    break;
                }
                if (len == 0) {
                    break;
                }
                if (is_initialized(self)) {
                    for (size_t i = 0; i < len; i++) {
                        if (ringbuf_free(&self->rx_buf) == 0) {
                            ringbuf_get(&self->rx_buf);
                        }
                        ringbuf_put(&self->rx_buf, buf[i]);
                    }
                }
                ESP_LOGD(LOG_TAG_СDC, "RX event, %s %u bytes",
                    is_initialized(self) ? "received" : "flushed", len);
            }
            break;

        case CDC_EVENT_RX_WANTED_CHAR:
            ESP_LOGD(LOG_TAG_СDC, "Received wanted char '%c'",
                event->rx_wanted_char_data.wanted_char);
            break;

        case CDC_EVENT_LINE_STATE_CHANGED:
            ESP_LOGD(LOG_TAG_СDC, "Line state changed [DTR: %i, RTS: %i]",
                event->line_state_changed_data.dtr,
                event->line_state_changed_data.rts);
            break;

        case CDC_EVENT_LINE_CODING_CHANGED:
            ESP_LOGD(LOG_TAG_СDC, "Line coding changed [bit_rate: %lu, data_bits: %i, parity: %i, stop_bits:%i]",
                event->line_coding_changed_data.p_line_coding->bit_rate,
                event->line_coding_changed_data.p_line_coding->data_bits,
                event->line_coding_changed_data.p_line_coding->parity,
                event->line_coding_changed_data.p_line_coding->stop_bits);
            break;

        default:
            ESP_LOGE(LOG_TAG_СDC, "cdcacm_callback - unknown event: %i", event->type);
            break;
    }
}

void tud_cdc_tx_complete_cb(uint8_t itf) {
    const uint32_t available = tud_cdc_n_write_available(itf);
    ESP_LOGD(LOG_TAG_СDC, "TX complete, remained %lu",
        CONFIG_TINYUSB_CDC_TX_BUFSIZE - available
        );
}


/******************************************************************************/
// MicroPython bindings

STATIC void machine_cdc_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t line_state = tud_cdc_n_get_line_state(INTERFACE);
    mp_printf(print, "CDC(DTR: %d, RTS: %d, rxbuf: %u/%u)",
        ((line_state >> 0) & 0x1), ((line_state >> 1) & 0x1),
        ringbuf_avail(&self->rx_buf), self->rx_buf.size
        );
}

STATIC mp_obj_t machine_cdc_deinit(mp_obj_t self_in) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    ESP_LOGD(LOG_TAG_СDC, "deinit self: %p, buf ptr: %p", self, self->rx_buf.buf);
    if (self) {
        if (self->rx_buf.buf) {
            m_del(uint8_t, self->rx_buf.buf, self->rx_buf.size);
            self->rx_buf.buf = NULL;
        }
    }
    ESP_LOGD(LOG_TAG_СDC, "deinited...");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_deinit_obj, machine_cdc_deinit);

STATIC mp_obj_t machine_cdc_init_helper(machine_cdc_obj_t *self, mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_rxbuf };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_rxbuf,    MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 1024} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (self) {
        if (args[ARG_rxbuf].u_int <= 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("rx buffer size must be > 0"));
        }
        if (self->rx_buf.buf) {
            m_del(uint8_t, self->rx_buf.buf, self->rx_buf.size);
        }
        self->rx_buf.size = args[ARG_rxbuf].u_int;
        self->rx_buf.iget = self->rx_buf.iput = 0;
        self->rx_buf.buf = m_new(uint8_t, self->rx_buf.size);
    }
    ESP_LOGD(LOG_TAG_СDC, "inited self: %p, buf ptr: %p", self, self->rx_buf.buf);
    return mp_const_none;
}

STATIC mp_obj_t machine_cdc_init(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    machine_cdc_obj_t *self = args[0];
    machine_cdc_deinit(self);
    return machine_cdc_init_helper(self, n_args - 1, args + 1, kw_args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_cdc_init_obj, 0, machine_cdc_init);

STATIC mp_obj_t machine_cdc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(instance);
    mp_arg_check_num(n_args, n_kw, 0, MP_OBJ_FUN_ARGS_MAX, true);

    if (esp_log_level_get(LOG_TAG_СDC) != LOG_LEVEL_MACHINE_CDC) {
        esp_log_level_set(LOG_TAG_СDC, LOG_LEVEL_MACHINE_CDC);
    }

    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    machine_cdc_init_helper(self, n_args, args, &kw_args);

    return self;
}

STATIC mp_obj_t machine_cdc_connected(mp_obj_t self_in) {
    // machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bool connected = tud_cdc_n_connected(INTERFACE);
    ESP_LOGD(LOG_TAG_СDC, "connected: %s", connected ? "yes" : "no");
    return mp_obj_new_bool(connected);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_connected_obj, machine_cdc_connected);

STATIC mp_obj_t machine_cdc_line_state(mp_obj_t self_in) {
    // machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t line_state = tud_cdc_n_get_line_state(INTERFACE);
    mp_obj_t dict = mp_obj_new_dict(2);
    mp_obj_dict_store(dict, mp_obj_new_str("DTR", 3), mp_obj_new_int((line_state >> 0) & 0x1));
    mp_obj_dict_store(dict, mp_obj_new_str("RTS", 3), mp_obj_new_int((line_state >> 1) & 0x1));
    ESP_LOGD(LOG_TAG_СDC, "line state: DTR(%i), RTS(%i)", (line_state >> 0) & 0x1, (line_state >> 1) & 0x1);
    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_line_state_obj, machine_cdc_line_state);

STATIC mp_obj_t machine_cdc_line_coding(mp_obj_t self_in) {
    // machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    cdc_line_coding_t line_coding;
    tud_cdc_n_get_line_coding(INTERFACE, &line_coding);
    mp_obj_t dict = mp_obj_new_dict(4);
    mp_obj_dict_store(dict, mp_obj_new_str("bit_rate", 8), mp_obj_new_int(line_coding.bit_rate));
    mp_obj_dict_store(dict, mp_obj_new_str("data_bits", 9), mp_obj_new_int(line_coding.data_bits));
    mp_obj_dict_store(dict, mp_obj_new_str("parity", 6), mp_obj_new_int(line_coding.parity));
    mp_obj_dict_store(dict, mp_obj_new_str("stop_bits", 9), mp_obj_new_int(line_coding.stop_bits));
    ESP_LOGD(LOG_TAG_СDC, "line coding: bit_rate=%lu, data_bits=%i, parity=%i, stop_bits=%i",
        line_coding.bit_rate, line_coding.data_bits, line_coding.parity, line_coding.stop_bits);
    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_line_coding_obj, machine_cdc_line_coding);

STATIC mp_obj_t machine_cdc_any(mp_obj_t self_in) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int available = ringbuf_avail(&self->rx_buf);
    return MP_OBJ_NEW_SMALL_INT(available);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_any_obj, machine_cdc_any);

STATIC const mp_rom_map_elem_t machine_cdc_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&machine_cdc_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_cdc_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_cdc_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_connected), MP_ROM_PTR(&machine_cdc_connected_obj) },
    { MP_ROM_QSTR(MP_QSTR_line_state), MP_ROM_PTR(&machine_cdc_line_state_obj) },
    { MP_ROM_QSTR(MP_QSTR_line_coding), MP_ROM_PTR(&machine_cdc_line_coding_obj) },
    { MP_ROM_QSTR(MP_QSTR_any), MP_ROM_PTR(&machine_cdc_any_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
};
STATIC MP_DEFINE_CONST_DICT(machine_cdc_locals_dict, machine_cdc_locals_dict_table);

STATIC mp_uint_t machine_cdc_read(mp_obj_t self_in, void *buf_in, mp_uint_t size, int *errcode) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    size_t bytes_read = 0;
    if (!is_initialized(self)) {
        *errcode = MP_ENOENT;
        return MP_STREAM_ERROR;
    }
    // make sure we want at least 1 char
    if (size == 0) {
        return 0;
    }
    while ((bytes_read < size) && ringbuf_avail(&self->rx_buf)) {
        *(uint8_t *)(buf_in + bytes_read++) = ringbuf_get(&self->rx_buf);
    }
    return bytes_read;
}

STATIC mp_uint_t machine_cdc_write(mp_obj_t self_in, const void *buf_in, mp_uint_t size, int *errcode) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    size_t bytes_written = 0;
    if (!is_initialized(self)) {
        *errcode = MP_ENODEV;
        return MP_STREAM_ERROR;
    }
    if (size == 0) {
        return 0;
    }
    bytes_written += tinyusb_cdcacm_write_queue(INTERFACE, buf_in, size);
    if (bytes_written >= 64) {
        esp_err_t ret = tinyusb_cdcacm_write_flush(INTERFACE, pdMS_TO_TICKS(10));
        if (ret != ESP_OK) {
            *errcode = (ret == ESP_ERR_TIMEOUT) ? MP_ETIMEDOUT : MP_ENODEV;
            return MP_STREAM_ERROR;
        }
    }
    if (bytes_written == 0) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }
    ESP_LOGD(LOG_TAG_СDC, "written to queue: %u bytes", bytes_written);
    return bytes_written;
}

STATIC mp_uint_t machine_cdc_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_uint_t ret;
    if (!is_initialized(self)) {
        *errcode = MP_ENOENT;
        return MP_STREAM_ERROR;
    }
    if (request == MP_STREAM_POLL) {
        mp_uint_t flags = arg;
        ret = 0;
        if ((flags & MP_STREAM_POLL_RD) && ringbuf_avail(&self->rx_buf)) {
            ret |= MP_STREAM_POLL_RD;
        }
        if ((flags & MP_STREAM_POLL_WR) && 1) {
            ret |= MP_STREAM_POLL_WR;
        }
    } else if (request == MP_STREAM_FLUSH) {
        tinyusb_cdcacm_write_flush(INTERFACE, 0);
        ret = 0;
    } else {
        *errcode = MP_EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}

STATIC const mp_stream_p_t cdc_stream_p = {
    .read = machine_cdc_read,
    .write = machine_cdc_write,
    .ioctl = machine_cdc_ioctl,
    .is_text = false,
};

MP_DEFINE_CONST_OBJ_TYPE(
    machine_cdc_type,
    MP_QSTR_CDC,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    make_new, machine_cdc_make_new,
    print, machine_cdc_print,
    protocol, &cdc_stream_p,
    locals_dict, &machine_cdc_locals_dict
    );

#endif
