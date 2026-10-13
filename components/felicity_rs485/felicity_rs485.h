#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/canbus/canbus.h"

#include <cstdint>
#include <vector>

namespace esphome {
namespace felicity_rs485 {

static constexpr uint8_t FELICITY_MAX_CELLS = 20;
static constexpr uint8_t FELICITY_MAX_TEMPERATURES = 10;

struct FelicityPack {
  uint8_t address{0};

  uint16_t protocol_type{0};
  uint16_t protocol_version{0};
  uint32_t firmware_version{0};
  uint32_t hardware_version{0};
  uint16_t cell_count{0};
  uint16_t cells_raw[FELICITY_MAX_CELLS]{};
  uint16_t temperature_count{0};
  uint16_t temperatures_raw[FELICITY_MAX_TEMPERATURES]{};
  uint16_t charge_current_raw{0};
  int16_t discharge_current_raw{0};
  uint16_t voltage_raw{0};
  uint16_t soc_raw{0};
  uint32_t full_capacity_raw{0};
  uint16_t packs_online_raw{0};
  uint16_t charge_alarm_raw{0};
  uint16_t discharge_alarm_raw{0};
  uint16_t charge_protect_raw{0};
  uint16_t charge_protect_2_raw{0};
  uint16_t discharge_protect_raw{0};
  uint16_t discharge_protect_2_raw{0};
  uint32_t design_capacity_raw{0};
  uint16_t charge_voltage_limit_raw{0};
  uint16_t discharge_voltage_limit_raw{0};
  uint16_t charge_current_limit_raw{0};
  uint16_t discharge_current_limit_raw{0};
  uint16_t status_raw{0};
  uint16_t runtime_to_empty_raw{0};
  uint32_t remaining_capacity_raw{0};
  uint32_t bus_errors{0};
  uint8_t consecutive_status_failures{0};
  uint32_t telemetry_updated_at{0};
  uint32_t alarms_updated_at{0};
  uint32_t limits_updated_at{0};
  bool telemetry_valid{false};
  bool alarms_valid{false};
  bool limits_valid{false};

  sensor::Sensor *voltage_sensor{nullptr};
  sensor::Sensor *current_sensor{nullptr};
  sensor::Sensor *charge_current_sensor{nullptr};
  sensor::Sensor *discharge_current_sensor{nullptr};
  sensor::Sensor *power_sensor{nullptr};
  sensor::Sensor *soc_sensor{nullptr};
  sensor::Sensor *soh_sensor{nullptr};
  sensor::Sensor *full_capacity_sensor{nullptr};
  sensor::Sensor *design_capacity_sensor{nullptr};
  sensor::Sensor *remaining_capacity_sensor{nullptr};
  sensor::Sensor *pack_temperature_sensor{nullptr};
  sensor::Sensor *cell_count_sensor{nullptr};
  sensor::Sensor *temperature_count_sensor{nullptr};
  sensor::Sensor *min_cell_voltage_sensor{nullptr};
  sensor::Sensor *max_cell_voltage_sensor{nullptr};
  sensor::Sensor *average_cell_voltage_sensor{nullptr};
  sensor::Sensor *cell_delta_sensor{nullptr};
  sensor::Sensor *min_voltage_cell_sensor{nullptr};
  sensor::Sensor *max_voltage_cell_sensor{nullptr};
  sensor::Sensor *charge_voltage_limit_sensor{nullptr};
  sensor::Sensor *discharge_voltage_limit_sensor{nullptr};
  sensor::Sensor *charge_current_limit_sensor{nullptr};
  sensor::Sensor *discharge_current_limit_sensor{nullptr};
  sensor::Sensor *runtime_to_empty_sensor{nullptr};
  sensor::Sensor *packs_online_sensor{nullptr};
  sensor::Sensor *charge_alarm_sensor{nullptr};
  sensor::Sensor *discharge_alarm_sensor{nullptr};
  sensor::Sensor *charge_protect_sensor{nullptr};
  sensor::Sensor *charge_protect_2_sensor{nullptr};
  sensor::Sensor *discharge_protect_sensor{nullptr};
  sensor::Sensor *discharge_protect_2_sensor{nullptr};
  sensor::Sensor *status_sensor{nullptr};
  sensor::Sensor *protocol_type_sensor{nullptr};
  sensor::Sensor *protocol_version_sensor{nullptr};
  sensor::Sensor *firmware_version_sensor{nullptr};
  sensor::Sensor *hardware_version_sensor{nullptr};
  sensor::Sensor *bus_errors_sensor{nullptr};
  sensor::Sensor *cell_voltage_sensors[FELICITY_MAX_CELLS]{};
  sensor::Sensor *temperature_sensors[FELICITY_MAX_TEMPERATURES]{};

  binary_sensor::BinarySensor *online_binary_sensor{nullptr};
  binary_sensor::BinarySensor *charging_binary_sensor{nullptr};
  binary_sensor::BinarySensor *discharging_binary_sensor{nullptr};
  binary_sensor::BinarySensor *charge_enabled_binary_sensor{nullptr};
  binary_sensor::BinarySensor *discharge_enabled_binary_sensor{nullptr};
  binary_sensor::BinarySensor *fault_binary_sensor{nullptr};
};

class FelicityRS485 : public PollingComponent, public uart::UARTDevice {
 public:
  struct Block {
    const char *name;
    uint16_t start;
    uint16_t count;
  };

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_direction_pin(InternalGPIOPin *pin) { this->direction_pin_ = pin; }
  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  void set_bridge_enabled(bool enabled) { this->bridge_enabled_ = enabled; }
  void add_pack(uint8_t address);

#define FELICITY_DECLARE_SENSOR_SETTER(NAME, MEMBER) \
  void set_##NAME##_sensor(uint8_t address, sensor::Sensor *value) { \
    if (auto *pack = this->find_pack_(address)) pack->MEMBER = value; \
  }

  FELICITY_DECLARE_SENSOR_SETTER(voltage, voltage_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(current, current_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(charge_current, charge_current_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(discharge_current, discharge_current_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(power, power_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(soc, soc_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(soh, soh_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(full_capacity, full_capacity_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(design_capacity, design_capacity_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(remaining_capacity, remaining_capacity_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(pack_temperature, pack_temperature_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(cell_count, cell_count_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(temperature_count, temperature_count_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(min_cell_voltage, min_cell_voltage_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(max_cell_voltage, max_cell_voltage_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(average_cell_voltage, average_cell_voltage_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(cell_delta, cell_delta_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(min_voltage_cell, min_voltage_cell_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(max_voltage_cell, max_voltage_cell_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(charge_voltage_limit, charge_voltage_limit_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(discharge_voltage_limit, discharge_voltage_limit_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(charge_current_limit, charge_current_limit_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(discharge_current_limit, discharge_current_limit_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(runtime_to_empty, runtime_to_empty_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(packs_online, packs_online_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(charge_alarm, charge_alarm_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(discharge_alarm, discharge_alarm_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(charge_protect, charge_protect_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(charge_protect_2, charge_protect_2_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(discharge_protect, discharge_protect_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(discharge_protect_2, discharge_protect_2_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(status, status_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(protocol_type, protocol_type_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(protocol_version, protocol_version_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(firmware_version, firmware_version_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(hardware_version, hardware_version_sensor)
  FELICITY_DECLARE_SENSOR_SETTER(bus_errors, bus_errors_sensor)

#undef FELICITY_DECLARE_SENSOR_SETTER

  void set_cell_voltage_sensor(uint8_t address, uint8_t index, sensor::Sensor *value) {
    if (auto *pack = this->find_pack_(address); pack != nullptr && index < FELICITY_MAX_CELLS)
      pack->cell_voltage_sensors[index] = value;
  }
  void set_temperature_sensor(uint8_t address, uint8_t index, sensor::Sensor *value) {
    if (auto *pack = this->find_pack_(address); pack != nullptr && index < FELICITY_MAX_TEMPERATURES)
      pack->temperature_sensors[index] = value;
  }

#define FELICITY_DECLARE_BINARY_SETTER(NAME, MEMBER) \
  void set_##NAME##_binary_sensor(uint8_t address, binary_sensor::BinarySensor *value) { \
    if (auto *pack = this->find_pack_(address)) pack->MEMBER = value; \
  }

  FELICITY_DECLARE_BINARY_SETTER(online, online_binary_sensor)
  FELICITY_DECLARE_BINARY_SETTER(charging, charging_binary_sensor)
  FELICITY_DECLARE_BINARY_SETTER(discharging, discharging_binary_sensor)
  FELICITY_DECLARE_BINARY_SETTER(charge_enabled, charge_enabled_binary_sensor)
  FELICITY_DECLARE_BINARY_SETTER(discharge_enabled, discharge_enabled_binary_sensor)
  FELICITY_DECLARE_BINARY_SETTER(fault, fault_binary_sensor)

#undef FELICITY_DECLARE_BINARY_SETTER

 protected:
  FelicityPack *find_pack_(uint8_t address);
  void start_next_request_();
  void send_request_(FelicityPack &pack, const Block &block);
  bool parse_response_(FelicityPack &pack, const Block &block);
  void finish_request_(bool success);
  void apply_registers_(FelicityPack &pack, uint16_t start, const std::vector<uint16_t> &registers);
  void publish_block_(FelicityPack &pack, uint16_t start);
  void transmit_deye_frames_();
  static uint16_t crc16_(const uint8_t *data, size_t length);

  InternalGPIOPin *direction_pin_{nullptr};
  canbus::Canbus *canbus_{nullptr};
  bool bridge_enabled_{false};
  uint32_t last_can_tx_at_{0};
  uint32_t last_can_cycle_at_{0};
  uint32_t can_retry_at_{0};
  uint8_t can_frame_index_{0};
  std::vector<FelicityPack> packs_;
  std::vector<uint8_t> rx_buffer_;
  size_t pack_index_{0};
  size_t block_index_{0};
  bool busy_{false};
  bool waiting_{false};
  uint32_t request_started_at_{0};
  uint32_t last_byte_at_{0};
  uint32_t next_request_at_{0};
};

}  // namespace felicity_rs485
}  // namespace esphome

