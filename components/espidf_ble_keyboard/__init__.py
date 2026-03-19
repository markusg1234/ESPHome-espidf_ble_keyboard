import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]

# Define configuration keys
CONF_DEVICE_NAME = "device_name"
CONF_PASSKEY = "passkey"
CONF_PASSKEY_MODE = "passkey_mode"
PASSKEY_MODE_LEGACY = "legacy"
PASSKEY_MODE_SECURE_CONNECTIONS = "secure_connections"

espidf_ble_keyboard_ns = cg.esphome_ns.namespace("espidf_ble_keyboard")
EspidfBleKeyboard = espidf_ble_keyboard_ns.class_("EspidfBleKeyboard", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(EspidfBleKeyboard),
    cv.Optional(CONF_DEVICE_NAME, default="ESP32 BLE KB"): cv.string,
    # Allow a 6-digit integer for the passkey
    cv.Optional(CONF_PASSKEY): cv.int_range(min=0, max=999999),
    cv.Optional(CONF_PASSKEY_MODE, default=PASSKEY_MODE_LEGACY): cv.one_of(
        PASSKEY_MODE_LEGACY,
        PASSKEY_MODE_SECURE_CONNECTIONS,
        lower=True,
    ),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))

    # If a passkey is provided in the YAML, set it in the C++ object
    if CONF_PASSKEY in config:
        cg.add(var.set_passkey(config[CONF_PASSKEY]))

    cg.add(var.set_passkey_secure_connections(
        config[CONF_PASSKEY_MODE] == PASSKEY_MODE_SECURE_CONNECTIONS
    ))

    # Run after WiFi (priority -100)
    cg.add(var.set_setup_priority(-200))

    try:
        from esphome.components.esp32 import include_builtin_idf_component
        include_builtin_idf_component("bt")
        include_builtin_idf_component("nvs_flash")
    except ImportError:
        pass