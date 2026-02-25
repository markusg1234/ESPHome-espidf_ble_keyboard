import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID
from . import espidf_ble_keyboard_ns, EspidfBleKeyboard

DEPENDENCIES = ["espidf_ble_keyboard"]

EspidfBleKeyboardButton = espidf_ble_keyboard_ns.class_(
    "EspidfBleKeyboardButton", button.Button
)

CONF_ACTION = "action"
CONF_KEYBOARD_ID = "keyboard_id"

CONFIG_SCHEMA = button.button_schema(EspidfBleKeyboardButton).extend({
    cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
    cv.Required(CONF_ACTION): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_action(config[CONF_ACTION]))