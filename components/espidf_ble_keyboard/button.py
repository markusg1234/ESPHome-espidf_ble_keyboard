import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, CONF_ACTION
from . import espidf_ble_keyboard_ns, EspidfBleKeyboard

# Ensure the Python side knows it's both a Button and a Component
EspidfBleKeyboardButton = espidf_ble_keyboard_ns.class_(
    "EspidfBleKeyboardButton", button.Button, cg.Component
)

CONF_KEYBOARD_ID = "keyboard_id"

CONFIG_SCHEMA = button.button_schema(EspidfBleKeyboardButton).extend(
    {
        cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
        cv.Required(CONF_ACTION): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config) # This now works because of the .h change
    await button.register_button(var, config)

    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_action(config[CONF_ACTION]))