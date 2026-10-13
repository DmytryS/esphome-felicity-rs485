# Felicity Solar → Deye: connection example

For CAN layouts, source PDFs, observed behavior, and unresolved protocol
questions, start with the [Deye CAN protocol reference](deye-can-protocol.md).

## Recommended first: use the battery's native Deye protocol

Felicity documents a direct Deye RS485 mode (`12`) and a Deye CAN mode (`00`). If the exact FLB48100WG10H firmware exposes that protocol, the simplest path is **battery BMS → Deye BMS 485/CAN port**, with the Deye set to lithium protocol `12` for RS485. In that setup the ESP32 is not an in-line translator. Felicity's guide lists Deye SUN-SG03LP1 models among the supported inverter families, but check the battery's own manual/firmware and exact inverter suffix before changing settings. [Felicity: Deye communication guide](https://us.felicitysolar.com/how-to-communicate-with-deye-inverter/) [Felicity: RS485 guide](https://www.felicityess.com/au/download/felicity-battery-use-rs485-to-communicate-with-deye/)

The Deye SUN-6K-SG03LP1 manual identifies `BMS 485/CAN` as the battery communication port; it is different from the `RS485/METER` port. Use the exact pinout/cable diagram for your inverter revision and battery port. Do not rely on generic RJ45 orientation or assume that the two vendors number connector pins from the same viewing side. [Deye SUN-6K-SG03LP1 manual, port definitions](https://deps.ua/downloads/Documentation/Deye/SUN-6K-SG03LP1-EU/Deye_SUN-6K-SG03LP1-EU_User-manual_EN.pdf)

```text
Felicity battery BMS communication interface  ─── correct vendor cable ───  Deye BMS 485/CAN
                                                                                 Lithium protocol 12 (RS485)

WeAct ESP32 / RS485 reader  ─── battery's separate RS485 interface (for Home Assistant telemetry)
```

For monitoring alongside the battery's native inverter connection, use
[`felicity-to-deye.yaml`](../examples/felicity-to-deye.yaml) with `deye_can_bridge: false`
and `can_diagnostics: false`. Leave `can_diagnostic_send_test: false` and
do not connect the ESP32 CAN interface to the inverter. All configured
per-battery sensors remain available in this monitoring-only mode.

The ESPHome config actively polls the battery. Do **not** connect it in parallel to the same RS485 pair used by the inverter: both would initiate requests on that bus. If the battery's `PCS` and `RS485` sockets are separate interfaces, use only the socket/protocol combination specified by Felicity for that exact battery; do not bridge the connectors together. Confirm Deye communication from its Li-BMS/battery status page before treating the link as operational.

## Experimental ESPHome bridge

The example now loads `../components` from this checkout so it includes the
latest local fixes without depending on a GitHub release. For a local build,
add `wifi_ssid` and `wifi_password` to `examples/secrets.yaml`, then run
`uvx --from esphome esphome compile examples/felicity-to-deye.yaml` from the
repository root. The configured flash size is 8 MB, matching the tested WeAct.

The example YAML files are single files that load the `felicity_rs485` component from GitHub, so nothing needs copying besides the YAML itself. See [Adding to ESPHome inside Home Assistant](#adding-to-esphome-inside-home-assistant).

### Viewing battery data

This is a combined example: RS485 battery monitoring and Deye CAN translation
run on the same ESP32. Add the device through Home Assistant's ESPHome
integration, or open `http://<device-ip>/` for its local web page on port 80.
The device IP is also exposed as an ESPHome text sensor. The example web
server has no authentication; use it only on a trusted LAN, not the Internet.

Each battery package enables all scalar sensor fields and binary flags
currently exposed by the component, plus the 16 cell voltages read by the
current poll and all ten temperature-probe slots:

- Voltage, net/charge/discharge current, power, SOC and SOH.
- Full, design and remaining capacity; pack temperature and probe readings.
- Cell count, individual cell voltages, minimum/maximum/average voltage,
  cell voltage difference, and the lowest/highest-voltage cell numbers.
- Charge/discharge voltage and current limits; runtime and raw pack count.
- All six raw alarm/protection words, raw status, online/fault flags,
  charging/discharging activity and charge/discharge permissions.
- Protocol type/version, actual battery firmware/hardware versions and bus errors.

This means all **currently exposed** data, not every undocumented BMS register.
Absent probes remain unknown; temperatures are probe readings, not necessarily
one per cell. Runtime registers 117-119 are currently logged as probes only,
so runtime remains unknown and remaining capacity uses the existing estimate
`full capacity × SOC / 100`. Design capacity is published only if the BMS
returns a nonzero value. SOH uses the native BMS reading when available (the
component also has a capacity-ratio fallback before native data is available).
Cell slots 17-20 exist in the schema but are not polled by this implementation.
Do not interpret an unknown value as zero or as a healthy battery.

Many fields are classified as diagnostic entities in Home Assistant.
Check each battery's Online sensor before trusting displayed readings: a
disconnected battery can leave its last sensor values visible. Deye still
receives aggregate bank frames, not these individual cell/probe entities.
Adding another battery package adds the same complete set for that address.

### Choosing the number of batteries

Each connected battery needs its RS485 address in `pack_addresses` and its own group of `felicity_rs485` sensors and binary sensors (the entries with `address: N`). The examples include addresses 1 and 2.

For one battery, remove `"2"` from `pack_addresses` and delete the `address: 2` entries. For more, add the address to `pack_addresses` and copy an `address: 2` group, changing the address and the `Battery 2` names. Addresses must be unique but need not be consecutive.

The bridge aggregates every configured pack: capacity-weighted SOC/SOH,
summed currents and live current limits, mean voltage/temperature, and
charge/discharge permissions requiring every pack to allow that direction.
The identity frame's capacity is now the sum of native pack capacities,
not a fixed 200 Ah. For identical 100 Ah packs reporting 50 A limits,
one/two/three packs advertise 100/200/300 Ah and 50/100/150 A limits.

This is configured-bank support, not automatic discovery or an unlimited
hardware guarantee. Configure only physically present, compatible parallel
packs and observe the battery manufacturer's maximum parallel count.
All configured packs must answer with fresh data; a missing pack blocks
the bridge rather than being silently excluded. Large banks must complete
refreshes within the existing 60-second freshness limit. Polling is serial,
and an update interval is skipped while the preceding poll is still active.
CAN fields saturate at 6553.5 Ah for capacity and 3276.7 A for each current
limit rather than overflowing. These are encoding ceilings, not supported
installation ratings. No per-module Deye display frames are added here.

If native RS485 mode `12` does not work and you specifically want ESPHome to emulate a Deye-compatible battery, the conceptual wiring is:

```text
Battery BMS RS485 ── A/B ── WeAct RS485 transceiver ── ESP32 ── WeAct CAN transceiver ── CAN-H/L ── Deye BMS 485/CAN port
```

### Connector wiring (RJ45 pin numbers)

| Link | Connector | RJ45 pins | Connects to |
| --- | --- | --- | --- |
| Felicity battery → ESP | Battery **PCS** port | **7, 8** (RS485) | WeAct onboard RS485 terminal (A/B) |
| ESP → Deye inverter | Deye **BMS 485/CAN** port | **4, 5** (CAN) | WeAct CAN terminal (CAN-H/CAN-L) |

- Use the battery's **PCS** port for the RS485 link, not its dedicated `RS485` port or the LINK ports.
- On the Deye, use the **BMS 485/CAN** port (not `RS485/METER`), pins 4 and 5 for CAN.
- Check each port's own pin diagram: the two vendors may not number pins from the same side.
- This table gives the pin pair, not which pin is A/B or CAN-H/CAN-L. If there is no response, power down and swap the pair before changing anything else. Do not connect any battery or inverter supply pin to the board.
- The inverter's separate Modbus telemetry link uses an external RS485 adapter on GPIO16/GPIO18; see [`deye-rs485.md`](deye-rs485.md).

On the WeAct CAN485 DevBoard V1, the documented RS485 pins are RX GPIO21, TX GPIO22, DE GPIO17; CAN uses RX GPIO26 and TX GPIO27. The CAN connector is the *physical CAN interface*—it does not make the ESP32 speak Deye's battery protocol by itself. [WeActStudio board pinout](https://github.com/WeActStudio/WeActStudio.CAN485DevBoardV1_ESP32)

For this CAN topology, Felicity identifies Deye CAN mode `00`; that differs from native Deye RS485 mode `12`. The example [`felicity-to-deye.yaml`](../examples/felicity-to-deye.yaml) polls all configured Felicity addresses and emits CAN frames at 1-second intervals only after fresh telemetry, limits, alarm registers, and native SOH/capacity have been received from every configured pack. It sends `0x351` (limits), `0x355` (SOC/SOH), `0x356` (voltage/current/temperature), `0x359` (alarms), `0x35C` (charge/discharge permissions), `0x35E` (identity), and `0x363` (emulated version). The current identity is `DY001`, manufacturer profile code `3` (EVE 100 Ah), and nominal system capacity summed from the packs' native capacity registers. The manufacturer code is an emulated Deye profile, not verification of the Felicity cell maker. Version bytes `00 01` target the Deye display label `0001`; this is the bridge's synthetic version, not the actual battery firmware version. This is still an incomplete, experimental Deye PCS implementation. [Deye PCS protocol V3.3](https://github.com/Psynosaur/esphome-deye-bms/blob/a60a71d6bd619c1f107649b9a3329bcea627d2ed/PCS%20CAN-Bus-protocol-DY-low-voltage%20V3.3.zh-CN.en.pdf)

The USB-only configuration has been flashed and CAN transmission verified on the tested board; this Wi-Fi/API example uses the same local component. First verify battery telemetry and the live charge/discharge limits in Home Assistant, then confirm their display on the Deye. If RS485 polling becomes stale or a configured battery stops answering, CAN transmission stops rather than repeating the last known limits. The current aggregate model supports a configured bank of parallel packs: capacity-weighted SOC, summed current and current limits, and the lowest reported charge and discharge voltage limits. Full fault behavior is not yet validated.

SOH now comes from the native Felicity register `0x0211` in 0.1% units.
Each battery's sensor retains this precision; the CAN aggregate is weighted
by native pack capacity and rounded to a whole percent. Thus 99% and 100%
on equal-capacity packs produce 99.5%, transmitted as 100%.

Each battery exposes six raw alarm/protection bitmasks, a combined `Fault`
indicator, online status, charge/discharge permissions, and bus-error count.
These are Felicity readings, not a readback of Deye's screen. CAN diagnostics
and received `0x305` heartbeat logging are enabled; synthetic test frames are
disabled. The `0x359` metadata bug is corrected: a healthy bank sends eight
zero bytes. Translation of real nonzero faults is still incomplete; see
[`deye-alarm-debug.md`](deye-alarm-debug.md) for the exact limitation. The
`0x35C` frame preserves charge/discharge permissions and leaves force-charge
requests clear.

## Safe test order

1. Keep the Deye on its existing battery communication wiring and confirm whether it already shows valid BMS data with Felicity's native protocol mode.
2. Keep ESPHome connected only to the separate battery RS485 interface while collecting Home Assistant data.
3. If native communication does not work, test the experimental bridge in [`felicity-to-deye.yaml`](../examples/felicity-to-deye.yaml). Verify every value and limit shown by the Deye against the battery/inverter before relying on it. A production bridge must fail safely on stale/missing data and must never report greater charge/discharge limits than the battery bank's live BMS limits.

## Adding to ESPHome inside Home Assistant

Each of these is a single self-contained file. Only the `felicity_rs485` component is fetched, from this GitHub repository (branch `main`):

| File | Does |
| --- | --- |
| [`felicity-battery-monitor.yaml`](../examples/felicity-battery-monitor.yaml) | Reads the Felicity packs only; no CAN, no bridging |
| [`felicity-deye-bridge-only.yaml`](../examples/felicity-deye-bridge-only.yaml) | Reads the packs and bridges to the Deye over CAN |
| [`felicity-deye-bridge-inverter.yaml`](../examples/felicity-deye-bridge-inverter.yaml) | Bridge, plus Deye inverter data/controls over Modbus (external RS485 adapter on GPIO16/18) |

1. In the ESPHome Builder add `wifi_ssid` and `wifi_password` under **Secrets**.
2. **New device → Skip**, then paste the chosen file's contents into the editor.
3. Install over USB the first time, later over Wi-Fi.

The files cover packs 1 and 2. For a single pack, delete `"2"` from `pack_addresses` and the `address: 2` sensor/binary-sensor entries. For more packs, copy the `address: 2` entries with the new address.
