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
        elif action_type == "mouse_click":
            buttons = value.get("buttons", 1)
            if isinstance(buttons, str):
                buttons = int(buttons, 16) if buttons.startswith("0x") else int(buttons)
            return f"mouse_click:{hex(buttons)}"
        elif action_type == "mouse_move":
            x = int(value.get("x", 0))
            y = int(value.get("y", 0))
            return f"mouse_move:{x}:{y}"
        elif action_type == "mouse_scroll":
            wheel = int(value.get("wheel", 0))
            return f"mouse_scroll:{wheel}"
        elif action_type == "mouse_abs":
            x = float(value.get("x", 0))
            y = float(value.get("y", 0))
            return f"mouse_abs:{x}:{y}"
        elif action_type == "mouse_abs_px":
            x = float(value.get("x", 0))
            y = float(value.get("y", 0))
            return f"mouse_abs_px:{x}:{y}"
        elif action_type == "mouse_abs_mon":
            monitor = int(value.get("monitor", 0))
            x = float(value.get("x", 0))
            y = float(value.get("y", 0))
            return f"mouse_abs_mon:{monitor}:{x}:{y}"
        elif action_type == "mouse_goto":
            x = int(value.get("x", 0))
            y = int(value.get("y", 0))
            return f"mouse_goto:{x}:{y}"
        elif action_type == "switch_host":
            slot = value.get("slot", 0)
            # "next"/"prev" cycle through the configured slots on the device;
            # "back" returns to the slot active before the last switch
            if isinstance(slot, str) and slot.lower() == "back":
                return "switch_host:back"
            if isinstance(slot, str) and slot.lower() in ("next", "prev", "previous"):
                return f"switch_host:{'prev' if slot.lower().startswith('prev') else 'next'}"
            return f"switch_host:{int(slot)}"
        elif action_type == "forget_host":
            slot = int(value.get("slot", 0))
            return f"forget_host:{slot}"
        elif action_type == "advertise":
            # Whether a host advertises at all; no slot means the active host
            state = value.get("state", "")
            if isinstance(state, bool):  # YAML reads a bare on/off as a boolean
                state = "on" if state else "off"
            state = str(state).lower()
            if state not in ("on", "off", "toggle"):
                raise cv.Invalid("advertise takes state: on, off or toggle")
            if "slot" not in value:
                return f"advertise:{state}"
            return f"advertise:{cv.int_range(min=0, max=9)(value['slot'])}:{state}"
        raise cv.Invalid(f"Unknown action type '{action_type}'. Use 'combo', 'consumer', 'mouse_click', 'mouse_move', 'mouse_scroll', 'mouse_abs', 'mouse_abs_px', 'mouse_abs_mon', 'mouse_goto', 'switch_host', 'forget_host', or 'advertise'.")
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
    # Register button info for web control page
    name = config.get("name", config[CONF_ACTION])
    cg.add(parent.register_button(name, config[CONF_ACTION]))