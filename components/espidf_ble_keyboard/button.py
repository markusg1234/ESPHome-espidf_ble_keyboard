import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID
from . import espidf_ble_keyboard_ns, EspidfBleKeyboard

DEPENDENCIES = ["espidf_ble_keyboard"]

EspidfBleKeyboardButton = espidf_ble_keyboard_ns.class_(
    "EspidfBleKeyboardButton", button.Button, cg.Component
)

CONF_ACTION = "action"
CONF_KEYBOARD_ID = "keyboard_id"


def validate_action(value):
    if isinstance(value, str):
        return value
    if isinstance(value, dict):
        action_type = value.get("type", "")
        if action_type == "combo":
            mod = value.get("modifier", 0)
            key = value.get("key", 0)
            # Accept int or hex string
            if isinstance(mod, str):
                mod = int(mod, 16) if mod.startswith("0x") else int(mod)
            if isinstance(key, str):
                key = int(key, 16) if key.startswith("0x") else int(key)
            return f"combo:{hex(mod)}:{hex(key)}"
        elif action_type == "consumer":
            code = value.get("code", 0)
            if isinstance(code, str):
                code = int(code, 16) if code.startswith("0x") else int(code)
            return f"consumer:{hex(code)}"
        raise cv.Invalid(f"Unknown action type '{action_type}'. Use 'combo' or 'consumer'.")
    raise cv.Invalid("Action must be a string or a mapping with 'type' key.")


CONFIG_SCHEMA = button.button_schema(EspidfBleKeyboardButton).extend({
    cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
    cv.Required(CONF_ACTION): validate_action,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_action(config[CONF_ACTION]))