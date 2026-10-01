import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_DECIBEL_MILLIWATT,
)
import esphome.final_validate as fv
from . import EspidfBleKeyboard, espidf_ble_keyboard_ns

DEPENDENCIES = ["espidf_ble_keyboard"]

CONF_KEYBOARD_ID = "keyboard_id"
CONF_TYPE = "type"
CONF_SLOT = "slot"
CONF_PRESENCE_SCAN = "presence_scan"

TYPE_RSSI = "rssi"
TYPE_ACTIVE_HOST = "active_host"
TYPE_PRESENCE_COUNT = "presence_count"
TYPE_PRESENCE_RSSI = "presence_rssi"
TYPES = (TYPE_RSSI, TYPE_ACTIVE_HOST, TYPE_PRESENCE_COUNT, TYPE_PRESENCE_RSSI)
PRESENCE_TYPES = (TYPE_PRESENCE_COUNT, TYPE_PRESENCE_RSSI)


def _sensor_schema(config):
    sensor_type = str(config.get(CONF_TYPE, TYPE_RSSI)).lower()
    if sensor_type == TYPE_RSSI:
        return sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {
                cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
                cv.Optional(CONF_TYPE, default=TYPE_RSSI): cv.one_of(*TYPES, lower=True),
                cv.Optional("update_interval", default="10s"): cv.update_interval,
            }
        )(config)
    if sensor_type == TYPE_PRESENCE_RSSI:
        # The signal strength a paired host was last heard at: one reading per
        # scan, and unknown once it has gone unheard for a minute.
        return sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {
                cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
                cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
                cv.Required(CONF_SLOT): cv.int_range(min=0, max=9),
            }
        )(config)
    if sensor_type == TYPE_PRESENCE_COUNT:
        # Sightings since boot: it only ever climbs, and starts again from 0 after
        # a restart — which is what total_increasing tells Home Assistant.
        return sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            icon="mdi:radar",
        ).extend(
            {
                cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
                cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
                cv.Required(CONF_SLOT): cv.int_range(min=0, max=9),
            }
        )(config)
    return sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        {
            cv.Required(CONF_KEYBOARD_ID): cv.use_id(EspidfBleKeyboard),
            cv.Optional(CONF_TYPE, default=TYPE_RSSI): cv.one_of(*TYPES, lower=True),
        }
    )(config)


CONFIG_SCHEMA = _sensor_schema


def _needs_presence_scan(config):
    if config.get(CONF_TYPE) not in PRESENCE_TYPES:
        return config
    fconf = fv.full_config.get()
    keyboard = fconf.get_config_for_path(fconf.get_path_for_id(config[CONF_KEYBOARD_ID])[:-1])
    if not keyboard.get(CONF_PRESENCE_SCAN, False):
        raise cv.Invalid(
            f"type: {config[CONF_TYPE]} needs presence_scan: true under espidf_ble_keyboard:", path=[CONF_TYPE]
        )
    return config


FINAL_VALIDATE_SCHEMA = _needs_presence_scan


async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_KEYBOARD_ID])

    sensor_type = config.get(CONF_TYPE, TYPE_RSSI)
    if sensor_type == TYPE_RSSI:
        cg.add(parent.set_rssi_sensor(var))
        interval_ms = int(config["update_interval"].total_milliseconds)
        cg.add(parent.set_rssi_update_interval(interval_ms))
    elif sensor_type == TYPE_ACTIVE_HOST:
        cg.add(parent.set_active_host_sensor(var))
    elif sensor_type == TYPE_PRESENCE_COUNT:
        cg.add(parent.add_presence_count_sensor(config[CONF_SLOT], var))
    elif sensor_type == TYPE_PRESENCE_RSSI:
        cg.add(parent.add_presence_rssi_sensor(config[CONF_SLOT], var))
