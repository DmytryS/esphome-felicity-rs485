#include "felicity_rs485.h"
#include "deye_bank.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <driver/twai.h>

namespace esphome {
namespace felicity_rs485 {

static const char *const TAG = "felicity_rs485";
static constexpr uint32_t RESPONSE_TIMEOUT_MS = 500;
static constexpr uint32_t FRAME_SILENCE_MS = 20;
static constexpr uint32_t INTER_FRAME_GAP_MS = 120;
static constexpr uint32_t CAN_TX_PERIOD_MS = 1000;
static constexpr uint32_t CAN_FRAME_GAP_MS = 100;
static constexpr uint32_t BRIDGE_DATA_MAX_AGE_MS = 60000;

static const FelicityRS485::Block BLOCKS[] = {
    {"identity", 1, 6},
    {"cells", 16, 17},
    {"telemetry", 37, 17},
    {"alarms", 54, 10},
    {"cell-state", 64, 11},
    {"temperature-state", 80, 6},
    {"module-state", 96, 10},
    {"limits", 112, 5},
    {"runtime-probe", 117, 1},
    {"remaining-high-probe", 118, 1},
    {"remaining-low-probe", 119, 1},
    // Native Felicity low-voltage BMS map: SOC 0x0210, SOH 0x0211,
    // pack capacity high/low words 0x0212-0x0213 (read only).
    {"native-telemetry", 0x0210, 4},
};

static bool time_reached(uint32_t target) { return static_cast<int32_t>(millis() - target) >= 0; }

static const char *twai_state_name(twai_state_t state) {
  switch (state) {
    case TWAI_STATE_STOPPED: return "STOPPED";
    case TWAI_STATE_RUNNING: return "RUNNING";
    case TWAI_STATE_BUS_OFF: return "BUS_OFF";
    case TWAI_STATE_RECOVERING: return "RECOVERING";
    default: return "UNKNOWN";
  }
}

static void publish(sensor::Sensor *sensor, float value, float min_change = 0.0001f) {
  if (sensor == nullptr)
    return;
  if (!sensor->has_state() || std::isnan(sensor->state) || std::fabs(sensor->state - value) >= min_change)
    sensor->publish_state(value);
}

static void publish(binary_sensor::BinarySensor *sensor, bool value) {
  if (sensor != nullptr && (!sensor->has_state() || sensor->state != value))
    sensor->publish_state(value);
}

static float temperature_c(uint16_t raw) {
  if (raw == 0)
    return NAN;
  if (raw > 1000)
    return raw / 10.0f - 273.15f;
  return raw - 273.15f;
}

static float version_number(uint32_t raw) {
  uint16_t major = raw >> 16;
  uint16_t minor = raw & 0xFFFF;
  return major + minor / (minor < 10 ? 10.0f : 100.0f);
}

void FelicityRS485::add_pack(uint8_t address) {
  FelicityPack pack;
  pack.address = address;
  this->packs_.push_back(pack);
}

FelicityPack *FelicityRS485::find_pack_(uint8_t address) {
  for (auto &pack : this->packs_) {
    if (pack.address == address)
      return &pack;
  }
  return nullptr;
}

void FelicityRS485::setup() {
  if (this->direction_pin_ == nullptr) {
    ESP_LOGE(TAG, "RS485 direction pin is not configured");
    this->mark_failed();
    return;
  }
  this->direction_pin_->setup();
  this->direction_pin_->digital_write(false);
  this->rx_buffer_.reserve(64);
  for (auto &pack : this->packs_)
    publish(pack.online_binary_sensor, false);
  if (this->can_diagnostics_enabled_) {
    // This project configures one ESP32 TWAI controller, which ESPHome assigns
    // controller 0; use the matching handle-less ESP-IDF diagnostics API.
    esp_err_t err = twai_reconfigure_alerts(TWAI_ALERT_ALL, nullptr);
    ESP_LOGI(TAG, "TWAI diagnostics enabled: alerts=%s", esp_err_to_name(err));
    this->can_diagnostic_started_at_ = millis();
    this->can_diagnostic_next_status_at_ = millis() + 1000;
  }
  this->set_timeout(1000, [this]() { this->update(); });
}

void FelicityRS485::dump_config() {
  ESP_LOGCONFIG(TAG, "Felicity Solar RS485:");
  LOG_PIN("  Direction pin: ", this->direction_pin_);
  ESP_LOGCONFIG(TAG, "  Packs: %u", static_cast<unsigned>(this->packs_.size()));
  for (const auto &pack : this->packs_)
    ESP_LOGCONFIG(TAG, "    Address %u", pack.address);
  ESP_LOGCONFIG(TAG, "  Poll interval: %u ms", static_cast<unsigned>(this->get_update_interval()));
  this->check_uart_settings(9600, 1, uart::UART_CONFIG_PARITY_NONE, 8);
}

void FelicityRS485::update() {
  if (this->busy_) {
    ESP_LOGW(TAG, "Previous poll cycle is still active; skipping this interval");
    return;
  }
  if (this->packs_.empty()) {
    ESP_LOGW(TAG, "No pack addresses configured");
    return;
  }
  this->pack_index_ = 0;
  this->block_index_ = 0;
  this->busy_ = true;
  this->waiting_ = false;
  this->next_request_at_ = millis();
}

void FelicityRS485::loop() {
  this->transmit_deye_frames_();

  if (this->can_diagnostics_enabled_) {
    uint32_t alerts = 0;
    if (twai_read_alerts(&alerts, 0) == ESP_OK && alerts != 0) {
      ESP_LOGI(TAG,
               "TWAI alerts=0x%05X TX_SUCCESS=%u BUS_ERROR=%u ERR_WARN=%u ERR_PASS=%u BUS_OFF=%u RX_DATA=%u",
               static_cast<unsigned>(alerts), !!(alerts & TWAI_ALERT_TX_SUCCESS),
               !!(alerts & TWAI_ALERT_BUS_ERROR), !!(alerts & TWAI_ALERT_ABOVE_ERR_WARN),
               !!(alerts & TWAI_ALERT_ERR_PASS), !!(alerts & TWAI_ALERT_BUS_OFF),
               !!(alerts & TWAI_ALERT_RX_DATA));
    }

    if (this->can_diagnostic_send_test_ && !this->can_diagnostic_test_sent_ &&
        millis() - this->can_diagnostic_started_at_ >= 1500) {
      std::vector<uint8_t> payload(8, 0);
      auto result = this->canbus_ == nullptr
                        ? canbus::ERROR_FAIL
                        : this->canbus_->send_data(0x7FF, false, false, payload);
      ESP_LOGI(TAG, "One-shot harmless CAN test frame ID=0x7FF result=%u (0 means accepted by TX queue)",
               static_cast<unsigned>(result));
      this->can_diagnostic_test_sent_ = true;
    }

    if (time_reached(this->can_diagnostic_next_status_at_)) {
      twai_status_info_t status{};
      esp_err_t err = twai_get_status_info(&status);
      if (err == ESP_OK) {
        ESP_LOGI(TAG, "TWAI status=%s pending_tx=%u pending_rx=%u TEC=%u REC=%u bus_errors=%u arb_lost=%u",
                 twai_state_name(status.state), static_cast<unsigned>(status.msgs_to_tx),
                 static_cast<unsigned>(status.msgs_to_rx), static_cast<unsigned>(status.tx_error_counter),
                 static_cast<unsigned>(status.rx_error_counter), static_cast<unsigned>(status.bus_error_count),
                 static_cast<unsigned>(status.arb_lost_count));
      } else {
        ESP_LOGW(TAG, "TWAI status query failed: %s", esp_err_to_name(err));
      }
      this->can_diagnostic_next_status_at_ = millis() + 2000;
    }
  }

  if (!this->busy_)
    return;

  if (!this->waiting_) {
    if (time_reached(this->next_request_at_))
      this->start_next_request_();
    return;
  }

  bool received = false;
  while (this->available()) {
    uint8_t value;
    if (!this->read_byte(&value))
      break;
    received = true;
    if (this->rx_buffer_.size() < 128)
      this->rx_buffer_.push_back(value);
  }
  if (received)
    this->last_byte_at_ = millis();

  auto &pack = this->packs_[this->pack_index_];
  const auto &block = BLOCKS[this->block_index_];
  if (!this->rx_buffer_.empty() && millis() - this->last_byte_at_ >= FRAME_SILENCE_MS) {
    this->finish_request_(this->parse_response_(pack, block));
  } else if (millis() - this->request_started_at_ >= RESPONSE_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Address %u %s: response timeout (%u bytes)", pack.address, block.name,
             static_cast<unsigned>(this->rx_buffer_.size()));
    this->finish_request_(false);
  }
}

void FelicityRS485::start_next_request_() {
  const size_t block_count = sizeof(BLOCKS) / sizeof(BLOCKS[0]);
  while (this->pack_index_ < this->packs_.size()) {
    if (this->block_index_ >= block_count) {
      this->block_index_ = 0;
      this->pack_index_++;
      continue;
    }
    break;
  }
  if (this->pack_index_ >= this->packs_.size()) {
    this->busy_ = false;
    ESP_LOGD(TAG, "Poll cycle complete");
    return;
  }
  this->send_request_(this->packs_[this->pack_index_], BLOCKS[this->block_index_]);
}

void FelicityRS485::send_request_(FelicityPack &pack, const Block &block) {
  while (this->available()) {
    uint8_t discard;
    this->read_byte(&discard);
  }
  this->rx_buffer_.clear();

  uint8_t request[8] = {
      pack.address,
      0x03,
      static_cast<uint8_t>(block.start >> 8),
      static_cast<uint8_t>(block.start & 0xFF),
      static_cast<uint8_t>(block.count >> 8),
      static_cast<uint8_t>(block.count & 0xFF),
      0,
      0,
  };
  uint16_t crc = crc16_(request, 6);
  request[6] = crc & 0xFF;
  request[7] = crc >> 8;

  ESP_LOGD(TAG, "TX address %u %s: start=%u count=%u", pack.address, block.name, block.start, block.count);
  this->direction_pin_->digital_write(true);
  delayMicroseconds(200);
  this->write_array(request, sizeof(request));
  this->flush();
  delayMicroseconds(200);
  this->direction_pin_->digital_write(false);

  this->request_started_at_ = millis();
  this->last_byte_at_ = this->request_started_at_;
  this->waiting_ = true;
}

bool FelicityRS485::parse_response_(FelicityPack &pack, const Block &block) {
  const auto &frame = this->rx_buffer_;
  if (frame.size() < 5) {
    ESP_LOGW(TAG, "Address %u %s: short response (%u bytes)", pack.address, block.name,
             static_cast<unsigned>(frame.size()));
    return false;
  }
  uint16_t received_crc = frame[frame.size() - 2] | (static_cast<uint16_t>(frame.back()) << 8);
  uint16_t calculated_crc = crc16_(frame.data(), frame.size() - 2);
  if (received_crc != calculated_crc) {
    ESP_LOGW(TAG, "Address %u %s: CRC mismatch (got 0x%04X expected 0x%04X)", pack.address, block.name,
             received_crc, calculated_crc);
    ESP_LOGW(TAG, "RX raw (%u bytes): %s", static_cast<unsigned>(frame.size()),
             format_hex_pretty(frame).c_str());
    return false;
  }
  if (frame[0] != pack.address) {
    ESP_LOGW(TAG, "Address %u %s: response belongs to address %u", pack.address, block.name, frame[0]);
    return false;
  }
  if (frame[1] & 0x80) {
    ESP_LOGW(TAG, "Address %u %s: exception code %u", pack.address, block.name, frame[2]);
    return false;
  }
  if (frame[1] != 0x03) {
    ESP_LOGW(TAG, "Address %u %s: unexpected function 0x%02X", pack.address, block.name, frame[1]);
    return false;
  }

  size_t data_offset = 0;
  size_t data_length = 0;
  size_t body_length = frame.size() - 4;  // after address/function and before CRC
  uint8_t count8 = frame[2];
  if ((count8 == body_length - 1 || count8 == body_length + 1) && ((body_length - 1) % 2 == 0)) {
    data_offset = 3;
    data_length = body_length - 1;
  } else if (frame.size() >= 6) {
    uint16_t count16 = (static_cast<uint16_t>(frame[2]) << 8) | frame[3];
    if (count16 * 2 == frame.size() - 6) {
      data_offset = 4;
      data_length = count16 * 2;
    }
  }
  if (data_offset == 0 || data_length / 2 != block.count) {
    ESP_LOGW(TAG, "Address %u %s: invalid payload/header (%u bytes)", pack.address, block.name,
             static_cast<unsigned>(frame.size()));
    ESP_LOGVV(TAG, "RX: %s", format_hex_pretty(frame).c_str());
    return false;
  }

  std::vector<uint16_t> registers;
  registers.reserve(block.count);
  for (size_t offset = 0; offset < data_length; offset += 2) {
    size_t index = data_offset + offset;
    registers.push_back((static_cast<uint16_t>(frame[index]) << 8) | frame[index + 1]);
  }
  ESP_LOGD(TAG, "RX address %u %s: %u registers", pack.address, block.name,
           static_cast<unsigned>(registers.size()));
  for (size_t index = 0; index < registers.size(); index++) {
    ESP_LOGD(TAG, "RX address %u register[%u]=%u (0x%04X)", pack.address,
             static_cast<unsigned>(block.start + index), registers[index], registers[index]);
  }
  if (block.start >= 117 && block.start <= 119)
    return true;  // Probe individually; these registers are undocumented on this battery firmware.
  this->apply_registers_(pack, block.start, registers);
  if (block.start == 37) {
    pack.telemetry_valid = true;
    pack.telemetry_updated_at = millis();
  } else if (block.start == 54) {
    pack.alarms_valid = true;
    pack.alarms_updated_at = millis();
  } else if (block.start == 112) {
    pack.limits_valid = true;
    pack.limits_updated_at = millis();
  }
  this->publish_block_(pack, block.start);
  return true;
}

void FelicityRS485::transmit_deye_frames_() {
  if (!this->bridge_enabled_ || this->canbus_ == nullptr || this->packs_.empty())
    return;
  const uint32_t now = millis();
  if (!time_reached(this->can_retry_at_))
    return;
  if (this->can_frame_index_ == 0 && now - this->last_can_cycle_at_ < CAN_TX_PERIOD_MS)
    return;
  if (this->can_frame_index_ != 0 && now - this->last_can_tx_at_ < CAN_FRAME_GAP_MS)
    return;

  // Every configured pack must be fresh; do not silently drop a missing pack.
  DeyeBankData bank;
  if (!aggregate_deye_bank(this->packs_, now, BRIDGE_DATA_MAX_AGE_MS,
                           FELICITY_MAX_TEMPERATURES, temperature_c, bank))
    return;

  auto write_u16 = [](std::vector<uint8_t> &data, size_t offset, double value) {
    uint16_t v = deye_unsigned_word(value);
    data[offset] = v & 0xFF;
    data[offset + 1] = v >> 8;
  };
  std::vector<uint8_t> limits(8, 0);
  write_u16(limits, 0, std::lround(bank.charge_voltage_limit * 10));
  write_u16(limits, 2, deye_current_limit_word(bank.charge_current_limit * 10));
  write_u16(limits, 4, deye_current_limit_word(bank.discharge_current_limit * 10));
  write_u16(limits, 6, std::lround(bank.discharge_voltage_limit * 10));
  std::vector<uint8_t> soc_soh(8, 0);
  write_u16(soc_soh, 0, std::lround(bank.weighted_soc / bank.total_capacity));
  // Native capacity-weighted SOH across every configured battery.
  write_u16(soc_soh, 2, bank.weighted_soh / bank.native_capacity_ah);
  std::vector<uint8_t> telemetry(8, 0);
  // Deye's 0x356 voltage field is in 0.01 V units.
  write_u16(telemetry, 0, std::lround((bank.total_voltage / this->packs_.size()) * 100.0f));
  int32_t current_deciamps = std::lround(bank.total_current * 10);
  uint16_t current_word = static_cast<uint16_t>(static_cast<int16_t>(std::max<int32_t>(-32768, std::min<int32_t>(32767, current_deciamps))));
  telemetry[2] = current_word & 0xFF;
  telemetry[3] = current_word >> 8;
  int16_t temp_degrees = static_cast<int16_t>(std::lround((bank.total_temperature / bank.temperature_count) * 10));
  telemetry[4] = static_cast<uint8_t>(temp_degrees & 0xFF);
  telemetry[5] = static_cast<uint8_t>((static_cast<uint16_t>(temp_degrees) >> 8) & 0xFF);
  // Deye PCS V3.3 0x359 uses bytes 0-6 for protection/alarm/system-error
  // tables and reserves byte 7. With the DY001 identity, do NOT put the
  // generic Pylon module count or 'P'/'N' markers in bytes 4-6: those are
  // interpreted as real faults. Module counts belong in PCS frame 0x364.
  // TODO: The existing nonzero-fault fallback below comes from the generic
  // protocol; a bit-by-bit Felicity-to-PCS fault mapping is still unverified.
  // This correction only removes the bogus metadata-derived fault bits.
  std::vector<uint8_t> alarms(8, 0);
  alarms[1] = bank.any_bms_fault ? 0x08 : 0x00;
  // 0x35C request/enable flags: preserve the BMS charge/discharge permissions
  // and leave force-charge/full-charge requests clear until Felicity's
  // semantics for those request bits are confirmed.
  std::vector<uint8_t> requests(8, 0);
  requests[0] = bank.can_enable_flags;
  // Deye PCS profile 0x35E: "DY001", manufacturer code, and aggregate
  // nominal capacity in 0.1 Ah. Use code 3 (EVE 100 Ah) as the closest
  // 100 Ah Deye profile; the Felicity cell maker has not been independently
  // verified. Nominal capacity is the sum of the native capacities of all packs.
  std::vector<uint8_t> brand{'D', 'Y', '0', '0', '1', 0x03, 0, 0};
  write_u16(brand, 6, bank.native_capacity_ah * 10);
  // Synthetic bridge version, not the Felicity BMS firmware version.
  // This Deye HMI displayed bytes EA 07 as EA07, so send 00 01 to target
  // the visible label 0001. Leave the hardware-version bytes at zero.
  std::vector<uint8_t> bms_version{0x00, 0x01, 0x00, 0x00, 0, 0, 0, 0};
  // Current tested subset of the Deye PCS profile, including the synthetic
  // version frame. Extra PCS telemetry frames are not implemented here yet.
  static constexpr uint32_t CAN_IDS[] = {0x351, 0x355, 0x356, 0x359, 0x35C, 0x35E, 0x363};
  const std::vector<uint8_t> *frames[] = {&limits, &soc_soh, &telemetry, &alarms, &requests, &brand, &bms_version};
  const auto &frame = *frames[this->can_frame_index_];
  char frame_hex[3 * 8 + 1] = {};
  for (size_t i = 0; i < frame.size() && i < 8; i++)
    snprintf(frame_hex + i * 3, sizeof(frame_hex) - i * 3, "%02X ", frame[i]);
  ESP_LOGI(TAG, "Deye CAN TX id=0x%03X data=%s", static_cast<unsigned>(CAN_IDS[this->can_frame_index_]), frame_hex);
  auto result = this->canbus_->send_data(CAN_IDS[this->can_frame_index_], false, false, frame);
  if (result != canbus::ERROR_OK) {
    ESP_LOGW(TAG, "Deye CAN TX 0x%03X failed (%d); retrying after 1s",
             static_cast<unsigned>(CAN_IDS[this->can_frame_index_]), static_cast<int>(result));
    this->last_can_tx_at_ = now;
    this->can_retry_at_ = now + CAN_TX_PERIOD_MS;
    return;
  }
  this->last_can_tx_at_ = now;
  if (this->can_frame_index_ == 0)
    this->last_can_cycle_at_ = now;
  this->can_frame_index_ = (this->can_frame_index_ + 1) % 7;
}

void FelicityRS485::finish_request_(bool success) {
  auto &pack = this->packs_[this->pack_index_];
  const auto &block = BLOCKS[this->block_index_];
  if (!success) {
    pack.bus_errors++;
    publish(pack.bus_errors_sensor, pack.bus_errors);
    if (block.start == 37) {
      pack.consecutive_status_failures++;
      if (pack.consecutive_status_failures >= 3)
        publish(pack.online_binary_sensor, false);
    }
  } else if (block.start == 37) {
    pack.consecutive_status_failures = 0;
    publish(pack.online_binary_sensor, true);
  }

  this->waiting_ = false;
  this->rx_buffer_.clear();
  this->block_index_++;
  if (this->block_index_ >= sizeof(BLOCKS) / sizeof(BLOCKS[0])) {
    this->block_index_ = 0;
    this->pack_index_++;
  }
  this->next_request_at_ = millis() + INTER_FRAME_GAP_MS;
}

void FelicityRS485::apply_registers_(FelicityPack &pack, uint16_t start,
                                      const std::vector<uint16_t> &registers) {
  auto value = [&](uint16_t address) -> uint16_t { return registers[address - start]; };
  if (start == 1) {
    pack.protocol_type = value(1);
    pack.protocol_version = value(2);
    pack.firmware_version = (static_cast<uint32_t>(value(3)) << 16) | value(4);
    pack.hardware_version = (static_cast<uint32_t>(value(5)) << 16) | value(6);
  } else if (start == 16) {
    pack.cell_count = value(16);
    // The current poll returns 16 cells, while storage reserves 20 slots.
    // Never index past the response or retain values for unreturned cells.
    for (uint8_t i = 0; i < FELICITY_MAX_CELLS; i++)
      pack.cells_raw[i] = static_cast<size_t>(i + 1) < registers.size() ? value(17 + i) : 0;
  } else if (start == 37) {
    pack.temperature_count = value(37);
    for (uint8_t i = 0; i < FELICITY_MAX_TEMPERATURES; i++)
      pack.temperatures_raw[i] = value(38 + i);
    pack.charge_current_raw = value(48);
    pack.discharge_current_raw = static_cast<int16_t>(value(49));
    pack.voltage_raw = value(50);
    pack.soc_raw = value(51);
    pack.full_capacity_raw = (static_cast<uint32_t>(value(52)) << 16) | value(53);
  } else if (start == 54) {
    pack.packs_online_raw = value(54);
    pack.charge_alarm_raw = value(55);
    pack.discharge_alarm_raw = value(56);
    pack.charge_protect_raw = value(57);
    pack.charge_protect_2_raw = value(58);
    pack.discharge_protect_raw = value(59);
    pack.discharge_protect_2_raw = value(60);
    pack.design_capacity_raw = (static_cast<uint32_t>(value(62)) << 16) | value(63);
  } else if (start == 112) {
    pack.charge_voltage_limit_raw = value(112);
    pack.discharge_voltage_limit_raw = value(113);
    pack.charge_current_limit_raw = value(114);
    pack.discharge_current_limit_raw = value(115);
    pack.status_raw = value(116);
  } else if (start == 117) {
    pack.runtime_to_empty_raw = value(117);
    pack.remaining_capacity_raw = (static_cast<uint32_t>(value(118)) << 16) | value(119);
  } else if (start == 0x0210) {
    pack.native_soc_tenths = value(0x0210);
    pack.native_soh_tenths = value(0x0211);
    pack.native_capacity_mah = (static_cast<uint32_t>(value(0x0212)) << 16) | value(0x0213);
    pack.native_status_updated_at = millis();
    pack.native_status_valid = pack.native_soh_tenths <= 1000 && pack.native_capacity_mah > 0;
  }
}

void FelicityRS485::publish_block_(FelicityPack &pack, uint16_t start) {
  if (start == 0x0210) {
    if (pack.native_status_valid)
      publish(pack.soh_sensor, pack.native_soh_tenths / 10.0f);
    return;
  }
  if (start == 1) {
    publish(pack.protocol_type_sensor, pack.protocol_type);
    publish(pack.protocol_version_sensor, pack.protocol_version);
    publish(pack.firmware_version_sensor, version_number(pack.firmware_version));
    publish(pack.hardware_version_sensor, version_number(pack.hardware_version));
    return;
  }

  if (start == 16) {
    publish(pack.cell_count_sensor, pack.cell_count);
    uint8_t count = std::min<uint16_t>(pack.cell_count, FELICITY_MAX_CELLS);
    float scale = (count > 0 && pack.cells_raw[0] > 1000) ? 0.001f : 0.1f;
    float minimum = 1000.0f;
    float maximum = 0.0f;
    float total = 0.0f;
    uint8_t valid = 0;
    uint8_t min_index = 0;
    uint8_t max_index = 0;
    for (uint8_t i = 0; i < count; i++) {
      if (pack.cells_raw[i] == 0)
        continue;
      float voltage = pack.cells_raw[i] * scale;
      publish(pack.cell_voltage_sensors[i], voltage, 0.0005f);
      total += voltage;
      valid++;
      if (voltage < minimum) {
        minimum = voltage;
        min_index = i + 1;
      }
      if (voltage > maximum) {
        maximum = voltage;
        max_index = i + 1;
      }
    }
    if (valid > 0) {
      publish(pack.min_cell_voltage_sensor, minimum, 0.0005f);
      publish(pack.max_cell_voltage_sensor, maximum, 0.0005f);
      publish(pack.average_cell_voltage_sensor, total / valid, 0.0005f);
      publish(pack.cell_delta_sensor, (maximum - minimum) * 1000.0f, 0.5f);
      publish(pack.min_voltage_cell_sensor, min_index);
      publish(pack.max_voltage_cell_sensor, max_index);
    }
    return;
  }

  if (start == 37) {
    float voltage_scale = pack.voltage_raw >= 10000 ? 0.001f : 0.1f;
    float voltage = pack.voltage_raw * voltage_scale;
    float charge_current = pack.charge_current_raw * 0.1f;
    float discharge_current = std::abs(static_cast<float>(pack.discharge_current_raw)) * 0.1f;
    float current = charge_current - discharge_current;
    float full_capacity = pack.full_capacity_raw * 0.001f;
    publish(pack.voltage_sensor, voltage);
    publish(pack.charge_current_sensor, charge_current);
    publish(pack.discharge_current_sensor, discharge_current);
    publish(pack.current_sensor, current);
    publish(pack.power_sensor, voltage * current, 0.5f);
    publish(pack.soc_sensor, pack.soc_raw);
    publish(pack.full_capacity_sensor, full_capacity);
    if (pack.remaining_capacity_raw == 0)
      publish(pack.remaining_capacity_sensor, full_capacity * pack.soc_raw / 100.0f);
    publish(pack.temperature_count_sensor, pack.temperature_count);

    uint8_t count = std::min<uint16_t>(pack.temperature_count, FELICITY_MAX_TEMPERATURES);
    float total = 0.0f;
    uint8_t valid = 0;
    for (uint8_t i = 0; i < count; i++) {
      float temperature = temperature_c(pack.temperatures_raw[i]);
      if (std::isnan(temperature))
        continue;
      publish(pack.temperature_sensors[i], temperature, 0.05f);
      if (i < 3) {
        total += temperature;
        valid++;
      }
    }
    if (valid == 0) {
      for (uint8_t i = 3; i < count; i++) {
        float temperature = temperature_c(pack.temperatures_raw[i]);
        if (!std::isnan(temperature)) {
          total += temperature;
          valid++;
        }
      }
    }
    if (valid > 0)
      publish(pack.pack_temperature_sensor, total / valid, 0.05f);
    publish(pack.charging_binary_sensor, charge_current > 0.0f);
    publish(pack.discharging_binary_sensor, discharge_current > 0.0f);
    return;
  }

  if (start == 54) {
    publish(pack.packs_online_sensor, pack.packs_online_raw);
    publish(pack.charge_alarm_sensor, pack.charge_alarm_raw);
    publish(pack.discharge_alarm_sensor, pack.discharge_alarm_raw);
    publish(pack.charge_protect_sensor, pack.charge_protect_raw);
    publish(pack.charge_protect_2_sensor, pack.charge_protect_2_raw);
    publish(pack.discharge_protect_sensor, pack.discharge_protect_raw);
    publish(pack.discharge_protect_2_sensor, pack.discharge_protect_2_raw);
    bool fault = pack.charge_alarm_raw || pack.discharge_alarm_raw || pack.charge_protect_raw ||
                 pack.charge_protect_2_raw || pack.discharge_protect_raw || pack.discharge_protect_2_raw;
    publish(pack.fault_binary_sensor, fault);
    if (pack.design_capacity_raw > 0) {
      float design_capacity = pack.design_capacity_raw * 0.001f;
      publish(pack.design_capacity_sensor, design_capacity);
      if (pack.full_capacity_raw > 0 && !pack.native_status_valid)
        publish(pack.soh_sensor, pack.full_capacity_raw * 100.0f / pack.design_capacity_raw);
    }
    return;
  }

  if (start == 112) {
    publish(pack.charge_voltage_limit_sensor, pack.charge_voltage_limit_raw * 0.1f);
    publish(pack.discharge_voltage_limit_sensor, pack.discharge_voltage_limit_raw * 0.1f);
    publish(pack.charge_current_limit_sensor, pack.charge_current_limit_raw * 0.1f);
    publish(pack.discharge_current_limit_sensor, pack.discharge_current_limit_raw * 0.1f);
    publish(pack.status_sensor, pack.status_raw);
    publish(pack.charge_enabled_binary_sensor, (pack.status_raw & 0x80) != 0);
    publish(pack.discharge_enabled_binary_sensor, (pack.status_raw & 0x40) != 0);
    return;
  }

  if (start == 117) {
    publish(pack.runtime_to_empty_sensor, pack.runtime_to_empty_raw);
    if (pack.remaining_capacity_raw > 0)
      publish(pack.remaining_capacity_sensor, pack.remaining_capacity_raw * 0.001f);
  }
}

uint16_t FelicityRS485::crc16_(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

}  // namespace felicity_rs485
}  // namespace esphome

