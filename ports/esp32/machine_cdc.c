#include "py/runtime.h"
#include "py/ringbuf.h"
#include "py/stream.h"
#include "esp_log.h"

#ifndef NO_QSTR
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tusb.h"
#endif

#define LOG_PREFIX  "CDC"
#define itf         TINYUSB_CDC_ACM_0

#if MICROPY_PY_MACHINE_CDC

const mp_obj_type_t machine_cdc_type;

typedef struct _machine_cdc_obj_t {
    mp_obj_base_t base;
    bool dummy;
} machine_cdc_obj_t;

STATIC machine_cdc_obj_t cdc_default = {
    {&machine_cdc_type}, false
};
STATIC machine_cdc_obj_t *instance = &cdc_default;


/******************************************************************************/
// USB CDC Callbacks

void machine_cdc_callback(int itf, cdcacm_event_t *event) {
    //machine_cdc_obj_t *self = MP_OBJ_TO_PTR(instance);
    // if (self) {
    //     switch (event->type)
    //     {
    //         case CDC_EVENT_RX:
    //             for (;;) {
    //                 uint8_t buf[64];
    //                 size_t len;

    //                 esp_err_t ret = tinyusb_cdcacm_read(itf, buf, sizeof(buf), &len);
    //                 if (ret != ESP_OK) {
    //                     break;
    //                 }
    //                 if (len == 0) {
    //                     break;
    //                 }
    //                 for (size_t i = 0; i < len; i++) {
    //                     if (ringbuf_free(&self->rx_ringbuf) == 0) {
    //                         ringbuf_get(&self->rx_ringbuf);
    //                     }
    //                     ringbuf_put(&self->rx_ringbuf, buf[i]);
    //                 }
    //                 break;
    //             }
    //             break;

    //         case CDC_EVENT_RX_WANTED_CHAR:
    //             ESP_LOGD(LOG_PREFIX, "Received wanted char: %c",
    //                 event->rx_wanted_char_data.wanted_char);
    //             break;

    //         case CDC_EVENT_LINE_STATE_CHANGED:
    //             ESP_LOGD(LOG_PREFIX, "Callback line state: DTR(%i), RTS(%i)",
    //                 event->line_state_changed_data.dtr,
    //                 event->line_state_changed_data.rts);
    //             break;

    //         case CDC_EVENT_LINE_CODING_CHANGED:
    //             ESP_LOGD(LOG_PREFIX, "Callback line coding: bit_rate=%lu, data_bits=%i, parity=%i, stop_bits=%i",
    //                 event->line_coding_changed_data.p_line_coding->bit_rate,
    //                 event->line_coding_changed_data.p_line_coding->data_bits,
    //                 event->line_coding_changed_data.p_line_coding->parity,
    //                 event->line_coding_changed_data.p_line_coding->stop_bits);
    //             break;

    //         default:
    //             ESP_LOGE(LOG_PREFIX, "cdcacm_callback - unknown event: %i", event->type);
    //             break;
    //         }
    // }
}

void tud_cdc_tx_complete_cb(uint8_t itf) {
    // machine_cdc_obj_t *self = MP_OBJ_TO_PTR(instance);
    int bytes_left = CONFIG_TINYUSB_CDC_TX_BUFSIZE - tud_cdc_write_available();
    ESP_LOGI(LOG_PREFIX, "cdc_tx_complete: %i bytes left", bytes_left);
}


/******************************************************************************/
// MicroPython bindings

STATIC void machine_cdc_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    // machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t line_state = tud_cdc_n_get_line_state(itf);
    bool connected = tud_cdc_n_connected(itf);
    mp_printf(print, "CDC(DTR: %d, RTS: %d, connected: %s)",
        ((line_state >> 0) & 0x1), ((line_state >> 1) & 0x1), connected ? "yes" : "no");
}


STATIC mp_obj_t machine_cdc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(instance);
    enum { ARG_rxbuf, ARG_timeout };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_rxbuf, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 1024} },
        { MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_rxbuf].u_int >= 0) {
        // if (instance->rx_ringbuf.size != args[ARG_rxbuf].u_int) {
        //     instance->rx_ringbuf.size = args[ARG_rxbuf].u_int;
        //     if (instance->rx_ringbuf.buf) {
        //         free(instance->rx_ringbuf.buf);
        //     }
        //     instance->rx_ringbuf.buf = malloc(instance->rx_ringbuf.size);
        // }
        // instance->rx_ringbuf.iget = instance->rx_ringbuf.iput = 0;
    }

    // TODO? timeout

    // tud_cdc_set_wanted_char('\n'); // TODO! parameter...

    return self;
}

STATIC mp_obj_t machine_cdc_connected(mp_obj_t self_in) {
    // machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bool connected = tud_cdc_n_connected(itf);
    ESP_LOGI(LOG_PREFIX, "connected: %s", connected ? "yes" : "no");
    return mp_obj_new_bool(connected);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_connected_obj, machine_cdc_connected);


STATIC mp_obj_t machine_cdc_line_state(mp_obj_t self_in) {
    // machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t line_state = tud_cdc_n_get_line_state(itf);
    mp_obj_t dict = mp_obj_new_dict(2);
    mp_obj_dict_store(dict, mp_obj_new_str("DTR", 3), mp_obj_new_int((line_state >> 0) & 0x1));
    mp_obj_dict_store(dict, mp_obj_new_str("RTS", 3), mp_obj_new_int((line_state >> 1) & 0x1));
    ESP_LOGI(LOG_PREFIX, "line state: DTR(%i), RTS(%i)", (line_state >> 0) & 0x1, (line_state >> 1) & 0x1);
    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_line_state_obj, machine_cdc_line_state);

STATIC mp_obj_t machine_cdc_line_coding(mp_obj_t self_in) {
    //machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    cdc_line_coding_t line_coding;
    tud_cdc_n_get_line_coding(itf, &line_coding);
    mp_obj_t dict = mp_obj_new_dict(4);
    mp_obj_dict_store(dict, mp_obj_new_str("bit_rate", 8), mp_obj_new_int(line_coding.bit_rate));
    mp_obj_dict_store(dict, mp_obj_new_str("data_bits", 9), mp_obj_new_int(line_coding.data_bits));
    mp_obj_dict_store(dict, mp_obj_new_str("parity", 6), mp_obj_new_int(line_coding.parity));
    mp_obj_dict_store(dict, mp_obj_new_str("stop_bits", 9), mp_obj_new_int(line_coding.stop_bits));
    ESP_LOGI(LOG_PREFIX, "line coding: bit_rate=%lu, data_bits=%i, parity=%i, stop_bits=%i",
        line_coding.bit_rate, line_coding.data_bits, line_coding.parity, line_coding.stop_bits);
    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_line_coding_obj, machine_cdc_line_coding);

STATIC mp_obj_t machine_cdc_any(mp_obj_t self_in) {
    return MP_OBJ_NEW_SMALL_INT(tud_cdc_n_available(itf));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_any_obj, machine_cdc_any);

STATIC const mp_rom_map_elem_t machine_cdc_locals_dict_table[] = {
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
    size_t bytes_read = 0;
    // make sure we want at least 1 char
    if (size == 0) {
        return 0;
    }
    if (ESP_OK != tinyusb_cdcacm_read(itf, buf_in, size, &bytes_read)) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }
    return bytes_read;
}

STATIC mp_uint_t machine_cdc_write(mp_obj_t self_in, const void *buf_in, mp_uint_t size, int *errcode) {
    size_t bytes_written = tinyusb_cdcacm_write_queue(itf, buf_in, size);
    return bytes_written;
}

STATIC mp_uint_t machine_cdc_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    mp_uint_t ret;
    if (request == MP_STREAM_POLL) {
        mp_uint_t flags = arg;
        ret = 0;
        if ((flags & MP_STREAM_POLL_RD) && tud_cdc_n_available(itf)) {
            ret |= MP_STREAM_POLL_RD;
        }
        if ((flags & MP_STREAM_POLL_WR) && (CONFIG_TINYUSB_CDC_TX_BUFSIZE - tud_cdc_n_write_available(itf))) {
            ret |= MP_STREAM_POLL_WR;
        }
    } else if (request == MP_STREAM_FLUSH) {
        tinyusb_cdcacm_write_flush(itf, 0);
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
