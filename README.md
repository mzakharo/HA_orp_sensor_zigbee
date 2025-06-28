| Supported Targets | ESP32-H2 | ESP32-C6 | ESP32-C5 |
| ----------------- | -------- | -------- | -------- |

# ORP Sensor Example

This example demonstrates how to configure a Home Automation ORP (Oxidation-Reduction Potential) sensor on a Zigbee end device using an Atlas Scientific ORP analog probe.

## Hardware Required

* One 802.15.4 enabled development board (e.g., ESP32-H2 or ESP32-C6) running this example.
* Atlas Scientific ORP analog probe
* A second board running as a Zigbee coordinator (see [HA_thermostat](../HA_thermostat/) example)

## Hardware Connections

Connect the Atlas Scientific ORP analog probe to GPIO4 (ADC1_CH3) on your ESP32 board:
- ORP probe analog output → GPIO4
- ORP probe VCC → 3.3V
- ORP probe GND → GND

## Configure the project

Before project configuration and build, make sure to set the correct chip target using `idf.py set-target TARGET` command.

## Erase the NVRAM

Before flash it to the board, it is recommended to erase NVRAM if user doesn't want to keep the previous examples or other projects stored info
using `idf.py -p PORT erase-flash`

## Build and Flash

Build the project, flash it to the board, and start the monitor tool to view the serial output by running `idf.py -p PORT flash monitor`.

(To exit the serial monitor, type ``Ctrl-]``.)

## Application Functions

- When the program starts, the board will attempt to detect an available Zigbee network every **1 second** until one is found.

```
I (420) main_task: Calling app_main()
I (440) phy: phy_version: 211,0, 5857fe5, Nov  1 2023, 11:31:09
I (440) phy: libbtbb version: ce629d6, Nov  1 2023, 11:31:19
I (450) main_task: Returned from app_main()
I (580) ESP_ZB_ORP_SENSOR: ZDO signal: ZDO Config Ready (0x17), status: ESP_FAIL
I (580) ESP_ZB_ORP_SENSOR: Initialize Zigbee stack
I (590) ESP_ORP_SENSOR_DRIVER: ORP sensor initialized - Range: 100-1000 mV, Calibration offset: 0 mV
I (600) gpio: GPIO[9]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:2
I (600) ESP_ZB_ORP_SENSOR: Device started up in  factory-reset mode
I (600) ESP_ZB_ORP_SENSOR: Start network steering
I (3720) ESP_ZB_ORP_SENSOR: ZDO signal: NLME Status Indication (0x32), status: ESP_OK
I (8730) ESP_ZB_ORP_SENSOR: Joined network successfully (Extended PAN ID: 74:4d:bd:ff:fe:60:2d:57, PAN ID: 0xbcc7, Channel:13, Short Address: 0xe6d0)
```

- If the board is on a network, it acts as a Zigbee End Device with the `Home Automation ORP Sensor` function using the Analog Input cluster

- The board updates the present value attribute of the `Analog Input` cluster based on the actual ORP sensor reading in millivolts (100-1000 mV range).

- By clicking the `BOOT` button on this board, the board will actively report the current measured ORP value to the bound device.
```
I (17800) ESP_ZB_ORP_SENSOR: Send 'report attributes' command
I (18850) ESP_ZB_ORP_SENSOR: ORP sensor value updated: 650 mV
```

## Calibration

The ORP sensor supports calibration to improve accuracy:

1. **Automatic calibration storage**: Calibration values are stored in NVS and persist across reboots
2. **Calibration range**: ±500mV offset can be applied
3. **Default calibration**: 0mV offset (no calibration)

### Setting Calibration via Console

You can set calibration programmatically by calling:
```c
orp_sensor_set_calibration(offset_mv); // offset_mv between -500 and +500
```

## Zigbee2MQTT Integration

This sensor is designed for maximum compatibility with Zigbee2MQTT and will appear as an analog input sensor with the following attributes:

- **present_value**: Current ORP reading in mV
- **min_present_value**: 100 mV
- **max_present_value**: 1000 mV
- **resolution**: 1 mV
- **engineering_units**: Millivolts (0x00A3)

### External Definition File

For optimal integration with Zigbee2MQTT, use the provided external definition file `zigbee2mqtt-definition.js`. This file enables proper reading of the genAnalogInput cluster:

1. Copy `zigbee2mqtt-definition.js` to your Zigbee2MQTT external converters directory
2. Add the file to your Zigbee2MQTT configuration:
   ```yaml
   external_converters:
     - zigbee2mqtt-definition.js
   ```
3. Restart Zigbee2MQTT
4. The sensor will appear with proper ORP measurement support

The definition exposes the ORP value as a numeric sensor with millivolt units and 1mV precision.

### Automatic Reporting

The firmware is configured for automatic reporting with the following settings:
- **Minimum reporting interval**: 1 second
- **Maximum reporting interval**: 300 seconds (5 minutes)
- **Reportable change**: 10 mV delta
- **Automatic reports**: Sent whenever ORP value changes by ±10mV or every 5 minutes maximum

This ensures zigbee2mqtt will automatically receive updates when the ORP value changes significantly, without requiring manual polling.

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-zigbee-sdk/issues) on GitHub. We will get back to you soon.

### Common Issues

1. **ADC readings seem incorrect**: Check your wiring and ensure the ORP probe is properly connected to GPIO4
2. **Calibration not working**: Ensure NVS is properly initialized and the calibration value is within ±500mV range
3. **Zigbee connection issues**: Make sure you have a Zigbee coordinator running and the device is in pairing mode
