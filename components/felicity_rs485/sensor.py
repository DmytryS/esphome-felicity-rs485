import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ADDRESS,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY_STORAGE,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HOUR,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
)

from . import FelicityRS485

CONF_FELICITY_RS485_ID = "felicity_rs485_id"

UNIT_AMPERE_HOURS = "Ah"
UNIT_MILLIVOLT = "mV"


SENSORS = [
    {"key": "voltage", "setter": "set_voltage_sensor", "unit": UNIT_VOLT, "device_class": DEVICE_CLASS_VOLTAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 3},
    {"key": "current", "setter": "set_current_sensor", "unit": UNIT_AMPERE, "device_class": DEVICE_CLASS_CURRENT, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1},
    {"key": "charge_current", "setter": "set_charge_current_sensor", "unit": UNIT_AMPERE, "device_class": DEVICE_CLASS_CURRENT, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1, "diagnostic": True},
    {"key": "discharge_current", "setter": "set_discharge_current_sensor", "unit": UNIT_AMPERE, "device_class": DEVICE_CLASS_CURRENT, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1, "diagnostic": True},
    {"key": "power", "setter": "set_power_sensor", "unit": UNIT_WATT, "device_class": DEVICE_CLASS_POWER, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 0},
    {"key": "soc", "setter": "set_soc_sensor", "unit": UNIT_PERCENT, "device_class": DEVICE_CLASS_BATTERY, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 0},
    {"key": "soh", "setter": "set_soh_sensor", "unit": UNIT_PERCENT, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1, "diagnostic": True},
    {"key": "full_capacity", "setter": "set_full_capacity_sensor", "unit": UNIT_AMPERE_HOURS, "device_class": DEVICE_CLASS_ENERGY_STORAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 2},
    {"key": "design_capacity", "setter": "set_design_capacity_sensor", "unit": UNIT_AMPERE_HOURS, "device_class": DEVICE_CLASS_ENERGY_STORAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 2, "diagnostic": True},
    {"key": "remaining_capacity", "setter": "set_remaining_capacity_sensor", "unit": UNIT_AMPERE_HOURS, "device_class": DEVICE_CLASS_ENERGY_STORAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 2},
    {"key": "temperature", "setter": "set_pack_temperature_sensor", "unit": UNIT_CELSIUS, "device_class": DEVICE_CLASS_TEMPERATURE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1},
    {"key": "cell_count", "setter": "set_cell_count_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "temperature_sensor_count", "setter": "set_temperature_count_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "min_cell_voltage", "setter": "set_min_cell_voltage_sensor", "unit": UNIT_VOLT, "device_class": DEVICE_CLASS_VOLTAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 3, "diagnostic": True},
    {"key": "max_cell_voltage", "setter": "set_max_cell_voltage_sensor", "unit": UNIT_VOLT, "device_class": DEVICE_CLASS_VOLTAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 3, "diagnostic": True},
    {"key": "average_cell_voltage", "setter": "set_average_cell_voltage_sensor", "unit": UNIT_VOLT, "device_class": DEVICE_CLASS_VOLTAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 3, "diagnostic": True},
    {"key": "cell_delta", "setter": "set_cell_delta_sensor", "unit": UNIT_MILLIVOLT, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 0, "diagnostic": True},
    {"key": "min_voltage_cell", "setter": "set_min_voltage_cell_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "max_voltage_cell", "setter": "set_max_voltage_cell_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "charge_voltage_limit", "setter": "set_charge_voltage_limit_sensor", "unit": UNIT_VOLT, "device_class": DEVICE_CLASS_VOLTAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1, "diagnostic": True},
    {"key": "discharge_voltage_limit", "setter": "set_discharge_voltage_limit_sensor", "unit": UNIT_VOLT, "device_class": DEVICE_CLASS_VOLTAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1, "diagnostic": True},
    {"key": "charge_current_limit", "setter": "set_charge_current_limit_sensor", "unit": UNIT_AMPERE, "device_class": DEVICE_CLASS_CURRENT, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1, "diagnostic": True},
    {"key": "discharge_current_limit", "setter": "set_discharge_current_limit_sensor", "unit": UNIT_AMPERE, "device_class": DEVICE_CLASS_CURRENT, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1, "diagnostic": True},
    {"key": "runtime_to_empty", "setter": "set_runtime_to_empty_sensor", "unit": "min", "device_class": DEVICE_CLASS_DURATION, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 0, "diagnostic": True},
    {"key": "packs_online", "setter": "set_packs_online_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "charge_alarm", "setter": "set_charge_alarm_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "discharge_alarm", "setter": "set_discharge_alarm_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "charge_protect", "setter": "set_charge_protect_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "charge_protect_2", "setter": "set_charge_protect_2_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "discharge_protect", "setter": "set_discharge_protect_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "discharge_protect_2", "setter": "set_discharge_protect_2_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "status", "setter": "set_status_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "protocol_type", "setter": "set_protocol_type_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "protocol_version", "setter": "set_protocol_version_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "firmware_version", "setter": "set_firmware_version_sensor", "accuracy": 2, "diagnostic": True},
    {"key": "hardware_version", "setter": "set_hardware_version_sensor", "accuracy": 2, "diagnostic": True},
    {"key": "bus_errors", "setter": "set_bus_errors_sensor", "accuracy": 0, "diagnostic": True},
    {"key": "cell_voltage", "setter": "set_cell_voltage_sensor", "unit": UNIT_VOLT, "device_class": DEVICE_CLASS_VOLTAGE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 3, "diagnostic": True, "count": 20},
    {"key": "temperature_sensor", "setter": "set_temperature_sensor", "unit": UNIT_CELSIUS, "device_class": DEVICE_CLASS_TEMPERATURE, "state_class": STATE_CLASS_MEASUREMENT, "accuracy": 1, "diagnostic": True, "count": 10},
]


def _schema(row):
    kwargs = {"accuracy_decimals": row.get("accuracy", 0)}
    for source, target in (
        ("unit", "unit_of_measurement"),
        ("device_class", "device_class"),
        ("state_class", "state_class"),
        ("icon", "icon"),
    ):
        if source in row:
            kwargs[target] = row[source]
    if row.get("diagnostic"):
        kwargs["entity_category"] = ENTITY_CATEGORY_DIAGNOSTIC
    return sensor.sensor_schema(**kwargs)


def _keys(row):
    count = row.get("count", 1)
    if count == 1:
        return [row["key"]]
    return [f"{row['key']}_{i}" for i in range(1, count + 1)]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FELICITY_RS485_ID): cv.use_id(FelicityRS485),
        cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=247),
        **{cv.Optional(key): _schema(row) for row in SENSORS for key in _keys(row)},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_FELICITY_RS485_ID])
    address = config[CONF_ADDRESS]
    for row in SENSORS:
        setter = getattr(hub, row["setter"])
        for index, key in enumerate(_keys(row)):
            if key not in config:
                continue
            sens = await sensor.new_sensor(config[key])
            if row.get("count", 1) == 1:
                cg.add(setter(address, sens))
            else:
                cg.add(setter(address, index, sens))

