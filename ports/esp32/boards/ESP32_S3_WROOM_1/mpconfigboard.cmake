set(IDF_TARGET esp32s3)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/sdkconfig.usb
    boards/sdkconfig.ble
    ${MICROPY_BOARD_DIR}/sdkconfig.board
)

### FIXME! ----------------------------- 
# set(MICROPY_SOURCE_BOARD
#     ${MICROPY_BOARD_DIR}/machine_cdc.c
#     ${MICROPY_BOARD_DIR}/machine_can.c
# )

set(MICROPY_FROZEN_MANIFEST
    ${MICROPY_BOARD_DIR}/manifest.py
)
