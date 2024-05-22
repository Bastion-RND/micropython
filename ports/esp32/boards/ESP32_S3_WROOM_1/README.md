# Build customized version of micropython (v1.22.2)

---

## 1. Import idf.py

```bash
$ source ~/esp/esp-idf/export.sh
```

## 2. Clean first

```bash
$ make BOARD=ESP32_S3_WROOM_1 clean
```

## 3. Make submodules

```bash
$ make BOARD=ESP32_S3_WROOM_1 submodules
```


## 4. Make firmware...

```bash
$ make BOARD=ESP32_S3_WROOM_1 
```

## 5. Get file 
File **firmware.bin** is located in _ports/esp32/build-ESP32_S3_WROOM_1/_

## 6. flash (somewhere)

```powershell
esptool.py --chip esp32s3 --port %COM_PORT% erase_flash
esptool.py --chip esp32s3 --port %COM_PORT% -b 921600 write_flash -z 0 .\docs\micropython\firmware.bin
```