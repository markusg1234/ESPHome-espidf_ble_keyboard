import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_TYPE
from . import EspidfBleKeyboard

DEPENDENCIES = ["espidf_ble_keyboard"]

CONF_KEYBOARD_ID = "keyboard_id"
CONF_SLOT = "slot"

TYPE_PAIRED = "paired"
TYPE_NUM_LOCK = "num_lock"
TYPE_CAPS_LOCK = "caps_lock"
TYPE_SCROLL_LOCK = "scroll_lock"


def _slot_needs_paired(config):
    if CONF_SLOT in config and config[CONF_TYPE] != TYPE_PAIRED:
        raise cv.Invalid("slot: only applies to the paired sensor")
    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema().extend(
        {
            cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
            cv.Optional(CONF_TYPE, default=TYPE_PAIRED): cv.one_of(
                TYPE_PAIRED, TYPE_NUM_LOCK, TYPE_CAPS_LOCK, TYPE_SCROLL_LOCK, lower=True
            ),
            # ON only while this slot's host is connected and paired.
            cv.Optional(CONF_SLOT): cv.int_range(min=0, max=9),
        }
    ),
    _slot_needs_paired,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])

    sensor_type = config.get(CONF_TYPE, TYPE_PAIRED)
    if sensor_type == TYPE_NUM_LOCK:
        cg.add(parent.set_num_lock_binary_sensor(var))
    elif sensor_type == TYPE_CAPS_LOCK:
        cg.add(parent.set_caps_lock_binary_sensor(var))
    elif sensor_type == TYPE_SCROLL_LOCK:
        cg.add(parent.set_scroll_lock_binary_sensor(var))
    elif CONF_SLOT in config:
        cg.add(parent.add_slot_paired_binary_sensor(config[CONF_SLOT], var))
    else:
        cg.add(parent.set_paired_binary_sensor(var))
