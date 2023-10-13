#include "py/builtin.h"
#include "py/runtime.h"

#if MICROPY_PY_MACHINE_CDC

const mp_obj_type_t machine_cdc_type;

// info()
STATIC mp_obj_t py_machine_cdc_info(void)
{
    return MP_OBJ_NEW_SMALL_INT(42);
}
MP_DEFINE_CONST_FUN_OBJ_0(machine_cdc_info_obj, py_machine_cdc_info);

STATIC const mp_rom_map_elem_t mp_module_machine_cdc_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_CDC)},
    {MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&machine_cdc_info_obj)},
};
STATIC MP_DEFINE_CONST_DICT(
    mp_module_machine_cdc_globals, mp_module_machine_cdc_globals_table);

// const mp_obj_module_t mp_module_machine_cdc = {
//     .base = {&mp_type_module},
//     .globals = (mp_obj_dict_t *)&mp_module_machine_cdc_globals,
// };
// MP_REGISTER_MODULE(MP_QSTR_machine_cdc, mp_module_machine_cdc);

MP_DEFINE_CONST_OBJ_TYPE(
    machine_cdc_type,
    MP_QSTR_CDC,
    MP_TYPE_FLAG_NONE,
    locals_dict, &mp_module_machine_cdc_globals);

#endif