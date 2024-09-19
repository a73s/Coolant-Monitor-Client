# Coolant Monitor Client
This is a work in progress firmware for ESP32. The device connects to a server and sends data from a temperature, pressure, and flow sensor.

## Building
Install the ESP-IDF build system at [the espressif website](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)

```bash
get_idf
git clone <repository link>
cd <repository folder>
idf.py flash monitor
```
