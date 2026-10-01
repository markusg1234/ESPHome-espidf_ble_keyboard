import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_TYPE, DEVICE_CLASS_PRESENCE
import esphome.final_validate as fv
from . import EspidfBleKeyboard

DEPENDENCIES = ["espidf_ble_keyboard"]

CONF_KEYBOARD_ID = "keyboard_id"
CONF_SLOT = "slot"
CONF_TIMEOUT = "timeout"
CONF_PRESENCE_SCAN = "presence_scan"

TYPE_PAIRED = "paired"
TYPE_NUM_LOCK = "num_lock"
TYPE_CAPS_LOCK = "caps_lock"
TYPE_SCROLL_LOCK = "scroll_lock"
TYPE_PRESENCE = "presence"
TYPES = (TYPE_PAIRED, TYPE_NUM_LOCK, TYPE_CAPS_LOCK, TYPE_SCROLL_LOCK, TYPE_PRESENCE)


def _check_slot(config):
    kind = config[CONF_TYPE]
    if kind == TYPE_PRESENCE and CONF_SLOT not in config:
        raise cv.Invalid("type: presence needs slot: — the host slot to listen for")
    if CONF_SLOT in config and kind not in (TYPE_PAIRED, TYPE_PRESENCE):
        raise cv.Invalid("slot: only applies to the paired and presence sensors")
    if CONF_TIMEOUT in config and kind != TYPE_PRESENCE:
        raise cv.Invalid("timeout: only applies to the presence sensor")
    return config


def _schema(config):
    # The presence sensor reads as Home / Away in Home Assistant; the rest keep
    # whatever device class the user gives them.
    is_presence = str(config.get(CONF_TYPE, TYPE_PAIRED)).lower() == TYPE_PRESENCE
    base = binary_sensor.binary_sensor_schema(device_class=DEVICE_CLASS_PRESENCE if is_presence else cv.UNDEFINED)
    return cv.All(
        base.extend(
            {
                cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
                cv.Optional(CONF_TYPE, default=TYPE_PAIRED): cv.one_of(*TYPES, lower=True),
                # paired: ON only while this slot's host is connected and paired.
                # presence: the slot whose host it listens for.
                cv.Optional(CONF_SLOT): cv.int_range(min=0, max=9),
                # presence: OFF once the host has gone unheard this long. A scan
                # takes ~12 s, so anything much shorter flaps between scans.
                cv.Optional(CONF_TIMEOUT): cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(min=cv.TimePeriod(seconds=15), max=cv.TimePeriod(hours=24)),
                ),
            }
        ),
        _check_slot,
    )(config)


CONFIG_SCHEMA = _schema


def _needs_presence_scan(config):
    if config[CONF_TYPE] != TYPE_PRESENCE:
        return config
    fconf = fv.full_config.get()
    keyboard = fconf.get_config_for_path(fconf.get_path_for_id(config[CONF_KEYBOARD_ID])[:-1])
    if not keyboard.get(CONF_PRESENCE_SCAN, False):
        raise cv.Invalid(
            "type: presence needs presence_scan: true under espidf_ble_keyboard:", path=[CONF_TYPE]
        )
    return config


FINAL_VALIDATE_SCHEMA = _needs_presence_scan


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
    elif sensor_type == TYPE_PRESENCE:
        timeout = config.get(CONF_TIMEOUT)
        timeout_ms = timeout.total_milliseconds if timeout is not None else 60000
        cg.add(parent.add_presence_binary_sensor(config[CONF_SLOT], var, int(timeout_ms)))
    elif CONF_SLOT in config:
        cg.add(parent.add_slot_paired_binary_sensor(config[CONF_SLOT], var))
    else:
        cg.add(parent.set_paired_binary_sensor(var))
