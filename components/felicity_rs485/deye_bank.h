#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace felicity_rs485 {

// Pure aggregation shared by the live bridge and host-side regression tests.
struct DeyeBankData {
  double total_capacity{0};
  double native_capacity_ah{0};
  double weighted_soc{0};
  double weighted_soh{0};
  double total_current{0};
  double charge_current_limit{0};
  double discharge_current_limit{0};
  double charge_voltage_limit{1000};
  double discharge_voltage_limit{1000};
  double total_voltage{0};
  double total_temperature{0};
  size_t temperature_count{0};
  uint8_t can_enable_flags{0xC0};
  bool any_bms_fault{false};
};

// Never round/cast an unbounded bank total to an integer first: large banks
// can exceed a CAN word. Current-limit words have a signed positive range.
inline uint16_t deye_unsigned_word(double value) {
  return static_cast<uint16_t>(std::lround(std::max(0.0, std::min(65535.0, value))));
}

inline uint16_t deye_current_limit_word(double deciamps) {
  return deye_unsigned_word(std::min(32767.0, deciamps));
}

template<typename Packs, typename TemperatureConverter>
bool aggregate_deye_bank(const Packs &packs, uint32_t now, uint32_t max_age,
                         uint16_t max_temperatures, TemperatureConverter temperature_c,
                         DeyeBankData &bank) {
  bank = {};
  if (packs.empty())
    return false;

  for (const auto &pack : packs) {
    if (!pack.telemetry_valid || !pack.alarms_valid || !pack.limits_valid ||
        pack.consecutive_status_failures >= 3 ||
        now - pack.telemetry_updated_at > max_age ||
        now - pack.alarms_updated_at > max_age ||
        now - pack.limits_updated_at > max_age || !pack.native_status_valid ||
        now - pack.native_status_updated_at > max_age)
      return false;

    const double capacity = pack.full_capacity_raw * 0.001;
    if (capacity <= 0 || pack.soc_raw > 100 ||
        pack.native_soh_tenths > 1000 || pack.native_capacity_mah == 0)
      return false;
    const double voltage = pack.voltage_raw * (pack.voltage_raw >= 10000 ? 0.001 : 0.1);
    if (voltage < 40 || voltage > 65)
      return false;

    const double native_capacity = pack.native_capacity_mah * 0.001;
    bank.total_capacity += capacity;
    bank.native_capacity_ah += native_capacity;
    bank.total_voltage += voltage;
    bank.weighted_soc += capacity * pack.soc_raw;
    bank.weighted_soh += native_capacity * (pack.native_soh_tenths * 0.1);
    bank.total_current += (pack.charge_current_raw - std::abs(static_cast<int16_t>(pack.discharge_current_raw))) * 0.1;
    bank.charge_current_limit += pack.charge_current_limit_raw * 0.1;
    bank.discharge_current_limit += pack.discharge_current_limit_raw * 0.1;
    bank.charge_voltage_limit = std::min(bank.charge_voltage_limit, pack.charge_voltage_limit_raw * 0.1);
    bank.discharge_voltage_limit = std::min(bank.discharge_voltage_limit, pack.discharge_voltage_limit_raw * 0.1);
    bank.can_enable_flags &= static_cast<uint8_t>(pack.status_raw & 0xC0);
    for (uint16_t i = 0; i < std::min<uint16_t>(pack.temperature_count, max_temperatures); i++) {
      const double temperature = temperature_c(pack.temperatures_raw[i]);
      if (std::isfinite(temperature) && temperature >= -30 && temperature <= 100) {
        bank.total_temperature += temperature;
        bank.temperature_count++;
      }
    }
    bank.any_bms_fault |= pack.charge_alarm_raw || pack.discharge_alarm_raw || pack.charge_protect_raw ||
                          pack.charge_protect_2_raw || pack.discharge_protect_raw || pack.discharge_protect_2_raw;
  }
  return bank.total_capacity > 0 && bank.native_capacity_ah > 0 && bank.temperature_count > 0 &&
         bank.charge_voltage_limit > 0 && bank.discharge_voltage_limit > 0;
}

}  // namespace felicity_rs485
}  // namespace esphome
