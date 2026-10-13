# Deye inverter monitoring over RS485

[`examples/deye-rs485.yaml`](../examples/deye-rs485.yaml) is a standalone,
read-only Modbus RTU reader for the SUN-6K-SG03LP1 register map supplied with
this project. It uses standard ESPHome components, not an external component.
It requests holding registers (function 03); it does not change inverter
settings or synchronize its clock.

This is **not** the Felicity battery protocol or the Deye CAN battery-emulation
protocol. The register addresses/scalings are based on the user's supplied
configuration, not independently verified against this inverter's firmware.
Successful compilation does not prove that a connected inverter answers them.

## Hardware and provisional wiring

The example uses ESP32 **GPIO16/GPIO18**, corresponding to physical positions
**15/16 on the WeAct V1.1 FPC breakout**. These are not Deye RJ45 contacts:

| ESP32 connection | External RS485 adapter |
| --- | --- |
| Breakout position 15: GPIO16, configured TX | DI / logic input from ESP32 |
| Breakout position 16: GPIO18, configured RX | RO / logic output to ESP32 |
| Logic ground | Adapter logic ground, as required by its design |

Confirm the adapter model, TX/RX assignment, power requirements and direction
control before flashing. Its logic interface must be compatible with ESP32
3.3 V inputs; a generic 5 V MAX485 board may not be. An isolated adapter is
preferable for an inverter installation. Never connect A/B directly to GPIOs
or connect battery/inverter supply voltage to the ESP32.

The default assumes automatic transmit-direction control. For a manual
DE/RE adapter, a third, verified spare GPIO is needed; wire DE and active-low
RE as required by the module and set `uart.flow_control_pin` to that GPIO.
Do not reuse GPIO17 from the existing bridge: it controls the WeAct's onboard
RS485 transceiver. GPIO16/18 here drive an **additional external adapter**, not
the onboard RS485 terminal. See [ESPHome Modbus direction control](https://esphome.io/components/modbus/).

GPIO16 and GPIO18 are listed as expansion GPIOs on this WeAct board. These
assignments do not overlap the TF-card, Felicity UART/direction or Deye CAN
pins. Follow the breakout's printed position numbers, not an assumed
left-to-right counting direction.

Serial logging remains disabled with `logger.baud_rate: 0`; view ESPHome logs
over Wi-Fi instead. GPIO16/18 do not share the UART0 logging/programming
pins. Disconnect the adapter from the inverter during USB flashing.
On the V1.1 expansion connector, physical positions 15/16 mean GPIO16/GPIO18,
not GPIO15/GPIO16. Follow GPIO labels or the exact board schematic.
See [Espressif ESP32 GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html)
and the [WeAct board documentation](https://github.com/WeActStudio/WeActStudio.CAN485DevBoardV1_ESP32).

The inverter-side port and A/B contacts still require confirmation from its
exact connector diagram. GPIO16/18 do **not** establish the inverter RJ45 pinout.
Do not assume an RS485 meter port is an inverter-data server: it may instead
be a bus on which the inverter polls a meter. Likewise, a BMS socket may use
a battery protocol rather than the inverter monitoring register map.
Use the inverter's documented monitoring interface. Do not add a second
Modbus master to a bus already driven by another controller/logger.

## Install

1. Copy `examples/deye-rs485.yaml` into your ESPHome configuration directory.
2. Confirm/edit `deye_tx_pin`, `deye_rx_pin`, and `deye_address` at the top.
   Defaults are GPIO16 TX, GPIO18 RX, Modbus address 1, 9600 baud, 8N1.
3. Add these secrets in the same directory's `secrets.yaml`:

   ```yaml
   wifi_ssid: "YOUR_WIFI"
   wifi_password: "YOUR_WIFI_PASSWORD"
   deye_api_encryption_key: "YOUR_BASE64_ENCODED_32_BYTE_KEY"
   deye_ota_password: "YOUR_UNIQUE_OTA_PASSWORD"
   ```

   Generate an API encryption key with `openssl rand -base64 32`.
   Do not copy the credentials from a publicly shared configuration; rotate
   previously shared keys/passwords if they are still in use.
4. Validate and compile through ESPHome. From this repository:

   ```sh
   uvx --from esphome esphome config examples/deye-rs485.yaml
   uvx --from esphome esphome compile examples/deye-rs485.yaml
   ```

5. After installing on the intended device, add it through Home Assistant's
   ESPHome integration. The local web page is at `http://<device-ip>/`.
   The web page has no authentication: keep it on a trusted LAN.

## Readings and differences from the supplied configuration

- Battery temperature, voltage, SOC, signed power/current and BMS current limits.
- Inverter/grid/load/AUX power, voltage/current/frequency where supplied.
- Two MPPTs: PV power, voltage/current and calculated combined solar power.
- Inverter-reported daily/lifetime battery, grid, load and PV energy counters.
- DC transformer/radiator temperatures, grid and peak-shaving flags.
- Read-only battery thresholds, configured current/power limits, mode values
  and six time slots. These are sensors, not controls.
- Serial number, MCU register values and overall state.

Settings switches, numbers, selects, hourly/manual RTC writes, fallback-AP
credentials, optional local daily-energy integration and alternative inferred
load-power formulas from the supplied config are intentionally omitted.
The inverter's native counters do not need a Home Assistant time source.
No unverified SOH or decoded inverter fault-code register has been added.

Addresses are written in decimal without leading zeroes, avoiding YAML's
legacy octal interpretation (`013`, `015`, `016` are not safe decimal spellings).
The conflicting COMM-version interpretation at register 16 is omitted; the
sample's rated-power interpretation of registers 16/17 is retained as one
low-word-first 32-bit value. Check it against the inverter's rating.

The sample's single-word grid energy totals at 78 and 81 are retained; their
width/rollover behavior is not confirmed. Check lifetime counters, grid power
sign, SOC, current limits and all scaling against the inverter screen before
using them for automations or energy accounting. The register labels L1/L2
are inherited from the supplied map, not a claim that this is a two-phase
inverter. The expected battery sign from the supplied config is positive for
discharging and negative for charging; verify it on the actual installation.

`Deye Telemetry Online` is true only while a positive battery-voltage reading
has arrived in the last 45 seconds. It starts false and is not merely the
ESP32's network status. It confirms that telemetry register only, not that
every configured range works. Other sensors can retain their last readings
after communication stops; check freshness before trusting them.

## Alongside the Felicity bridge

Flashing this standalone firmware onto the existing WeAct **replaces and
stops the Felicity-to-Deye CAN bridge**. This change only adds the example;
it does not flash hardware or modify the existing bridge configuration.

To combine them later, retain the Felicity UART on GPIO22/21 with direction
GPIO17 and CAN on GPIO27/26. Use a separate UART and external RS485 adapter
for inverter monitoring, with distinct component IDs. Do not join the two
RS485 A/B buses. The new example already uses `deye_uart`, `deye_modbus` and
`deye_inverter` IDs, but its complete node configuration is not a package to
include blindly into another full configuration.

## Troubleshooting

For USB-only bench testing, [`deye-rs485-usb-test.yaml`](../deye-rs485-usb-test.yaml)
at the repository root polls only register 183 at address 1 every five
seconds, using GPIO16 TX/GPIO18 RX, 9600 baud, 8N1. It logs raw TX/RX and
decoded voltage over UART0 USB at 115200 baud. It requires no secrets or
Wi-Fi, assumes automatic adapter direction control, and contains no CAN or
Felicity reader. Flashing it replaces the bridge; use it only when that
interruption is intended. Even a zero-voltage reply confirms a Modbus reply,
but still verify the register interpretation. TX logs alone do not prove
that bytes reached the RS485 wires or the inverter.

Bench observation on 2026-10-13: the USB test was compiled and uploaded with
flash verification. At GPIO15 TX/GPIO16 RX, address 1, 9600/8N1, the logs
showed repeated `01 03 00 B7 00 01 34 2C` requests and response timeouts.
One isolated RX byte `01` was observed, but no complete valid response.
The user confirmed this adapter
worked with the earlier supplied configuration without a DE/RE control pin.
The user subsequently identified the numbered FPC breakout: physical positions
15/16 are GPIO16/GPIO18, so the original GPIO15/GPIO16 firmware did not match
that wiring. Both configurations now use GPIO16 TX/GPIO18 RX. The earlier
timeouts are not evidence of a fault with the corrected mapping; successful
inverter communication still needs verification.

Use network logs: `esphome logs examples/deye-rs485.yaml --device <device-ip>`.
For a first link test, reduce the entity list to just battery voltage at
register 183, then add ranges back after receiving plausible values. No
response means checking the interface, A/B assignment, TX/RX, direction
control, address and baud rate—not changing inverter settings at random.
An illegal-address response can mean this firmware does not support a
configured register; adjacent sensors can share a request, so one unsupported
register can affect a whole range. A compile test cannot resolve this.

Implementation references: [ESPHome Modbus Controller](https://esphome.io/components/modbus_controller/)
and [Modbus sensor types/scaling](https://esphome.io/components/sensor/modbus_controller/).
