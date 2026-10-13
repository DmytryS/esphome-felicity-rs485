# Felicity Solar → Deye: connection example

## Recommended first: use the battery's native Deye protocol

Felicity documents a direct Deye RS485 mode (`12`) and a Deye CAN mode (`00`). If the exact FLB48100WG10H firmware exposes that protocol, the simplest path is **battery BMS → Deye BMS 485/CAN port**, with the Deye set to lithium protocol `12` for RS485. In that setup the ESP32 is not an in-line translator. Felicity's guide lists Deye SUN-SG03LP1 models among the supported inverter families, but check the battery's own manual/firmware and exact inverter suffix before changing settings. [Felicity: Deye communication guide](https://us.felicitysolar.com/how-to-communicate-with-deye-inverter/) [Felicity: RS485 guide](https://www.felicityess.com/au/download/felicity-battery-use-rs485-to-communicate-with-deye/)

The Deye SUN-6K-SG03LP1 manual identifies `BMS 485/CAN` as the battery communication port; it is different from the `RS485/METER` port. Use the exact pinout/cable diagram for your inverter revision and battery port. Do not rely on generic RJ45 orientation or assume that the two vendors number connector pins from the same viewing side. [Deye SUN-6K-SG03LP1 manual, port definitions](https://deps.ua/downloads/Documentation/Deye/SUN-6K-SG03LP1-EU/Deye_SUN-6K-SG03LP1-EU_User-manual_EN.pdf)

```text
Felicity battery BMS communication interface  ─── correct vendor cable ───  Deye BMS 485/CAN
                                                                                 Lithium protocol 12 (RS485)

WeAct ESP32 / RS485 reader  ─── battery's separate RS485 interface (for Home Assistant telemetry)
```

The ESPHome config in [`homeassistant.yaml`](homeassistant.yaml) actively polls the battery. Do **not** connect it in parallel to the same RS485 pair used by the inverter: both would initiate requests on that bus. If the battery's `PCS` and `RS485` sockets are separate interfaces, use only the socket/protocol combination specified by Felicity for that exact battery; do not bridge the connectors together. Confirm Deye communication from its Li-BMS/battery status page before treating the link as operational.

## Experimental ESPHome bridge

If native RS485 mode `12` does not work and you specifically want ESPHome to emulate a Deye-compatible battery, the conceptual wiring is:

```text
Battery BMS RS485 ── A/B ── WeAct RS485 transceiver ── ESP32 ── WeAct CAN transceiver ── CAN-H/L ── Deye BMS 485/CAN port
```

On the WeAct CAN485 DevBoard V1, the documented RS485 pins are RX GPIO21, TX GPIO22, DE GPIO17; CAN uses RX GPIO26 and TX GPIO27. The CAN connector is the *physical CAN interface*—it does not make the ESP32 speak Deye's battery protocol by itself. [WeActStudio board pinout](https://github.com/WeActStudio/WeActStudio.CAN485DevBoardV1_ESP32)

For this CAN topology, Felicity identifies Deye CAN mode `00`; that differs from native Deye RS485 mode `12`. The example [`felicity-to-deye.yaml`](felicity-to-deye.yaml) is an experimental transmitter. It polls both Felicity addresses and emits Deye CAN frames at 1-second intervals only after fresh telemetry, limits, and alarm registers have been received from both packs. It sends the Deye 0x351 (limits), 0x355 (SOC/SOH), 0x356 (voltage/current/temperature), 0x359 (alarms), and 0x35E (brand) frames. The source standard is 500 kbit/s, 11-bit IDs, little-endian fields, with an interval of at least 500 ms. [Deye low-voltage CAN protocol](https://www.i4wifi.cz/uploads/faq/2003_1_deye-lv-can-communication-protocol-2-.pdf) [Felicity protocol-mode guide](https://www.felicityess.com/demonstration-of-communication-between-lithium-battery-pack-and-inverter/)

This is not yet validated against the user's exact Deye inverter firmware. First verify battery telemetry and the live charge/discharge limits in Home Assistant. Then flash this example with the Deye on CAN protocol `00`, confirm BMS communication and plausible values on the Deye battery page, and keep an eye on operation before relying on it. If RS485 polling becomes stale or either battery stops answering, CAN transmission stops rather than repeating the last known limits. The current aggregate model assumes two packs in parallel: capacities-weighted SOC, summed current and current limits, and the stricter of the two voltage limits.

SOH is a limitation: the two batteries returned zero in the design-capacity/SOH registers during our read test. The bridge sends SOH as zero (unknown), not a fabricated value. Deye display/acceptance behavior for this value is unverified. It also asserts only the generic Deye system-error alarm when Felicity reports any raw alarm/protection; the individual Felicity alarm bits have not yet been mapped to Deye's specific alarm/warning bits. It does not send the Deye 0x35C full-charge-enable frame because its semantics are not established for these batteries.

## Safe test order

1. Keep the Deye on its existing battery communication wiring and confirm whether it already shows valid BMS data with Felicity's native protocol mode.
2. Keep ESPHome connected only to the separate battery RS485 interface while collecting Home Assistant data.
3. If native communication does not work, test the experimental bridge in [`felicity-to-deye.yaml`](felicity-to-deye.yaml). Verify every value and limit shown by the Deye against the battery/inverter before relying on it. A production bridge must fail safely on stale/missing data and must never report greater charge/discharge limits than the battery bank's live BMS limits.
