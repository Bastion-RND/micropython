set(IDF_TARGET esp32s3)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/sdkconfig.usb
    boards/sdkconfig.ble
    boards/sdkconfig.spiram_sx
    boards/sdkconfig.spiram_oct
    boards/sdkconfig.240mhz
    ${MICROPY_BOARD_DIR}/sdkconfig.board
)

 set(MICROPY_SOURCE_BOARD
    ${MICROPY_BOARD_DIR}/machine_cdc.c
 )

set(MICROPY_FROZEN_MANIFEST
    ${MICROPY_BOARD_DIR}/manifest.py
)
