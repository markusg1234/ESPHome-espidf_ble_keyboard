import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]

# Define the passkey configuration key
CONF_PASSKEY = "passkey"

espidf_ble_keyboard_ns = cg.esphome_ns.namespace("espidf_ble_keyboard")
EspidfBleKeyboard = espidf_ble_keyboard_ns.class_("EspidfBleKeyboard", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(EspidfBleKeyboard),
    # Allow a 6-digit integer for the passkey
    cv.Optional(CONF_PASSKEY): cv.int_range(min=0, max=999999),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # If a passkey is provided in the YAML, set it in the C++ object
    if CONF_PASSKEY in config:
        cg.add(var.set_passkey(config[CONF_PASSKEY]))

    # Run after WiFi (priority 500)
    cg.add(var.set_setup_priority(500.0))

    try:
        from esphome.components.esp32 import include_builtin_idf_component
        include_builtin_idf_component("bt")
        include_builtin_idf_component("nvs_flash")
    except ImportError:
        pass