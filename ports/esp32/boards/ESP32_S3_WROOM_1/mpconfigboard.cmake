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

list(APPEND MICROPY_DEF_BOARD
    MICROPY_HW_BOARD_NAME="Interstellar ESP32-S3-WROOM-1 module"
)

set(MICROPY_FROZEN_MANIFEST 
    ${MICROPY_BOARD_DIR}/manifest.py
)
