# Deye low-voltage CAN protocol: reference and bridge notes

Last reviewed: 2026-10-13. This is the starting point for Deye protocol work
in this repository. It separates protocol documentation, observed behavior,
and current implementation limitations. It is not a claim of complete
Deye battery emulation.

## Sources and where to look

| Source | Use |
| --- | --- |
| [Deye PCS CAN protocol V3.3, pinned PDF](https://github.com/Psynosaur/esphome-deye-bms/blob/a60a71d6bd619c1f107649b9a3329bcea627d2ed/PCS%20CAN-Bus-protocol-DY-low-voltage%20V3.3.zh-CN.en.pdf) | Primary protocol document mirrored by the community; use this for the `DY001` PCS profile. PDF pages 4-5: alarm tables; 5-7: main frames; later pages: per-module frames. |
| [Deye generic low-voltage CAN protocol V1.0, 2022-04-02](https://www.i4wifi.cz/uploads/faq/2003_1_deye-lv-can-communication-protocol-2-.pdf) | Older Pylon-compatible layout. Its `0x359` layout differs from PCS; do not combine the two profiles. |
| [YamBMS CAN implementation](https://github.com/Sleeper85/esphome-yambms/blob/main/packages/yambms/yambms_canbus.yaml) | Secondary implementation reference. Search for `Deye PCS`. Its PCS alarm implementation is explicitly incomplete; it is not an authoritative fault mapping. |
| [WeAct board documentation](https://github.com/WeActStudio/WeActStudio.CAN485DevBoardV1_ESP32) | Board pinout and hardware. |

The PCS PDF is an English machine translation. Its cover says V3.2, but
the filename and revision history include V3.3 dated 2024-05-25. Its printed
page totals are inconsistent. Read the tables visually: plain-text PDF
extraction previously scrambled field ordering, and greyed-out legacy bits
can look like active fields in extracted text.

Relevant project files:

- [Bridge implementation](../components/felicity_rs485/felicity_rs485.cpp): `transmit_deye_frames_()`; bank aggregation is in [`deye_bank.h`](../components/felicity_rs485/deye_bank.h).
- [Wi-Fi/Home Assistant example](../examples/felicity-to-deye.yaml), with local component source.
- [Connection and installation guide](felicity-to-deye.md).
- [False-alarm investigation and USB verification](deye-alarm-debug.md).

Local bench configuration: `felicity-deye-usb-test.yaml` at the repository
root. Older Felicity/Voltronic research is under `protocol-research/`, which
is gitignored and must not be assumed to exist in another checkout.

## Link and framing

The tested setup is two Felicity FLB48100WG10H packs, a WeAct CAN485
DevBoard V1, and a Deye SUN-6K-SG03LP1. The inverter was set to lithium CAN
mode `00`, using its BMS RS485/CAN port, not the meter port.

| Interface | Configuration used here |
| --- | --- |
| Battery RS485 | 9600 baud, 8N1; TX GPIO22, RX GPIO21, direction GPIO17; addresses 1 and 2 |
| Deye CAN | 500 kbit/s, standard 11-bit IDs; TX GPIO27, RX GPIO26; normal mode |
| CAN timing | Target cycle 1 second, approximately 100 ms between frames |
| Numeric fields | Little-endian unless the field is ASCII or explicitly described otherwise; signed values use two's complement |

RJ45 pin numbers and connector viewing orientation are not established by
this note. Use the exact inverter revision's wiring diagram.

The inverter sends `0x305` heartbeats. All-zero payloads were observed;
the PCS document also describes time fields. This is separate from the
CAN controller's hardware ACK/TX-success indication. Receiving `0x305`
and successful transmission verify bus activity, not that every field is
interpreted correctly or that the inverter's alarm screen is clear.

## Frames implemented by this bridge

Byte offsets below are zero-based. All seven transmitted frames currently
have an eight-byte payload.

| CAN ID | Fields | Current bridge behavior |
| --- | --- | --- |
| `0x351` | Bytes 0-1 charge voltage (0.1 V); 2-3 charge current limit (0.1 A); 4-5 discharge current limit (0.1 A); 6-7 discharge voltage (0.1 V) | Limits derived from all configured packs. PCS describes these as off-grid limits; positive signed current limits are capped at 3276.7 A. |
| `0x355` | Bytes 0-1 SOC (1%); 2-3 SOH (1%) | Capacity-weighted aggregates; remaining bytes zero. |
| `0x356` | Bytes 0-1 voltage (0.01 V); 2-3 signed current (0.1 A); 4-5 signed temperature (0.1 C) | Mean pack voltage, summed current, mean accepted temperatures; remaining bytes zero. See current-direction caveat below. |
| `0x359` | PCS protection/alarm/system-error tables in bytes 0-6; byte 7 unused | Eight zeros when all source alarm/protection registers are zero. Active-fault mapping is incomplete. |
| `0x35C` | Byte 0 permission/request bits; other bytes zero | Bit 7 charge allowed, bit 6 discharge allowed, ANDed across every configured pack. Force/full-charge requests are not asserted. |
| `0x35E` | Bytes 0-4 `DY001`; byte 5 manufacturer profile; bytes 6-7 nominal capacity (0.1 Ah) | `DY001` and EVE 100 Ah profile code 3 remain fixed; capacity is summed from all packs' native capacity registers, capped at 6553.5 Ah. Two 100 Ah packs still encode `D0 07`. |
| `0x363` | Software and hardware version fields | `00 01 00 00 00 00 00 00`, targeting displayed version `0001`. Synthetic bridge identity, not Felicity firmware. |

`DY001` enables Deye-style identification in the observed setup. Code `3`
does not establish the actual cell manufacturer in the Felicity packs.
Recognition by the inverter does not make this a complete native Deye BMS.

### Current direction: unresolved mismatch

PCS V3.3 specifies discharge-positive and charge-negative current in
`0x356`. The current implementation computes charge minus discharge,
so it sends discharge as negative. The inverter screenshots showed negative
current, but a screenshot alone does not validate the required protocol sign.
Compare a known charge/discharge condition against the inverter and battery
before changing it. This documentation task does not modify firmware.

### SOC, SOH, limits, and freshness

- The bridge supports any nonempty configured address list; it does not
  automatically discover batteries. Each example package adds one address
  and its sensors. See [battery-count configuration](felicity-to-deye.md#choosing-the-number-of-batteries).
- SOC is weighted by the reported full-capacity registers.
- Native Felicity `0x0211` supplies SOH in 0.1% units. Native capacity at
  `0x0212-0x0213` supplies the weighting. Per-pack ESPHome sensors retain
  decimal SOH; CAN rounds the bank result to one percent. The observed
  99.0% and 100.0% for equal-capacity packs yield 99.5%, transmitted as 100%.
- Charge/discharge current limits are summed from each pack's live limits,
  not obtained by multiplying one fixed limit by a detected count. The
  observed per-pack limits were 50 A each, producing 100 A for the bank.
- The implementation selects the lower charge voltage and the lower
  discharge voltage. Do not describe both as automatically the stricter
  limits: discharge-cutoff aggregation needs separate review.
- CAN transmission is gated on fresh telemetry, limits, alarms, and native
  SOH/capacity from every configured pack. The current stale-data threshold is 60 seconds;
  three consecutive status failures also gate transmission. A failed gate
  stops transmission rather than repeating old data. The inverter's actual
  timeout response has not been fully validated. Polling remains sequential,
  so practical bank size is limited by the battery hardware and completing
  refreshes within that timeout, not just by the address range.

## Critical difference: `0x359` is profile-dependent

| Byte | Generic low-voltage/Pylon-compatible profile | Deye PCS V3.3 |
| --- | --- | --- |
| 0-1 | Alarm flags | Protection tables 1-2 |
| 2-3 | Warning flags | Protection tables 3-4 |
| 4 | Parallel module count | Table 5, voltage/current/temperature alarms |
| 5 | ASCII `P` (`50`) | Table 6, additional alarms/heating faults |
| 6 | ASCII `N` (`4E`) | Table 7, system errors |
| 7 | Unused | Unused |

The old payload `00 00 00 00 4E 50 02 00` was wrong for PCS: it asserted
alarm bits despite healthy source registers. Even rearranging the generic
module-count and `P/N` fields would not fix the profile mismatch. PCS module
counts belong in `0x364`, not `0x359`.

After removing those metadata bytes on 2026-10-13, live USB logs showed
`0x359 = 00 00 00 00 00 00 00 00`, successful transmissions, and zero
CAN error counters. Both packs' source registers 55-60 were zero.
The inverter-screen result after that correction has not yet been confirmed
in the recorded conversation. Do not claim that `0x004F` or `0x000C` was
conclusively decoded or cleared on the screen.

### Real fault handling still needs work

The component exposes raw Felicity charge/discharge alarm and protection
registers (55-60) plus a combined `Fault` binary sensor. The combined flag
currently means any of those raw words is nonzero; it is not a fully decoded
diagnosis, and some source bits can represent status rather than faults.

The existing CAN fallback sets byte 1 bit 3 whenever any of those raw words
is nonzero. That bit means cell-temperature difference protection in PCS,
not generic system error. It is not an accurate general fault translator.
The metadata correction deliberately left this existing fallback unchanged.
Implement and test a real bit-by-bit mapping before relying on specific
Deye fault descriptions. Do not solve false alarms by unconditionally
clearing actual battery faults.

Do not confuse Deye inverter fault-history codes such as `F14` with Li-BMS
hexadecimal alarm words or raw Felicity bitmasks. There is no verified
readback of the inverter's displayed alarm words through the current CAN
receiver; it logs heartbeats and bus activity.

## Additional PCS frames documented but not implemented

| CAN ID | Meaning in the PCS document |
| --- | --- |
| `0x361` | Max/min cell voltage (1 mV) and max/min temperature (0.1 C) |
| `0x364` | Byte 0 normal modules; byte 1 charge-blocked; byte 2 discharge-blocked; byte 3 disconnected; byte 4 parallel module count |
| `0x371` | On-grid charge/discharge current limits (0.1 A, signed words) |
| `0x110 + (address - 1)` | Per-module fault/status information; consult the version-specific tables |
| `0x150 + (address - 1)` | Per-module voltage, current, SOC and SOH; SOC/SOH use 0.1%, unlike aggregate `0x355` |
| `0x200 + (address - 1)` | Per-module cell voltage and temperature extrema |
| `0x500 + (address - 1)` | Per-module software/boot version information |

Consult the PDF for the complete frame set. Do not infer individual-battery
display support merely from transmitting aggregate frames or `DY001`.

## Version display observation

Sending `EA 07` in the software-version field produced `EA07` on this HMI.
Consequently the current payload uses `00 01` to target the literal display
`0001`. This is an observed display convention, not evidence that every CAN
numeric field should change endianness. The final `0001` screen display is
still awaiting explicit user confirmation. An eight-digit date such as
`20261013` has not been shown to fit this version field.

## Build and investigation checklist

Use the existing UV-managed ESPHome environment; the lack of an `esphome`
executable on PATH does not mean ESPHome is unavailable. The successful
project builds in this session used ESPHome 2026.6.5.

```sh
cd /Users/dima/Projects/esphome-felicity
uvx --offline --from esphome esphome compile felicity-deye-usb-test.yaml
uvx --offline --from esphome esphome logs felicity-deye-usb-test.yaml --device /dev/cu.usbmodem5B910109821
```

The serial path is specific to the observed laptop/board. Stop an existing
serial monitor before an upload. For the Wi-Fi example, supply its secrets
and compile `examples/felicity-to-deye.yaml`; never flash a verification
build containing placeholder Wi-Fi credentials.

When resuming debugging, record the exact firmware/config, source battery
alarm words, complete CAN payloads, controller errors, and the inverter's
screen separately. Check the profile before interpreting bits. Keep source
facts, implementation behavior, and observed screen results distinct.
