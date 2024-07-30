//
// Created by EPushkarev on 19.07.2024.
//

#include "opentherm.h"
#include "string.h"

#define TIMER_DIVIDER  8
#define TIMER_FLAGS    0

// Base data struct storage
const mp_obj_type_t opentherm_type;
static uint8_t _rx_buf[128];
static opentherm_obj_t opentherm_obj = {
        .base = {&opentherm_type},
        .state = OT_NOT_INITIALIZED,
        .in = -1,
        .out = -1,
        .isSlave = false,
        .debug = 41,
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

static size_t write_to_buf(uint8_t *p, size_t len);


//manchester Code functions
static void set_output(uint8_t state) {
    gpio_set_level(instance->out, state);
}

/*----------------------------------------------------------------------------*/
static bool get_input() {
    return (bool) gpio_get_level(instance->in);
}

#define INACTIVE    0
#define ACTIVE      1

void sendBit(bool high) {
    set_output(high ? ACTIVE : INACTIVE);
    mp_hal_delay_us(500);
    set_output(high ? INACTIVE : ACTIVE);
    mp_hal_delay_us(500);
}

static bool isReady()
{
    return instance->state == OT_READY;
}

static void handleInterrupt()
{
    gpio_set_level(instance->debug, gpio_get_level(instance->in));
    if (isReady())
    {
        if (get_input())
        {
            instance->state = OT_RESPONSE_WAITING;
        }
        else
        {
            return;
        }
    }
    unsigned long newTs = mp_hal_ticks_cpu();
    switch (instance->state) {
        case OT_RESPONSE_WAITING:
            if (get_input())
            {
                instance->state = OT_RESPONSE_START_BIT;
                instance->responseTimestamp = newTs;
            }
            else
            {
                instance->state = OT_RESPONSE_INVALID;
                instance->responseTimestamp = newTs;
            }
            break;
        case OT_RESPONSE_START_BIT:
            if ((newTs - instance->responseTimestamp < 750) && !get_input())
            {
                instance->state = OT_RESPONSE_RECEIVING;
                instance->responseTimestamp = newTs;
                instance->responseBitIndex = 0;
            }
            else
            {
                instance->state = OT_RESPONSE_INVALID;
                instance->responseTimestamp = newTs;
            }
            break;
        case OT_RESPONSE_RECEIVING:
            if ((newTs - instance->responseTimestamp) > 750)
            {
                if (instance->responseBitIndex < 32)
                {
                    instance->response = (instance->response << 1) | !get_input();
                    instance->responseTimestamp = newTs;
                    instance->responseBitIndex++;
                }
                else
                { // stop bit
                    instance->state = OT_READY;
                    instance->response = 0;
                    instance->responseBitIndex = 0;
                    instance->responseTimestamp = newTs;
                    write_to_buf((uint8_t *)instance->response, instance->responseBitIndex/8);
                }
            }
            break;
        default:
            break;
    }

}

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"

const static char *TAG = "tusb_tsk";
static TaskHandle_t s_ot_tskh;
/**
 * @brief This top level thread processes all usb events and invokes callbacks
 */
static void ot_device_task(void *arg)
{
    mp_print_str(&mp_plat_print, "OT task started");
    while (1) { // RTOS forever loop
        if(mp_hal_ticks_cpu() - instance->responseTimestamp >= 40000){
            instance->response = 0;
            instance->responseBitIndex = 0;
            instance->state = OT_READY;
            instance->responseTimestamp = mp_hal_ticks_cpu();
//            write_to_buf((uint8_t *)instance->response, instance->responseBitIndex/8);
        }
    }
}

esp_err_t ot_run_task(void)
{
    // This function is not garanteed to be thread safe, if invoked multiple times without calling `tusb_stop_task`, will cause memory leak
    // doing a sanity check anyway
    ESP_RETURN_ON_FALSE(!s_ot_tskh, ESP_ERR_INVALID_STATE, TAG, "OT main task already started");
    // Create a task for tinyusb device stack:
    xTaskCreate(ot_device_task, "OpenTherm", 4096, NULL, 5, &s_ot_tskh);
    ESP_RETURN_ON_FALSE(s_ot_tskh, ESP_FAIL, TAG, "create OT main task failed");
    return ESP_OK;
}

esp_err_t ot_stop_task(void)
{
    ESP_RETURN_ON_FALSE(s_ot_tskh, ESP_ERR_INVALID_STATE, TAG, "OT main task not started yet");
    vTaskDelete(s_ot_tskh);
    s_ot_tskh = NULL;
    return ESP_OK;
}

static void mp_opentherm_gpio_irq_config() {
    opentherm_obj_t *self = instance;
    if (self == mp_const_none)
        return;
    gpio_isr_handler_remove(self->in);
    gpio_set_intr_type(self->in, GPIO_INTR_POSEDGE | GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(self->in, handleInterrupt, (void *) self);
    mp_printf(&mp_plat_print, "IRQ config Done!!!\n");
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
//
// from machine import Pin
// import opentherm
//d = Pin(41, Pin.OUT)
//o = opentherm.OpenTherm(Pin(7, Pin.IN), Pin(15,Pin.OUT))
// opentherm.init(in, out, timeout, invert)
static mp_obj_t opentherm_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_in, ARG_out, ARG_timeout, ARG_invert
    };
    static const mp_arg_t allowed_args[] = {
            {MP_QSTR_in,      MP_ARG_OBJ,                  {.u_obj = mp_const_none}},
            {MP_QSTR_out,     MP_ARG_OBJ,                  {.u_obj = mp_const_none}},
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
    if (args[ARG_in].u_obj != mp_const_none) {
        self->in = machine_pin_get_id(args[ARG_in].u_obj);
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid IN Pin"));
    }
    if (args[ARG_in].u_obj != mp_const_none) {
        self->out = machine_pin_get_id(args[ARG_out].u_obj);
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid OUT Pin"));
    }
    self->timeout = (args[ARG_timeout].u_int > 0) ? args[ARG_timeout].u_int : OT_DEFAULT_TIMEOUT;
    self->invert = (args[ARG_invert].u_int >= 0) ? args[ARG_invert].u_int : OT_DEFAULT_INVERSION;
//    ot_run_task();
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
//        OPENTHERM_Encode(bufinfo.buf, bufinfo.len);
        return mp_obj_new_int(bufinfo.len);
    } else if (mp_obj_is_type(data, &mp_type_list) ||
               mp_obj_is_type(data, &mp_type_tuple)) {
        size_t len = 0;
        mp_obj_t *target_array_ptr = NULL;
        mp_obj_get_array(data, &len, &target_array_ptr);
//        OPENTHERM_Encode(MP_OBJ_TO_PTR(target_array_ptr), len);
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
