# Felicity Solar battery reader for ESPHome

ESPHome firmware for reading Felicity Solar 48 V batteries over the battery bank's RS485 bus. The examples default to addresses 1 and 2 and expose telemetry in Home Assistant. The Deye bridge example supports a configurable bank through one package entry per battery. An experimental, opt-in bridge can also translate the available pack data into Deye low-voltage CAN frames.

## Deye protocol documentation

Start with [Deye CAN protocol reference and implementation notes](docs/deye-can-protocol.md).
It contains the source protocol PDFs, CAN frame layouts, the PCS versus generic
alarm-format distinction, SOH/version handling, verified observations, and
remaining limitations. For wiring and installation, see the
[Felicity-to-Deye example guide](docs/felicity-to-deye.md).

For **Deye translation plus full available battery monitoring**, use
[`examples/felicity-to-deye.yaml`](examples/felicity-to-deye.yaml). Its shared
battery package enables pack readings, 16 cell voltages, temperature probes,
limits and diagnostics per configured battery. Readings are available through
Home Assistant and the device's web page on port 80; unsupported fields remain
unknown. Deye receives aggregate bank data, not individual cell entities.

## Install in Home Assistant

For **reading the inverter itself over a separate RS485 adapter**, use
[`examples/deye-rs485.yaml`](examples/deye-rs485.yaml) and its
[wiring/installation guide](docs/deye-rs485.md). It is a standalone read-only
example, not the battery bridge; it uses GPIO16 TX/GPIO18 RX (WeAct V1.1
FPC breakout positions 15/16). Check adapter direction control before flashing.
Installing it on the same ESP32 would replace the bridge firmware.

For the combined Felicity reader + Deye CAN bridge:

1. In the ESPHome Builder, add `wifi_ssid` and `wifi_password` under **Secrets**.
2. Create a new device, then paste one single-file config from [`examples/`](examples/): [`felicity-deye-bridge-only.yaml`](examples/felicity-deye-bridge-only.yaml) (bridge), [`felicity-battery-monitor.yaml`](examples/felicity-battery-monitor.yaml) (monitor only) or [`felicity-deye-bridge-inverter.yaml`](examples/felicity-deye-bridge-inverter.yaml) (bridge plus Deye inverter data). No other files need copying; the component is fetched from GitHub.
3. Adjust the pack addresses to your batteries and install on the ESP32.

The combined example enables Deye CAN translation by default. For battery
monitoring only, set `deye_can_bridge: false` and `can_diagnostics: false`,
leave `can_diagnostic_send_test: false`, and leave the ESP32 CAN connection
disconnected from the inverter. Battery readings remain available through
Home Assistant and the local web page.

The example loads the local component, so it includes unpublished fixes; it
does not fetch GitHub automatically. It uses the node name
`felicity-deye-bridge`; change it if another device already uses that name.
The component requires ESPHome 2026.4.0 or newer.

## Tested hardware and wiring

The tested reader is a WeActStudio CAN485 DevBoard V1 (ESP32). The configuration uses GPIO22 for RS485 transmit, GPIO21 for receive, and GPIO17 for transceiver direction. Connect the board's RS485 differential pair to the battery bank's dedicated `RS485` port, observing the port and board labels. If there is no response, power down before swapping the A/B pair. For the Deye bridge, connect to the battery's `PCS` port on RJ45 pins 7 and 8 (RS485), and to the Deye `BMS 485/CAN` port on pins 4 and 5 (CAN); see [docs/felicity-to-deye.md](docs/felicity-to-deye.md). Do not connect any battery supply pin to the RS485 terminals. Keep the battery LINK connection as required for the two-pack bank.

The tested bus is 9600 baud, 8 data bits, no parity, 1 stop bit. The two responding battery addresses were 1 and 2. The combined ESPHome example has a 10-second update interval.

## Data and limitations

The component exposes pack voltage, current-related values, SOC, capacity, cell voltages, temperatures, limits, status, and raw alarms when the corresponding register data is returned. The current local implementation obtains real SOH from native Felicity register `0x0211`, despite the older design-capacity registers returning zero. Some register meanings/scalings can vary by firmware, so verify values against the battery display before relying on them for control.

In monitoring-only mode the combined example only reads the batteries. The experimental bridge has been tested for telemetry transmission, but complete fault translation and inverter responses are not yet validated. Review the [protocol limitations](docs/deye-can-protocol.md#real-fault-handling-still-needs-work) before relying on its fault reporting. The combined example loads the component from this checkout; a GitHub source only receives changes after they are published.

See [`docs/felicity-to-deye.md`](docs/felicity-to-deye.md) for direct-protocol guidance, bridge behavior, and limitations.

## USB-only test configuration

[`deye-rs485-usb-test.yaml`](deye-rs485-usb-test.yaml) is the minimal inverter
RS485 bench test: GPIO16 TX/GPIO18 RX, 9600/8N1, address 1, register 183.
It shows raw requests/replies over USB, needs no Wi-Fi or secrets, and does
not include the Felicity reader or CAN bridge. See the
[inverter troubleshooting notes](docs/deye-rs485.md#troubleshooting).

[`felicity-rs485.yaml`](felicity-rs485.yaml) is the original local-component configuration for bench testing. It expects the `components` directory beside the YAML and does not enable Wi-Fi or Home Assistant API.
