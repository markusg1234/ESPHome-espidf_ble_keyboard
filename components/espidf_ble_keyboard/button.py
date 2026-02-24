import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, CONF_ACTION
from . import espidf_ble_keyboard_ns, EspidfBleKeyboard

# Link to the C++ class
EspidfBleKeyboardButton = espidf_ble_keyboard_ns.class_(
    "EspidfBleKeyboardButton", button.Button, cg.Component
)

CONF_KEYBOARD_ID = "keyboard_id"

# In ESPHome 2026+, we use button_schema(Class) instead of BUTTON_SCHEMA.extend
CONFIG_SCHEMA = button.button_schema(EspidfBleKeyboardButton).extend(
    {
        cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
        cv.Required(CONF_ACTION): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    # Register common button and component properties
    await cg.register_component(var, config)
    await button.register_button(var, config)

    # Link the button to the main keyboard component
    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_action(config[CONF_ACTION]))