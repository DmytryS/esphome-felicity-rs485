# Felicity Solar battery reader for ESPHome

ESPHome firmware for reading two Felicity Solar 48 V batteries over the battery bank's RS485 bus. It polls Modbus addresses 1 and 2 and exposes telemetry in Home Assistant. It sends read requests only; it does not write settings to the batteries or emulate a Deye battery protocol.

## Install in Home Assistant

1. Copy [`examples/homeassistant.yaml`](examples/homeassistant.yaml) into the ESPHome add-on's configuration directory.
2. Make sure that directory's `secrets.yaml` contains `wifi_ssid` and `wifi_password`.
3. Open the YAML in the ESPHome dashboard and install it on the ESP32.

The example fetches this repository's `felicity_rs485` component automatically. You do not need to copy the `components` directory into Home Assistant. It uses the node name `inverterbattery`; change that name if it conflicts with another ESPHome node. The component requires ESPHome 2026.4.0 or newer.

## Tested hardware and wiring

The tested reader is a WeActStudio CAN485 DevBoard V1 (ESP32). The configuration uses GPIO22 for RS485 transmit, GPIO21 for receive, and GPIO17 for transceiver direction. Connect the board's RS485 differential pair to the battery bank's dedicated `RS485` port, observing the port and board labels. If there is no response, power down before swapping the A/B pair. Do not connect the reader to the battery's `PCS` port while testing, and do not connect any battery supply pin to the RS485 terminals. Keep the battery LINK connection as required for the two-pack bank.

The tested bus is 9600 baud, 8 data bits, no parity, 1 stop bit. The two responding battery addresses were 1 and 2. The ESPHome example polls every 30 seconds.

## Data and limitations

The component exposes pack voltage, current-related values, SOC, capacity, cell voltages, temperatures, limits, status, and alarms when the corresponding register data is returned. The observed batteries reported SOC and telemetry, but their design-capacity/SOH registers were zero. In that case the component leaves SOH unavailable; it does not infer SOH from SOC or nominal capacity. Some register meanings/scalings can vary by firmware, so verify values against the battery display before relying on them for control.

This project currently reads the batteries only. It does not pass data to the Deye inverter or make the inverter identify the batteries as Deye-compatible. Keep the inverter's existing battery communication wiring/configuration unchanged while testing this reader.

## USB-only test configuration

[`felicity-rs485.yaml`](felicity-rs485.yaml) is the original local-component configuration for bench testing. It expects the `components` directory beside the YAML and does not enable Wi-Fi or Home Assistant API.
