// Can be set by mpconfigboard.cmake.

#ifndef MICROPY_HW_BOARD_NAME

#define FORK_VERSION_MAJOR 0
#define FORK_VERSION_MINOR 2
#define FORK_VERSION_MICRO 1
#define FORK_VERSION_SUFFIX "beta"

#define STR(x) #x

#define FORK_VERSION(a, b, c, d) "v" STR(a) "." STR(b) "." STR(c) "-" d

#define MICROPY_HW_BOARD_NAME "FORK " FORK_VERSION( \
    FORK_VERSION_MAJOR,                             \
    FORK_VERSION_MINOR,                             \
    FORK_VERSION_MICRO,                             \
    FORK_VERSION_SUFFIX)
#endif

#define MICROPY_HW_MCU_NAME "IPROTO ESP32-S3-WROOM1"

#define MICROPY_PY_MACHINE_DAC (0)

#define MICROPY_PY_NETWORK_LAN (1)
#define MICROPY_PY_MACHINE_CDC (1)
//#define MICROPY_PY_MACHINE_CAN (1)


// Enable UART REPL for modules that have an external USB-UART and don't use native USB.
#define MICROPY_HW_ENABLE_UART_REPL (1)

#define MICROPY_HW_I2C0_SCL (9)
#define MICROPY_HW_I2C0_SDA (8)
