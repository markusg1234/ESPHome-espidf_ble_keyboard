import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import EspidfBleKeyboard

DEPENDENCIES = ["espidf_ble_keyboard"]

CONF_KEYBOARD_ID = "keyboard_id"
CONF_TYPE = "type"

TYPE_HIDDEN_BUTTONS = "hidden_buttons"
TYPE_HOST_MAC = "host_mac"
# The two per-host remote behaviours, so the Lovelace cards can mirror what the
# web remote does. They can't read the device's REST API reliably (a dashboard
# served over https blocks the plain-http fetch as mixed content), so the lists
# reach them as entity state instead.
TYPE_HOLD_BUTTONS = "hold_buttons"
TYPE_REPEAT_BUTTONS = "repeat_buttons"
# Which remote style the active host is set to, for the same reason: the id is
# on the /hosts response the card already polls, but that fetch is what https
# blocks. Empty state = the host uses the default style.
TYPE_REMOTE_STYLE = "remote_style"

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
        cv.Optional(CONF_TYPE, default=TYPE_HIDDEN_BUTTONS): cv.one_of(
            TYPE_HIDDEN_BUTTONS,
            TYPE_HOST_MAC,
            TYPE_HOLD_BUTTONS,
            TYPE_REPEAT_BUTTONS,
            TYPE_REMOTE_STYLE,
            lower=True,
        ),
    }
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])

    sensor_type = config.get(CONF_TYPE, TYPE_HIDDEN_BUTTONS)
    if sensor_type == TYPE_HOST_MAC:
        cg.add(parent.set_host_mac_sensor(var))
    elif sensor_type == TYPE_HOLD_BUTTONS:
        cg.add(parent.set_hold_sensor(var))
    elif sensor_type == TYPE_REPEAT_BUTTONS:
        cg.add(parent.set_repeat_sensor(var))
    elif sensor_type == TYPE_REMOTE_STYLE:
        cg.add(parent.set_remote_style_sensor(var))
    else:
        cg.add(parent.set_hidden_sensor(var))
