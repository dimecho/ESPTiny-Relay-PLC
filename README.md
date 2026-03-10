<p align="center"><img src="Web/img/icon.png?raw=true"></p>

# ESPTiny Relay (WiFi Edition)

Automatic 8-channel relay control - PLC style.

- WiFi (Web Interface) :electric_plug: [View Demo](https://dimecho.github.io/ESPTiny-Relay-PLC/Web/index.html)
- Battery Deep Sleep (~10μA)
- Modular PCB (ESP32, DS1307, TP4056)

<p align="center">

![Photo](Web/img/photo.jpg?raw=true)

</p>

## Download

[Firmware](../../releases/download/latest/ESPTiny-Relay-Firmware.zip)

## Connect

```
    SSID: Relay
    Password: (blank)
    Interface: http://192.168.8.8
```

## Build

Sketch (Firmware)

1. Install [Arduino IDE](https://www.arduino.cc/en/main/software)
2. Arduino/File -> Preferences -> Additional Boards Manager URLs: ```https://espressif.github.io/arduino-esp32/package_esp32_index.json```
3. Tools -> Boards -> Board Manager -> esp32 -> Install
4. Tools -> Boards -> ESP32S2 Dev Module
5. Sketch -> Export compiled Binary

Additional Libraries

* https://github.com/ESP32Async/ESPAsyncWebServer
* https://github.com/ESP32Async/AsyncTCP
* https://github.com/Seeed-Studio/RTC_DS1307
* https://github.com/mobizt/ESP-Mail-Client

File System (Web Interface)

1. Run "littlefs-build-mac" (Mac) or "littlefs-build-win.ps1" (Windows) to build. LittleFS Binary: `build/flash-littlefs.bin`

**Note:** Files must be GZIP'ed. HTTP server sends compressed code to the Browser for decompression.
```
response->addHeader("Content-Encoding", "gzip");
```

Flashing Options:

1. Wireless - Web Browser [http://192.168.8.8/update](http://192.168.8.8/update)
2. USB - [Arduino LittleFS Plugin](https://github.com/lorol/arduino-esp32littlefs-plugin)

## License

[![CCSA](https://licensebuttons.net/l/by-sa/4.0/88x31.png)](https://creativecommons.org/licenses/by-sa/4.0/legalcode)