set(IDF_TARGET esp32s3)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/sdkconfig.usb
    boards/sdkconfig.ble
    boards/sdkconfig.240mhz
    boards/sdkconfig.spiram_sx
    boards/sdkconfig.spiram_oct
    boards/ESP32_S3_WROOM_1/sdkconfig.board
)
