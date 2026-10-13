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

## Optional custom ESPHome bridge (not implemented yet)

If native RS485 mode `12` does not work and you specifically want ESPHome to emulate a Deye-compatible battery, the conceptual wiring is:

```text
Battery BMS RS485 ── A/B ── WeAct RS485 transceiver ── ESP32 ── WeAct CAN transceiver ── CAN-H/L ── Deye BMS 485/CAN port
```

On the WeAct CAN485 DevBoard V1, the documented RS485 pins are RX GPIO21, TX GPIO22, DE GPIO17; CAN uses RX GPIO26 and TX GPIO27. The CAN connector is the *physical CAN interface*—it does not make the ESP32 speak Deye's battery protocol by itself. [WeActStudio board pinout](https://github.com/WeActStudio/WeActStudio.CAN485DevBoardV1_ESP32)

For this CAN topology, Felicity identifies Deye CAN mode `00`; that differs from native Deye RS485 mode `12`. The current component only polls Felicity RS485 and publishes telemetry to ESPHome/Home Assistant. It does not transmit Deye CAN frames, answer an inverter's BMS handshake, or enforce Deye charge/discharge limits. Therefore this is a **wiring concept, not a flashable bridge config**. Do not select CAN mode `00` or connect the inverter to the ESP32 CAN port expecting it to work until a frame-level emulator is implemented and tested on your exact hardware/firmware. [Felicity protocol-mode guide](https://www.felicityess.com/demonstration-of-communication-between-lithium-battery-pack-and-inverter/)

SOH is another limitation: the two batteries returned zero in the design-capacity/SOH registers during our read test. A bridge cannot forward a real SOH value the BMS does not provide; it must leave it unavailable rather than fabricate one.

## Safe test order

1. Keep the Deye on its existing battery communication wiring and confirm whether it already shows valid BMS data with Felicity's native protocol mode.
2. Keep ESPHome connected only to the separate battery RS485 interface while collecting Home Assistant data.
3. Only consider an in-line bridge if native communication fails and the Deye/Felicity frame format is available or can be captured safely. A production bridge must fail safely on stale/missing data and must never report greater charge/discharge limits than the battery's live BMS limits.
