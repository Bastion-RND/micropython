//
// Created by EPushkarev on 19.07.2024.
//

// Include MicroPython API.
#include "py/runtime.h"

// Used to get the time in the Timer class example.
#include "py/mphal.h"

typedef struct _opentherm_hw_obj_t {
    mp_obj_base_t base;
    gpio_num_t in: 8;
    gpio_num_t out: 8;
    int _bit_count;
    uint32_t _raw_data;
    uint32_t _last_time_of_use_wire;
} opentherm_hw_obj_t;

#define OT_NUM_MAX 1

static opentherm_hw_obj_t opentherm_hw_obj[OT_NUM_MAX];

static void opentherm_init(opentherm_hw_obj_t *self, uint32_t freq, uint32_t timeout_us, bool first_init) {
    if (!first_init) {
        i2c_driver_delete(self->port);
    }
    i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = self->sda,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = self->scl,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = freq,
    };
    i2c_param_config(self->port, &conf);
    int timeout = I2C_SCLK_FREQ / 1000000 * timeout_us;
    i2c_set_timeout(self->port, (timeout > I2C_LL_MAX_TIMEOUT) ? I2C_LL_MAX_TIMEOUT : timeout);
    i2c_driver_install(self->port, I2C_MODE_MASTER, 0, 0, 0);
}

// This is the function which will be called from Python as cexample.add_ints(a, b).
static mp_obj_t opentherm_add_ints(mp_obj_t a_obj, mp_obj_t b_obj) {
// Extract the ints from the micropython input objects.
    int a = mp_obj_get_int(a_obj);
    int b = mp_obj_get_int(b_obj);
// Calculate the addition and convert to MicroPython object.
    return mp_obj_new_int(a + b);
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
MP_REGISTER_MODULE(MP_QSTR_opentherm, opentherm_cmodule
);
