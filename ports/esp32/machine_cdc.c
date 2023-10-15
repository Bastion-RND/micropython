#include "py/runtime.h"
#include "py/ringbuf.h"
#include "py/stream.h"
#include "esp_log.h"

#ifndef NO_QSTR
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#endif

#define DEBUG 1 //FIXME! --> 0
#if DEBUG
#define DEBUG_printf(...) ESP_LOGI("CDC", __VA_ARGS__)
#else
#define DEBUG_printf(...) (void)0
#endif

#if MICROPY_PY_MACHINE_CDC

const mp_obj_type_t machine_cdc_type;

typedef struct _machine_cdc_obj_t {
    mp_obj_base_t base;
    ringbuf_t tx_ringbuf;
    ringbuf_t rx_ringbuf;
    char wanted_char;
    bool dtr;
    bool rts;
    cdc_line_coding_t line_coding;
} machine_cdc_obj_t;

static machine_cdc_obj_t *self = NULL;


/******************************************************************************/
// USB CDC Callbacks 

void machine_cdc_usb_callback_rx(int itf, cdcacm_event_t *event) {
    uint8_t usb_rx_buf[64];
    for (;;) {
        size_t len = 0;
        esp_err_t ret = tinyusb_cdcacm_read(itf, usb_rx_buf, sizeof(usb_rx_buf), &len);
        if (ret != ESP_OK) {
            break;
        }
        if (len == 0) {
            break;
        }
        if (self && self->rx_ringbuf.buf) {
            for (size_t i = 0; i < len; ++i) {
                ringbuf_put(&self->rx_ringbuf, usb_rx_buf[i]);
            }
        } 
    }
}

void machine_cdc_cdcacm_callback(int itf, cdcacm_event_t *event) {
    machine_cdc_obj_t *instance = MP_OBJ_TO_PTR(self);
    if (instance) {
        switch (event->type)
        {
        case CDC_EVENT_RX_WANTED_CHAR:
            DEBUG_printf("Received wanted char: %c", event->rx_wanted_char_data.wanted_char);
            break;
        
        case CDC_EVENT_LINE_STATE_CHANGED:
            instance->dtr = event->line_state_changed_data.dtr;
            instance->rts = event->line_state_changed_data.rts;
            DEBUG_printf("Callback line state: DTR(%i), RTS(%i)", instance->dtr, instance->rts);
            break;

        case CDC_EVENT_LINE_CODING_CHANGED:
            memcpy(&instance->line_coding, event->line_coding_changed_data.p_line_coding, sizeof(cdc_line_coding_t));
            DEBUG_printf("Callback line coding: bit_rate=%lu, data_bits=%i, parity=%i, stop_bits=%i", 
                instance->line_coding.bit_rate, instance->line_coding.data_bits,
                instance->line_coding.parity, instance->line_coding.stop_bits);
            break;

        default:
            DEBUG_printf("cdcacm_callback - unknown event: %i", event->type);
            break;
        }
    }    
}




/******************************************************************************/
// MicroPython bindings 

STATIC void machine_cdc_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "CDC(txbuf_free: %u, rxbuf_available: %u)", 
        ringbuf_free(&self->tx_ringbuf), ringbuf_avail(&self->rx_ringbuf));    
}

STATIC void machine_cdc_init_helper(machine_cdc_obj_t *self, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_txbuf, ARG_rxbuf, ARG_timeout };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_txbuf, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = CONFIG_TINYUSB_CDC_TX_BUFSIZE *2} },
        { MP_QSTR_rxbuf, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = CONFIG_TINYUSB_CDC_RX_BUFSIZE *2} },
        { MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_txbuf].u_int >= 0) {
        if (self->tx_ringbuf.size != args[ARG_txbuf].u_int) {
            if (self->tx_ringbuf.buf) {
                free(self->tx_ringbuf.buf);
            }
            ringbuf_alloc(&self->tx_ringbuf, args[ARG_txbuf].u_int);
        }
    }

    if (args[ARG_rxbuf].u_int >= 0) {
        if (self->rx_ringbuf.size != args[ARG_rxbuf].u_int) {
            if (self->rx_ringbuf.buf) {
                free(self->rx_ringbuf.buf);
            }
            ringbuf_alloc(&self->rx_ringbuf, args[ARG_rxbuf].u_int);
        }
    }

    uint8_t line_state = tud_cdc_n_get_line_state(TINYUSB_CDC_ACM_0);
    self->dtr = (line_state >> 0) & 0x1;
    self->rts = (line_state >> 1) & 0x1;
    DEBUG_printf("Init line state: DTR(%i), RTS(%i)", self->dtr, self->rts);

    tud_cdc_n_get_line_coding(TINYUSB_CDC_ACM_0, &self->line_coding);
    DEBUG_printf("Init line coding: bit_rate=%lu, data_bits=%i, parity=%i, stop_bits=%i", 
        self->line_coding.bit_rate, self->line_coding.data_bits,
        self->line_coding.parity, self->line_coding.stop_bits);

    self->wanted_char = '~'; //FIXME!
    tud_cdc_n_set_wanted_char(TINYUSB_CDC_ACM_0, self->wanted_char);

    //TODO...
}

STATIC mp_obj_t machine_cdc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    if (!self) {
        self = mp_obj_malloc(machine_cdc_obj_t, &machine_cdc_type);
        DEBUG_printf("Created instance: %p", self);
    } else {
        DEBUG_printf("Using existed instance: %p", self);
    }

    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    machine_cdc_init_helper(self, n_args - 1, args + 1, &kw_args);

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t machine_cdc_init(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    machine_cdc_init_helper(args[0], n_args - 1, args + 1, kw_args);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_cdc_init_obj, 1, machine_cdc_init);

STATIC mp_obj_t machine_cdc_deinit(mp_obj_t self_in) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->tx_ringbuf.buf) {
        free(self->tx_ringbuf.buf);
    }
    if (self->rx_ringbuf.buf) {
        free(self->rx_ringbuf.buf);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_deinit_obj, machine_cdc_deinit);

STATIC mp_obj_t machine_cdc_any(mp_obj_t self_in) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    size_t rxbufsize = ringbuf_avail(&self->rx_ringbuf);
    return MP_OBJ_NEW_SMALL_INT(rxbufsize);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_cdc_any_obj, machine_cdc_any);

STATIC const mp_rom_map_elem_t machine_cdc_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_cdc_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_cdc_deinit_obj) },
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
    int bytes_read = 0;

    // make sure we want at least 1 char
    if (size == 0) {
        return 0;
    }

    while (ringbuf_avail(&self->rx_ringbuf) && (bytes_read < size)) {
        *(uint8_t*)(buf_in + bytes_read++) = ringbuf_get(&self->rx_ringbuf);
    }

    if (bytes_read <= 0) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }
    return bytes_read;
}

STATIC mp_uint_t machine_cdc_write(mp_obj_t self_in, const void *buf_in, mp_uint_t size, int *errcode) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int bytes_written = 0;

    while (ringbuf_free(&self->tx_ringbuf) && (bytes_written < size)) {
        ringbuf_put(&self->tx_ringbuf, *(uint8_t*)(buf_in + bytes_written++));
    }

    //TODO: usb_tx_strn(str, len);

    if (bytes_written < 0) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }

    return bytes_written;
}

STATIC mp_uint_t machine_cdc_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    machine_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_uint_t ret;
    if (request == MP_STREAM_POLL) {
        mp_uint_t flags = arg;
        ret = 0;
        if ((flags & MP_STREAM_POLL_RD) && ringbuf_avail(&self->rx_ringbuf)) {
            ret |= MP_STREAM_POLL_RD;
        }
        if ((flags & MP_STREAM_POLL_WR) && ringbuf_free(&self->tx_ringbuf)) { 
            ret |= MP_STREAM_POLL_WR;
        }
    } else if (request == MP_STREAM_FLUSH) {
        //TODO...
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