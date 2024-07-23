//
// Created by EPushkarev on 19.07.2024.
//


#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "hal/gpio_ll.h"

#include "esp_log.h"

// Include MicroPython API.
#include "py/runtime.h"
#include "py/obj.h"
// Used to get the time in the Timer class example.
#include "py/mphal.h"



#define OT_DEFAULT_TIMEOUT 100
#define OT_DEFAULT_INVERSION 0

typedef struct _opentherm_obj_t {
    mp_obj_base_t base;
    gpio_num_t in;
    gpio_num_t out;
    mp_int_t  invert;
    mp_int_t  timeout;
    int _bit_count;
    uint32_t _raw_data;
    uint32_t _last_time_of_use_wire;
} opentherm_obj_t;

//static const mp_obj_type_t socket_type;
const mp_obj_type_t opentherm_type;

static opentherm_obj_t opentherm_obj = {
        .base = {&opentherm_type},
        .in = -1,
        .out = -1,
        .timeout = 100,
        .invert = 0
};

static opentherm_obj_t *instance = &opentherm_obj;
//
//// all code executed in ISR must be in IRAM, and any const data must be in DRAM
//static void opentherm_irq_handler(void *arg) {
////    mp_printf(&mp_plat_print, "IRQ!!!\n");
//    return;
//    // make gpio handler
////    uint8_t rbuf[SOC_UART_FIFO_LEN];
////    uart_hal_context_t repl_hal = REPL_HAL_DEFN();
//    //clear irq flag
////    uart_hal_clr_intsts_mask(&repl_hal, UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT | UART_INTR_FRAM_ERR);
//    // put to buffer
////    ringbuf_put(&stdin_ringbuf, rbuf[i]);
//}
//
//static void mp_opentherm_irq_config(opentherm_hw_obj_t *self ){
//    if (self == mp_const_none)
//        return;
//    mp_printf(&mp_plat_print, "self is not none\n");
//    mp_printf(&mp_plat_print, "delete handler..");
//    gpio_isr_handler_remove(self->in);
//    mp_printf(&mp_plat_print, "Done!\n");
//    mp_printf(&mp_plat_print, "Config triger...");
//    gpio_set_intr_type(self->in, GPIO_INTR_POSEDGE || GPIO_INTR_NEGEDGE);
//    mp_printf(&mp_plat_print, "Done!!!\n");
//    mp_printf(&mp_plat_print, "Add handler...");
//    gpio_isr_handler_add(self->in, opentherm_irq_handler, (void *)self);
//    mp_printf(&mp_plat_print, "Done!!!\n");
//
//}

static void mp_opentherm_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void) kind;

    opentherm_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "OpenTherm(in=%d, out=%d, timeout=%d, invert=%d)",
              mp_obj_new_int(self->in),
              mp_obj_new_int(self->out),
              mp_obj_new_int(self->timeout),
              mp_obj_new_int(self->timeout))
              ;
}

//static void mp_opentherm_deinit(opentherm_hw_obj_t *self){
//    return self;
//}

// opentherm.init(in, out, timeout, invert)
static mp_obj_t opentherm_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_in, ARG_out, ARG_timeout, ARG_invert};
    mp_printf(&mp_plat_print, "Init arg table\n ");
    static const mp_arg_t allowed_args[] = {
            { MP_QSTR_in, MP_ARG_INT, {.u_int = 0} },
            { MP_QSTR_out, MP_ARG_INT, {.u_int = 0} },
            { MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
            { MP_QSTR_invert, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
    };
    mp_printf(&mp_plat_print, "Check qty of args...");
    mp_arg_check_num(n_args, n_kw, 2, 4, true);
    mp_printf(&mp_plat_print, "Done\n ");

    mp_printf(&mp_plat_print, "Init array for agrs...");
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_printf(&mp_plat_print, "Done\n ");

    mp_printf(&mp_plat_print, "parse arg...");

    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    mp_printf(&mp_plat_print, "Done\n ");
    // create instance
    mp_printf(&mp_plat_print, "create instance...");
    opentherm_obj_t  *self = MP_OBJ_TO_PTR(instance);
    mp_printf(&mp_plat_print, "Done\n ");


    // parse input agguments
    mp_printf(&mp_plat_print, "Validate args\n");
    if (args[ARG_in].u_int > 0)
    {
        self->in = args[ARG_in].u_int;
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid IN Pin"));
    }
    if (args[ARG_out].u_int > 0)
    {
        self->out = args[ARG_out].u_int;
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid OUT Pin"));
    }
    self->timeout = (args[ARG_timeout].u_int > 0) ? args[ARG_timeout].u_int: OT_DEFAULT_TIMEOUT;
    self->invert = (args[ARG_invert].u_int >= 0) ? args[ARG_invert].u_int : OT_DEFAULT_INVERSION;
    mp_printf(&mp_plat_print, "Done\n ");

//    mp_printf(&mp_plat_print, "Config IRQ\n");
//    mp_opentherm_irq_config(self);
    mp_printf(&mp_plat_print, "OPENTHERM inited\n");
//    mp_opentherm_print(self);
    return MP_OBJ_FROM_PTR(self);
}

//MP_DEFINE_CONST_FUN_OBJ_KW(opentherm_make_new_obj, 2, opentherm_make_new);

// opentherm.write(u32)
static mp_obj_t opentherm_write(mp_obj_t self, mp_obj_t data) {
    (void) self;
    uint32_t _byte = mp_obj_get_int(data);
//    mp_printf(&mp_plat_print,
//              "OT W\n 0: %X\n 1: %X\n 2: %X\n 3: %X\n",
//              (_byte >> 24 & 0xFF),
//              (_byte >> 16 & 0xFF),
//              (_byte >> 8 & 0xFF),
//              (_byte >> 0 & 0xFF)
//              );
    return mp_obj_new_int(0);
}
// Define a Python reference to the function above.
MP_DEFINE_CONST_FUN_OBJ_2(opentherm_write_obj, opentherm_write);

// opentherm.read()
static mp_obj_t opentherm_read() {
    mp_printf(&mp_plat_print, "OPENTHERM read\n");
    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_1(opentherm_read_obj, opentherm_read);

// opentherm.any()
static mp_obj_t opentherm_any() {
    mp_printf(&mp_plat_print, "OPENTHERM any\n");
    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_1(opentherm_any_obj, opentherm_any);

mp_rom_map_elem_t opentherm_locals_dict_table[] = {
{ MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&opentherm_write_obj) },
{ MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&opentherm_read_obj) },
{ MP_ROM_QSTR(MP_QSTR_any), MP_ROM_PTR(&opentherm_any_obj) },
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
        {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_opentherm)},
        {MP_ROM_QSTR(MP_QSTR_OpenTherm), MP_ROM_PTR(&opentherm_type)},
        {MP_ROM_QSTR(MP_QSTR_add_ints), MP_ROM_PTR(&opentherm_add_ints_obj)}
};

static MP_DEFINE_CONST_DICT(opentherm_module_globals, opentherm_module_globals_table
);
// Define module object.
const mp_obj_module_t opentherm_cmodule = {
        .base = {&mp_type_module},
        .globals = (mp_obj_dict_t * ) & opentherm_module_globals,
};

// Register the module to make it available in Python.
MP_REGISTER_MODULE(MP_QSTR_opentherm, opentherm_cmodule);

//// configure the pin for gpio
//esp_rom_gpio_pad_select_gpio(index);
//gpio_set_level(index, mp_obj_is_true(args[ARG_value].u_obj));
//gpio_set_direction(index, pin_io_mode);
//gpio_pulldown_en(index); // FIXME: CHECK IN TEST
//gpio_pulldown_dis(index);