import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor"]

CONF_DIRECTION_PIN = "direction_pin"
CONF_PACK_ADDRESSES = "pack_addresses"

felicity_rs485_ns = cg.esphome_ns.namespace("felicity_rs485")
FelicityRS485 = felicity_rs485_ns.class_(
    "FelicityRS485", cg.PollingComponent, uart.UARTDevice
)


def _addresses(value):
    values = cv.ensure_list(cv.int_range(min=0, max=247))(value)
    if not values:
        raise cv.Invalid("At least one pack address is required")
    if len(values) != len(set(values)):
        raise cv.Invalid("Pack addresses must be unique")
    return values


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FelicityRS485),
            cv.Required(CONF_DIRECTION_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_PACK_ADDRESSES): _addresses,
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    pin = await cg.gpio_pin_expression(config[CONF_DIRECTION_PIN])
    cg.add(var.set_direction_pin(pin))
    for address in config[CONF_PACK_ADDRESSES]:
        cg.add(var.add_pack(address))

