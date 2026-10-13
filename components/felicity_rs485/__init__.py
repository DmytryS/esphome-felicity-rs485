import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import canbus
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor", "canbus"]

CONF_DIRECTION_PIN = "direction_pin"
CONF_PACK_ADDRESSES = "pack_addresses"
CONF_CANBUS_ID = "canbus_id"
CONF_DEYE_CAN_BRIDGE = "deye_can_bridge"
CONF_CAN_DIAGNOSTICS = "can_diagnostics"
CONF_CAN_DIAGNOSTIC_SEND_TEST = "can_diagnostic_send_test"

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
            cv.Optional(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
            cv.Optional(CONF_DEYE_CAN_BRIDGE, default=False): cv.boolean,
            cv.Optional(CONF_CAN_DIAGNOSTICS, default=False): cv.boolean,
            cv.Optional(CONF_CAN_DIAGNOSTIC_SEND_TEST, default=False): cv.boolean,
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
    if CONF_CANBUS_ID in config:
        canbus_var = await cg.get_variable(config[CONF_CANBUS_ID])
        cg.add(var.set_canbus(canbus_var))
    cg.add(var.set_bridge_enabled(config[CONF_DEYE_CAN_BRIDGE]))
    cg.add(var.set_can_diagnostics_enabled(config[CONF_CAN_DIAGNOSTICS]))
    cg.add(var.set_can_diagnostic_send_test(config[CONF_CAN_DIAGNOSTIC_SEND_TEST]))
    for address in config[CONF_PACK_ADDRESSES]:
        cg.add(var.add_pack(address))

