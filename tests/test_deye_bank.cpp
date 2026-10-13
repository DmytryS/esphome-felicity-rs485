#include "../components/felicity_rs485/deye_bank.h"

#include <array>
#include <cassert>
#include <iostream>
#include <vector>

using namespace esphome::felicity_rs485;

// Register-level fixtures use the same fields consumed by the live aggregator.
struct Pack {
  bool telemetry_valid{true}, alarms_valid{true}, limits_valid{true}, native_status_valid{true};
  uint8_t consecutive_status_failures{0};
  uint32_t telemetry_updated_at{100}, alarms_updated_at{100}, limits_updated_at{100}, native_status_updated_at{100};
  uint32_t full_capacity_raw{100000}, native_capacity_mah{100000};
  uint16_t soc_raw{80}, native_soh_tenths{990}, voltage_raw{530};
  uint16_t charge_current_raw{0};
  int16_t discharge_current_raw{20};
  uint16_t charge_current_limit_raw{500}, discharge_current_limit_raw{500};
  uint16_t charge_voltage_limit_raw{576}, discharge_voltage_limit_raw{480}, status_raw{0xC0};
  uint16_t temperature_count{10};
  std::array<uint16_t, 10> temperatures_raw{25, 25, 25, 25, 25, 25, 25, 25, 25, 25};
  uint16_t charge_alarm_raw{0}, discharge_alarm_raw{0}, charge_protect_raw{0};
  uint16_t charge_protect_2_raw{0}, discharge_protect_raw{0}, discharge_protect_2_raw{0};
};

bool aggregate(const std::vector<Pack> &packs, DeyeBankData &bank, uint32_t now = 1000) {
  return aggregate_deye_bank(packs, now, 60000, 10, [](uint16_t raw) { return double(raw); }, bank);
}

int main() {
  DeyeBankData bank;
  assert(!aggregate({}, bank));
  // One pack used to be rejected; SOH must not index a nonexistent second pack.
  for (size_t count : {1, 2, 3, 16, 32, 248}) {
    std::vector<Pack> packs(count);
    assert(aggregate(packs, bank));
    assert(std::abs(bank.native_capacity_ah - 100.0 * count) < 0.001);
    assert(std::abs(bank.charge_current_limit - 50.0 * count) < 0.001);
    assert(std::abs(bank.discharge_current_limit - 50.0 * count) < 0.001);
    assert(std::abs(bank.total_current + 2.0 * count) < 0.001);
    assert(deye_unsigned_word(bank.weighted_soc / bank.total_capacity) == 80);
    assert(deye_unsigned_word(bank.weighted_soh / bank.native_capacity_ah) == 99);
    assert(bank.temperature_count == count * 10);  // Regression for uint8_t overflow.
    assert(bank.total_temperature / bank.temperature_count == 25);
    assert(!bank.any_bms_fault && bank.can_enable_flags == 0xC0);
  }

  // Mixed sizes and unequal current limits: weight capacity, sum actual limits.
  std::vector<Pack> packs(3);
  packs[0].soc_raw = 20;
  packs[0].native_soh_tenths = 800;
  packs[1].soc_raw = 80;
  packs[1].native_soh_tenths = 1000;
  packs[1].full_capacity_raw = packs[1].native_capacity_mah = 200000;
  packs[1].charge_current_limit_raw = 300;
  packs[2].soc_raw = 100;
  packs[2].native_soh_tenths = 900;
  packs[2].discharge_current_limit_raw = 200;
  packs[2].charge_voltage_limit_raw = 560;
  packs[2].status_raw = 0x40;
  packs[2].discharge_protect_raw = 1;
  assert(aggregate(packs, bank));
  assert(deye_unsigned_word(bank.weighted_soc / bank.total_capacity) == 70);
  assert(deye_unsigned_word(bank.weighted_soh / bank.native_capacity_ah) == 93);
  assert(deye_unsigned_word(bank.native_capacity_ah * 10) == 4000);
  assert(bank.charge_current_limit == 130 && bank.discharge_current_limit == 120);
  assert(bank.charge_voltage_limit == 56);
  assert(bank.can_enable_flags == 0x40 && bank.any_bms_fault);

  // A missing/stale/invalid pack anywhere must block the whole bank.
  for (size_t index = 0; index < packs.size(); index++) {
    auto bad = packs;
    bad[index].native_status_valid = false;
    assert(!aggregate(bad, bank));
    bad = packs;
    bad[index].consecutive_status_failures = 3;
    assert(!aggregate(bad, bank));
    bad = packs;
    bad[index].native_capacity_mah = 0;
    assert(!aggregate(bad, bank));
    bad = packs;
    bad[index].native_soh_tenths = 1001;
    assert(!aggregate(bad, bank));
    for (auto timestamp : {&Pack::telemetry_updated_at, &Pack::alarms_updated_at,
                           &Pack::limits_updated_at, &Pack::native_status_updated_at}) {
      bad = packs;
      bad[index].*timestamp = uint32_t(1000 - 60001);
      assert(!aggregate(bad, bank));
    }
  }
  // millis() wraparound must preserve the existing unsigned age semantics.
  packs = std::vector<Pack>(1);
  packs[0].telemetry_updated_at = packs[0].alarms_updated_at =
      packs[0].limits_updated_at = packs[0].native_status_updated_at = UINT32_MAX - 500;
  assert(aggregate(packs, bank, 100));

  // Protocol field limits: do not wrap current limits negative or capacity to zero.
  assert(deye_current_limit_word(40000) == 32767);
  assert(deye_current_limit_word(-10) == 0);
  assert(deye_unsigned_word(1e12) == 65535);
  assert(deye_unsigned_word(99.5) == 100);
  std::cout << "Bank aggregation: 1/2/3/16/32/248 packs, weighting, freshness, permissions and bounds passed\n";
}
