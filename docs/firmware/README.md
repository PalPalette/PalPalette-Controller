# PalPalette Controller Firmware

## Web Installer (Recommended)

Visit the web installer and follow the on-screen steps:
**https://palpalette.github.io/PalPalette-Controller/**

Requires Chrome, Edge, or Opera (Web Serial API support).

## Manual Flash with esptool

```bash
esptool.py --chip esp32c3 --port /dev/ttyUSB0 --baud 460800 write_flash \
  0x0     bootloader.bin \
  0x8000  partitions.bin \
  0x10000 firmware.bin
```

Replace `/dev/ttyUSB0` with your serial port (`COM3`, etc. on Windows).

## After Flashing

The device boots into setup mode and creates a WiFi access point named **PalPalette-Setup-XXXXXX**. Connect to it and follow the captive portal to configure WiFi and register the device.
