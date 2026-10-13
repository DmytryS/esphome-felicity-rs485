# Deye PCS alarm-frame correction

The broader [Deye CAN protocol reference](deye-can-protocol.md)
collects frame layouts, source documents, and implementation limitations.

The bridge advertises `DY001` in CAN frame `0x35E`. Its `0x359` alarm
frame must therefore follow the Deye PCS profile, not the generic
Pylon-compatible profile.

The Deye PCS V3.3 document assigns bytes 0-6 to protection/alarm/system-error
tables and reserves byte 7. In particular, bytes 4, 5, and 6 are tables 5,
6, and 7. They are not module count or ASCII identification fields.

Before the correction, the bridge sent `00 00 00 00 4E 50 02 00` even with
all Felicity alarm/protection registers clear. Those last fields asserted
PCS fault bits. The corrected healthy frame is `00 00 00 00 00 00 00 00`.
PCS module counts belong in `0x364`; this change does not add that frame.

Source: [Deye PCS CAN protocol V3.3, pages 4-5 and 7](https://github.com/Psynosaur/esphome-deye-bms/blob/a60a71d6bd619c1f107649b9a3329bcea627d2ed/PCS%20CAN-Bus-protocol-DY-low-voltage%20V3.3.zh-CN.en.pdf).

This is a focused correction for the false alarms observed with healthy
batteries. It does not validate the mapping from a particular PCS bit to
the inverter's displayed alarm word, or implement complete fault translation.
The previous generic nonzero-fault fallback remains unverified for PCS:
it sets byte 1 bit 3, which PCS calls cell-temperature difference protection,
not a generic system-error flag. Do not treat that fallback as an accurate
description of a real Felicity fault. Source registers remain available in
the ESPHome logs. A complete PCS fault mapping is still required.

Verification: compile and flash the USB test config, check the transmitted
`0x359` payload and CAN TX success, then confirm the inverter's Li-BMS alarm
display with the user. USB transmission success alone does not verify the
inverter's displayed state.

Observed on 2026-10-13 after a successful build and verified USB upload:
`0x359 = 00 00 00 00 00 00 00 00`, with `TX_SUCCESS=1`, TEC=0, REC=0,
and zero bus errors. Both packs' registers 55-60 were zero in the monitored
polls. Version frame `0x363` still sends `00 01 00 00 00 00 00 00`.
Inverter-screen confirmation is pending.
