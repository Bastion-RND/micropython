# Build customized version of micropython (v1.22.2)

---

## 1. Import idf.py

```bash
$ source ~/esp/esp-idf/export.sh
```

## 2. If first time - make submodules

```bash
$ make BOARD=ESP32_S3_WROOM_1 submodules
```

## 3. If necessary - clean first

```bash
$ make BOARD=ESP32_S3_WROOM_1 clean
```

## 4. make it...

```bash
$ make BOARD=ESP32_S3_WROOM_1 
```

## 5. get file 
File **firmware.bin** is located in _ports/esp32/build-ESP32_S3_WROOM_1_

## 6. flash (somewhere)

```powershell
esptool.py --chip esp32s3 --port %COM_PORT% erase_flash
esptool.py --chip esp32s3 --port %COM_PORT% -b 921600 write_flash -z 0 .\docs\micropython\firmware.bin
```