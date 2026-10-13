import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ADDRESS,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import FelicityRS485

CONF_FELICITY_RS485_ID = "felicity_rs485_id"

FLAGS = {
    "online": ("set_online_binary_sensor", DEVICE_CLASS_CONNECTIVITY),
    "charging": ("set_charging_binary_sensor", None),
    "discharging": ("set_discharging_binary_sensor", None),
    "charge_enabled": ("set_charge_enabled_binary_sensor", None),
    "discharge_enabled": ("set_discharge_enabled_binary_sensor", None),
    "fault": ("set_fault_binary_sensor", DEVICE_CLASS_PROBLEM),
}


def _schema(device_class):
    kwargs = {"entity_category": ENTITY_CATEGORY_DIAGNOSTIC}
    if device_class is not None:
        kwargs["device_class"] = device_class
    return binary_sensor.binary_sensor_schema(**kwargs)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FELICITY_RS485_ID): cv.use_id(FelicityRS485),
        cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=247),
        **{cv.Optional(key): _schema(value[1]) for key, value in FLAGS.items()},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_FELICITY_RS485_ID])
    address = config[CONF_ADDRESS]
    for key, (setter_name, _) in FLAGS.items():
        if key not in config:
            continue
        value = await binary_sensor.new_binary_sensor(config[key])
        cg.add(getattr(hub, setter_name)(address, value))

