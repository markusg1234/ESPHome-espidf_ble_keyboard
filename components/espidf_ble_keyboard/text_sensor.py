import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import EspidfBleKeyboard

DEPENDENCIES = ["espidf_ble_keyboard"]

CONF_KEYBOARD_ID = "keyboard_id"
CONF_TYPE = "type"

TYPE_HIDDEN_BUTTONS = "hidden_buttons"
TYPE_HOST_MAC = "host_mac"

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
        cv.Optional(CONF_TYPE, default=TYPE_HIDDEN_BUTTONS): cv.one_of(
            TYPE_HIDDEN_BUTTONS, TYPE_HOST_MAC, lower=True
        ),
    }
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])

    sensor_type = config.get(CONF_TYPE, TYPE_HIDDEN_BUTTONS)
    if sensor_type == TYPE_HOST_MAC:
        cg.add(parent.set_host_mac_sensor(var))
    else:
        cg.add(parent.set_hidden_sensor(var))
