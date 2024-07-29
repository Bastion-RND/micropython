//
// Created by EPushkarev on 19.07.2024.
//

#include "opentherm.h"
#include "string.h"

// Base data struct storage
const mp_obj_type_t opentherm_type;
static uint8_t _rx_buf[128];
static opentherm_obj_t opentherm_obj = {
        .base = {&opentherm_type},
        .in = -1,
        .out = -1,
        .debug = 6,
        .timeout = 100,
        .invert = 0,
        .rx_buf = {
                .buf = _rx_buf,
                .size = sizeof(_rx_buf),
                .iget = 0,
                .iput = 0,
        }
};


static opentherm_obj_t *instance = &opentherm_obj;


static OPENTHERM_Data encodeData;
static OPENTHERM_Data decodeData;

static uint8_t virtTact = 1;


static OPENTHERM_DecodeEdge curEdge = NONE;
static OPENTHERM_DecodeEdge prevEdge = NONE;

static OPENTHERM_DecodeState decodeState = NOT_SYNC;

static uint16_t encodeTimerCnt = 0;
static uint16_t decodeTimerCnt = 0;

static size_t write_to_buf(uint8_t *p, size_t len);


//manchester Code functions
void OPENTHERM_DataReadyCallback() {
    mp_printf(&mp_plat_print, "data recv:[");
    for (size_t i = 0; i < decodeData.bytesNum; i++) {
        mp_printf(&mp_plat_print, "%X,", decodeData.data[i]);
    }
    mp_printf(&mp_plat_print, "]\n");

    write_to_buf(decodeData.data, decodeData.bytesNum);

    return;
}

static void set_output(uint8_t state) {
    gpio_set_level(instance->out, state);
}

/*----------------------------------------------------------------------------*/
static bool get_input() {
    return (bool) gpio_get_level(instance->in);
}


/*----------------------------------------------------------------------------*/
static uint8_t get_data_bit(OPENTHERM_Data *OPENTHERMData) {
    uint8_t curByte = OPENTHERMData->data[OPENTHERMData->byteIdx];
    uint8_t curBitIdx = OPENTHERMData->bitIdx;
    return (curByte >> curBitIdx) & 0x01;
}


/*----------------------------------------------------------------------------*/
static void set_data_bit(OPENTHERM_Data *OPENTHERMData, uint8_t bit) {
    uint8_t curByteIdx = OPENTHERMData->byteIdx;
    uint8_t curBitIdx = OPENTHERMData->bitIdx;

    if (bit == 1) {
        OPENTHERMData->data[curByteIdx] |= (1 << curBitIdx);
    }
}

void timer_encode_callback() {
    if ((encodeTimerCnt == (OPENTHERM_ENCODE_TIMER_MAX / 2)) ||
        (encodeTimerCnt == OPENTHERM_ENCODE_TIMER_MAX)) {
        uint8_t curCodeBit = get_data_bit(&encodeData);
        uint8_t curOutputBit = curCodeBit ^ virtTact;
        set_output(curOutputBit);
        virtTact ^= 0x01;
    }

    if (encodeTimerCnt == OPENTHERM_ENCODE_TIMER_MAX) {
        encodeData.bitIdx++;

        if (encodeData.bitIdx == (OPENTHERM_BITS_IN_BYTE_NUM)) {
            encodeData.bitIdx = 0;

            encodeData.byteIdx++;
            if (encodeData.byteIdx == encodeData.bytesNum) {
                encodeData.active = 0;
            }
        }

        encodeTimerCnt = 0;
    }

    encodeTimerCnt++;
}

void timer_decode_callback() {
    if (decodeState == DATA_SYNC) {
        if (decodeTimerCnt >= (3 * OPENTHERM_DECODE_TIMER_MAX)) {
            decodeTimerCnt = 0;

            decodeData.active = 0;
            curEdge = NONE;
            prevEdge = NONE;

            decodeState = DATA_READY;

            // Data is ready
            OPENTHERM_DataReadyCallback();
        }
    }

    decodeTimerCnt++;
}

// Timer callback

void timer_callback() {
    // Encoding process
    if (encodeData.active) {
        timer_encode_callback();
    }

    // Decoding process
    if (decodeData.active) {
        timer_decode_callback();
    }
}


//
// all code executed in ISR must be in IRAM, and any const data must be in DRAM
static void opentherm_irq_handler(void *arg) {
    // FIXME: DELETE DEBUG PIN AFTER DEBUG!!!
    gpio_set_level(instance->debug, gpio_get_level(instance->in));

    curEdge = get_input() ? FALLING_EDGE : RAISING_EDGE;

    switch (decodeState) {
        case NOT_SYNC:
            if (!decodeData.active) {
                decodeData.active = true;
                decodeTimerCnt = 0;
            } else {
                if (((curEdge == FALLING_EDGE) && (prevEdge == RAISING_EDGE)) ||
                    ((curEdge == RAISING_EDGE) && (prevEdge == FALLING_EDGE))) {
                    if (decodeTimerCnt >= OPENTHERM_DECODE_TIMER_THRESHOLD) {
                        if (curEdge == FALLING_EDGE) {
                            decodeData.bitStream = 0x4000;
                            decodeData.bitStream >>= 1;
                        } else {
                            decodeData.bitStream = 0x8000;
                            decodeData.bitStream >>= 1;
                        }
                        for (uint8_t i = 0; i < OPENTHERM_BYTES_NUM; i++) {
                            decodeData.data[i] = 0x00;
                        }
                        decodeState = BIT_SYNC;
                    }
                    decodeTimerCnt = 0;
                }
            }
            break;

        case BIT_SYNC:
            if (decodeTimerCnt < OPENTHERM_DECODE_TIMER_THRESHOLD) {
                break;
            }
            if (curEdge == RAISING_EDGE) {
                decodeData.bitStream |= 0x8000;
            }

            if (decodeData.bitStream == OPENTHERM_SYNC_FIELD) {
                decodeState = DATA_SYNC;
                decodeData.data[0] = decodeData.bitStream & 0xFF;
                decodeData.data[1] = (decodeData.bitStream & 0xFF00) >> 8;
                decodeData.bitIdx = 0;
                decodeData.byteIdx = OPENTHERM_SYNC_BYTES_NUM;
                decodeData.bytesNum = OPENTHERM_DATA_BYTES_NUM + OPENTHERM_SYNC_BYTES_NUM;
            } else {
                decodeData.bitStream >>= 1;
            }
            decodeTimerCnt = 0;
            break;

        case DATA_SYNC:
            if (decodeTimerCnt >= OPENTHERM_DECODE_TIMER_THRESHOLD) {
                if (curEdge == RAISING_EDGE) {
                    set_data_bit(&decodeData, 1);
                }

                decodeData.bitIdx++;

                if (decodeData.bitIdx == (OPENTHERM_BITS_IN_BYTE_NUM)) {
                    decodeData.bitIdx = 0;

                    decodeData.byteIdx++;
                    if (decodeData.byteIdx == decodeData.bytesNum) {
                        decodeData.active = 0;
                        curEdge = NONE;
                        prevEdge = NONE;
                        decodeState = DATA_READY;
                        // Data is ready
                        OPENTHERM_DataReadyCallback();
                    }
                }
                decodeTimerCnt = 0;
            }
            break;

        case DATA_READY:
            break;

        default:
            break;
    }

    prevEdge = curEdge;
    return;
}


void OPENTHERM_Encode(uint8_t *data, uint8_t size) {
    encodeData.bitIdx = 0;
    encodeData.byteIdx = 0;

    if (size > OPENTHERM_DATA_BYTES_NUM) {
        encodeData.bytesNum = OPENTHERM_DATA_BYTES_NUM + OPENTHERM_SYNC_BYTES_NUM;
    } else {
        encodeData.bytesNum = size + OPENTHERM_SYNC_BYTES_NUM;
    }

    memcpy(&encodeData.data[OPENTHERM_SYNC_BYTES_NUM], data, encodeData.bytesNum - OPENTHERM_SYNC_BYTES_NUM);
    encodeData.data[0] = OPENTHERM_SYNC_FIELD & 0xFF;
    encodeData.data[1] = (OPENTHERM_SYNC_FIELD & 0xFF00) >> 8;

    encodeTimerCnt = 0;
    virtTact = 1;
    encodeData.active = 1;
}


/*----------------------------------------------------------------------------*/
void OPENTHERM_DecodeReset() {
    decodeState = NOT_SYNC;
}

static void mp_opentherm_timer_config() {

}

static void mp_opentherm_gpio_irq_config() {
    opentherm_obj_t *self = instance;
    if (self == mp_const_none)
        return;
    gpio_isr_handler_remove(self->in);
    gpio_set_intr_type(self->in, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(self->in, opentherm_irq_handler, (void *) self);
    mp_printf(&mp_plat_print, "Done!!!\n");
}

static void mp_opentherm_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void) kind;

    opentherm_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "OpenTherm(in=%u, out=%u, timeout=%u, invert=%u)",
              mp_obj_new_int(self->in),
              mp_obj_new_int(self->out),
              mp_obj_new_int(self->timeout),
              mp_obj_new_int(self->timeout));
}

//static void mp_opentherm_deinit(opentherm_hw_obj_t *self){
//    return self;
//}

// opentherm.init(in, out, timeout, invert)
static mp_obj_t opentherm_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_in, ARG_out, ARG_timeout, ARG_invert
    };
    static const mp_arg_t allowed_args[] = {
            {MP_QSTR_in,      MP_ARG_INT,                  {.u_int = 0}},
            {MP_QSTR_out,     MP_ARG_INT,                  {.u_int = 0}},
            {MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
            {MP_QSTR_invert,  MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
    };
    mp_arg_check_num(n_args, n_kw, 2, 4, true);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    // create instance
    opentherm_obj_t *self = MP_OBJ_TO_PTR(instance);
    // parse input agguments
    if (args[ARG_in].u_int > 0) {
        self->in = args[ARG_in].u_int;
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid IN Pin"));
    }
    if (args[ARG_out].u_int > 0) {
        self->out = args[ARG_out].u_int;
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid OUT Pin"));
    }
    self->timeout = (args[ARG_timeout].u_int > 0) ? args[ARG_timeout].u_int : OT_DEFAULT_TIMEOUT;
    self->invert = (args[ARG_invert].u_int >= 0) ? args[ARG_invert].u_int : OT_DEFAULT_INVERSION;

    gpio_set_direction(self->in, GPIO_MODE_INPUT);
    gpio_set_direction(self->out, GPIO_MODE_OUTPUT);
    gpio_set_direction(self->debug, GPIO_MODE_OUTPUT);
    mp_opentherm_gpio_irq_config();
    return MP_OBJ_FROM_PTR(self);
}


static size_t write_to_buf(uint8_t *p, size_t len) {
    opentherm_obj_t *self = MP_OBJ_TO_PTR(instance);
    for (size_t i = 0; i < len; i++) {
        if (ringbuf_free(&self->rx_buf) == 0) {
            ringbuf_get(&self->rx_buf);
        }
        ringbuf_put(&self->rx_buf, p[i]);
    }
    return len;
}

// opentherm.write(bytes, list, tupple)
static mp_obj_t opentherm_write(mp_obj_t self_in, mp_obj_t data) {
    if (mp_obj_is_type(data, &mp_type_bytes)) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
        OPENTHERM_Encode(bufinfo.buf, bufinfo.len);
        return mp_obj_new_int(bufinfo.len);
    } else if (mp_obj_is_type(data, &mp_type_list) ||
               mp_obj_is_type(data, &mp_type_tuple)) {
        size_t len = 0;
        mp_obj_t *target_array_ptr = NULL;
        mp_obj_get_array(data, &len, &target_array_ptr);
        OPENTHERM_Encode(MP_OBJ_TO_PTR(target_array_ptr), len);
        return mp_obj_new_int(len);

    } else {
        mp_raise_ValueError("Wrong type of data! Avaliable bytes, list, tuple");
    }
}
// Define a Python reference to the function above.
MP_DEFINE_CONST_FUN_OBJ_2(opentherm_write_obj, opentherm_write);

// opentherm.read()
static mp_obj_t opentherm_read(mp_obj_t self_in) {
    mp_printf(&mp_plat_print, "OPENTHERM read\n");
    opentherm_obj_t *self = MP_OBJ_TO_PTR(self_in);
    size_t bytes_read = 0;
    size_t qty_of_avaliable = ringbuf_avail(&self->rx_buf);
    if (qty_of_avaliable == 0) {
        return 0;
    }
    byte buf_in[qty_of_avaliable];

    // make sure we want at least 1 char
    while ((bytes_read < qty_of_avaliable) && ringbuf_avail(&self->rx_buf)) {
        int _byte = ringbuf_get(&self->rx_buf);
        if (_byte < 0) {
            return mp_obj_new_int(0);
        }
        buf_in[bytes_read++] = (byte) _byte;
    }
    return mp_obj_new_bytes(buf_in, qty_of_avaliable);
}


MP_DEFINE_CONST_FUN_OBJ_1(opentherm_read_obj, opentherm_read);

// opentherm.any()
static mp_obj_t opentherm_any(mp_obj_t self_in) {
    opentherm_obj_t *self = MP_OBJ_TO_PTR(self_in);
    size_t available = ringbuf_avail(&self->rx_buf);
    return MP_OBJ_NEW_SMALL_INT(available);
}

MP_DEFINE_CONST_FUN_OBJ_1(opentherm_any_obj, opentherm_any);

mp_rom_map_elem_t opentherm_locals_dict_table[] = {
        {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&opentherm_write_obj)},
        {MP_ROM_QSTR(MP_QSTR_read),  MP_ROM_PTR(&opentherm_read_obj)},
        {MP_ROM_QSTR(MP_QSTR_any),   MP_ROM_PTR(&opentherm_any_obj)},
};

static MP_DEFINE_CONST_DICT(opentherm_locals_dict, opentherm_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
        opentherm_type,
        MP_QSTR_OpenTherm,
        MP_TYPE_FLAG_NONE,
        make_new, opentherm_make_new,
        print, mp_opentherm_print,
        locals_dict, &opentherm_locals_dict
);

//module function

static mp_obj_t opentherm_add_ints(mp_obj_t a_obj, mp_obj_t b_obj) {
    return mp_obj_new_int(mp_obj_get_int(a_obj) + mp_obj_get_int(b_obj));
}

// Define a Python reference to the function above.
static MP_DEFINE_CONST_FUN_OBJ_2(opentherm_add_ints_obj, opentherm_add_ints
);


// Define all attributes of the module.
// Table entries are key/value pairs of the attribute name (a string)
// and the MicroPython object reference.
// All identifiers and strings are written as MP_QSTR_xxx and will be
// optimized to word-sized integers by the build system (interned strings).
static const mp_rom_map_elem_t
        opentherm_module_globals_table[] = {
        {MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR_opentherm)},
        {MP_ROM_QSTR(MP_QSTR_OpenTherm), MP_ROM_PTR(&opentherm_type)},
        {MP_ROM_QSTR(MP_QSTR_add_ints),  MP_ROM_PTR(&opentherm_add_ints_obj)}
};

static MP_DEFINE_CONST_DICT(opentherm_module_globals, opentherm_module_globals_table
);
// Define module object.
const mp_obj_module_t opentherm_cmodule = {
        .base = {&mp_type_module},
        .globals = (mp_obj_dict_t *) &opentherm_module_globals,
};
