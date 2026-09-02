import gzip
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import button, sensor
from esphome.const import CONF_ID
from esphome.core import EsphomeError, HexInt
from esphome import automation

DEPENDENCIES = ["esp32"]
# text_sensor is auto-loaded because espidf_ble_keyboard.h includes its header
# unconditionally (like sensor/binary_sensor) — without it, a config that never
# declares a text_sensor fails to compile on the missing include.
AUTO_LOAD = ["sensor", "binary_sensor", "button", "text", "text_sensor"]

# Define configuration keys
CONF_DEVICE_NAME = "device_name"
CONF_KEY_DELAY_MS = "key_delay_ms"
CONF_MAX_KEY_HOLD_MS = "max_key_hold_ms"
CONF_PASSKEY = "passkey"
CONF_PASSKEY_MODE = "passkey_mode"
CONF_WEB_CONTROL = "web_control"
CONF_WEB_PAGE_DATA_ID = "web_page_data_id"
CONF_API_SERVICES = "api_services"
CONF_HA_ACTION = "ha_action"
CONF_HOST_SLOTS = "host_slots"
CONF_MOUSE_SENSITIVITY = "mouse_sensitivity"
CONF_MOUSE_ACCEL = "mouse_acceleration"
CONF_MOUSE_MAX_SPEED = "mouse_max_speed"
CONF_SCROLL_SENSITIVITY = "scroll_sensitivity"
CONF_SCREEN_WIDTH = "screen_width"
CONF_SCREEN_HEIGHT = "screen_height"
CONF_MOUSE_GOTO_SCALE = "mouse_goto_scale"
CONF_MOUSE_GOTO_SCALE_X = "mouse_goto_scale_x"
CONF_MOUSE_GOTO_SCALE_Y = "mouse_goto_scale_y"
CONF_MONITORS = "monitors"
CONF_WIDTH = "width"
CONF_HEIGHT = "height"
CONF_X = "x"
CONF_Y = "y"
CONF_NAME = "name"
CONF_PRIMARY = "primary"
CONF_HOSTS = "hosts"
CONF_SLOT = "slot"
CONF_CUSTOM_TEXT_ID = "custom_text_id"
CONF_EXPOSE_BUTTONS = "expose_buttons"
CONF_HIDE_BUTTONS = "hide_buttons"
CONF_KEYBOARD_LAYOUT = "keyboard_layout"
CONF_LAYOUT = "layout"
CONF_ACTIONS = "actions"
CONF_BATTERY_LEVEL = "battery_level"
# Keep in sync with EspidfBleKeyboard::MAX_OVERRIDES
MAX_OVERRIDES_PER_HOST = 8
PASSKEY_MODE_LEGACY = "legacy"
PASSKEY_MODE_SECURE_CONNECTIONS = "secure_connections"

# To add a layout: register it in keyboard_layouts.cpp, add its id JS rows in
# web_control.cpp, then append the id here.
SUPPORTED_LAYOUTS = ["us", "uk", "de", "be"]

espidf_ble_keyboard_ns = cg.esphome_ns.namespace("espidf_ble_keyboard")
EspidfBleKeyboard = espidf_ble_keyboard_ns.class_("EspidfBleKeyboard", cg.Component)
RssiAboveTrigger = espidf_ble_keyboard_ns.class_("RssiAboveTrigger", automation.Trigger.template(cg.int_))
RssiBelowTrigger = espidf_ble_keyboard_ns.class_("RssiBelowTrigger", automation.Trigger.template(cg.int_))

CONF_ON_RSSI_ABOVE = "on_rssi_above"
CONF_ON_RSSI_BELOW = "on_rssi_below"
CONF_THRESHOLD = "threshold"

# Press-and-hold actions. A `button` entity can't drive these — it has a press
# and no release — so they are meant for a binary_sensor's on_press/on_release,
# which is what a physical push-to-talk key needs.
KeyHoldAction = espidf_ble_keyboard_ns.class_("KeyHoldAction", automation.Action)
HoldActionAction = espidf_ble_keyboard_ns.class_("HoldActionAction", automation.Action)
RunActionAction = espidf_ble_keyboard_ns.class_("RunActionAction", automation.Action)
KeyReleaseAction = espidf_ble_keyboard_ns.class_("KeyReleaseAction", automation.Action)
SetBatteryLevelAction = espidf_ble_keyboard_ns.class_("SetBatteryLevelAction", automation.Action)

CONF_MODIFIER = "modifier"
CONF_KEY = "key"
CONF_ACTION = "action"
CONF_LEVEL = "level"


@automation.register_action(
    "espidf_ble_keyboard.key_hold",
    KeyHoldAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(EspidfBleKeyboard),
        cv.Optional(CONF_MODIFIER, default=0): cv.templatable(cv.hex_uint8_t),
        cv.Optional(CONF_KEY, default=0): cv.templatable(cv.hex_uint8_t),
    }),
    # All three send their HID report and return — nothing is deferred to a
    # callback, timer or loop(), so play_next_() runs before play() returns.
    synchronous=True,
)
async def key_hold_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_modifier(await cg.templatable(config[CONF_MODIFIER], args, cg.uint8)))
    cg.add(var.set_key(await cg.templatable(config[CONF_KEY], args, cg.uint8)))
    return var


@automation.register_action(
    "espidf_ble_keyboard.hold_action",
    HoldActionAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(EspidfBleKeyboard),
        cv.Required(CONF_ACTION): cv.templatable(cv.string_strict),
    }),
    synchronous=True,
)
async def hold_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_action(await cg.templatable(config[CONF_ACTION], args, cg.std_string)))
    return var


# The no-lambda twin of execute_action(): any trigger can run an action string,
# including `macro:<name>` and multi-step sequences.
@automation.register_action(
    "espidf_ble_keyboard.run_action",
    RunActionAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(EspidfBleKeyboard),
        cv.Required(CONF_ACTION): cv.templatable(cv.string_strict),
    }),
    synchronous=True,
)
async def run_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_action(await cg.templatable(config[CONF_ACTION], args, cg.std_string)))
    return var


@automation.register_action(
    "espidf_ble_keyboard.key_release",
    KeyReleaseAction,
    cv.Schema({cv.GenerateID(): cv.use_id(EspidfBleKeyboard)}),
    synchronous=True,
)
async def key_release_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "espidf_ble_keyboard.set_battery_level",
    SetBatteryLevelAction,
    cv.Schema({
        cv.GenerateID(): cv.use_id(EspidfBleKeyboard),
        cv.Required(CONF_LEVEL): cv.templatable(cv.int_range(min=0, max=100)),
    }),
    synchronous=True,
)
async def set_battery_level_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_level(await cg.templatable(config[CONF_LEVEL], args, cg.uint8)))
    return var


def _web_control_schema(config):
    """When web_control is true, require web_server_base and claim its sockets."""
    if config.get(CONF_WEB_CONTROL):
        from esphome.components import web_server_base
        from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
        config = cv.Schema({
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
        }, extra=cv.ALLOW_EXTRA)(config)
        # Claim the sockets the control page actually needs. ESP-IDF sizes its
        # socket pool from what components declare here, and web_server declares
        # nothing — so the pool stayed at the default 10 no matter how heavy the
        # page was. Loading this one opens several connections at once, and every
        # reply carries Connection: close, so each finished request leaves a
        # socket in TIME_WAIT while the next ones are still arriving. Run out and
        # the server refuses the connection, which the log shows as
        # "httpd_accept_conn: error in accept (23)" and the browser shows as a
        # page that crawls or stalls until it retries.
        #
        # Six is the number a browser will open to one host at a time. A value
        # set in the user's own sdkconfig_options still wins over this.
        try:
            from esphome.components import socket
            socket.consume_sockets(6, "espidf_ble_keyboard web_control")(config)
        except (ImportError, AttributeError):
            # ESPHome without the socket-accounting API. Nothing to declare to,
            # and the build is no worse off than it was before.
            pass
    return config


# Absolute-pointer monitor region (virtual-desktop pixels)
MONITOR_SCHEMA = cv.Schema({
    cv.Optional(CONF_NAME): cv.string,
    cv.Required(CONF_X): cv.int_,
    cv.Required(CONF_Y): cv.int_,
    cv.Required(CONF_WIDTH): cv.int_range(min=1),
    cv.Required(CONF_HEIGHT): cv.int_range(min=1),
    # Mark the Windows primary monitor (its top-left = Windows 0,0). Lets the web
    # Position Finder emit mouse_goto values; only one should be primary.
    cv.Optional(CONF_PRIMARY, default=False): cv.boolean,
})

def _validate_override_names(value):
    """Validate the keys of a per-host `actions:` mapping.

    Checked here rather than as a key validator, because voluptuous discards a
    failing key validator's message and reports a bare "invalid option" instead.

    Only *named* actions can be remapped (parametric forms like `combo:` or
    `consumer:` are dispatched before the override lookup), and the name is
    stored in a `name=action` NVS record — so the separators are rejected here,
    matching EspidfBleKeyboard::valid_override_name() on the C++ side."""
    for name in value:
        if len(name) > 31:
            raise cv.Invalid(f"Action name '{name}' is too long (max 31 characters)")
        for ch in "=| \t\r\n":
            if ch in name:
                raise cv.Invalid(
                    f"Invalid action name '{name}': must not contain '=', '|' or whitespace. "
                    "Only named actions can be overridden (e.g. record, play_pause, stop); "
                    "see the Action Types table in the README."
                )
    return value


HOST_SCHEMA = cv.Schema({
    cv.Required(CONF_SLOT): cv.int_range(min=0, max=9),
    cv.Optional(CONF_PASSKEY): cv.int_range(min=0, max=999999),
    cv.Optional(CONF_PASSKEY_MODE, default=PASSKEY_MODE_LEGACY): cv.one_of(
        PASSKEY_MODE_LEGACY,
        PASSKEY_MODE_SECURE_CONNECTIONS,
        lower=True,
    ),
    cv.Optional(CONF_LAYOUT): cv.one_of(*SUPPORTED_LAYOUTS, lower=True),
    # Remap named actions for this host, e.g. record: "combo:0x0C:0x15" so a
    # Windows slot drives Game Bar while a TV slot keeps HID Record.
    cv.Optional(CONF_ACTIONS): cv.All(
        cv.Schema({cv.string: cv.All(cv.string, cv.Length(min=1, max=255))}),
        _validate_override_names,
        # Without this the C++ side would silently drop the extras
        cv.Length(
            max=MAX_OVERRIDES_PER_HOST,
            msg=f"At most {MAX_OVERRIDES_PER_HOST} action overrides per host slot",
        ),
    ),
})

def _validate_max_key_hold(value):
    """0 disables the auto-release; anything else must be long enough to be a
    deliberate hold rather than a value that fights the press itself."""
    value = cv.positive_int(value)
    if value == 0:
        return value
    return cv.int_range(min=100, max=600000)(value)


def _api_final_validate(config):
    """api_services and ha_action both need the api component, and each leans
    on an `api:` option that recent ESPHome turns off by default
    (custom_services gates register_service() and building user_services.cpp on
    2025.11+; homeassistant_services gates device→HA action calls) —
    force-enable them so users don't have to know about it."""
    needs = {
        CONF_API_SERVICES: "custom_services",
        CONF_HA_ACTION: "homeassistant_services",
    }
    enabled = [opt for opt in needs if config.get(opt)]
    if not enabled:
        return config
    full = fv.full_config.get()
    if "api" not in full:
        raise cv.Invalid(
            f"{enabled[0]}: true requires the 'api:' component — add an 'api:' section to your config"
        )
    api_conf = full["api"]
    if isinstance(api_conf, dict):
        for opt in enabled:
            api_conf[needs[opt]] = True
    return config


FINAL_VALIDATE_SCHEMA = _api_final_validate

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(EspidfBleKeyboard),
        cv.Optional(CONF_DEVICE_NAME, default="ESP32 BLE KB"): cv.All(cv.string, cv.Length(max=29)),
        cv.Optional(CONF_KEY_DELAY_MS, default=80): cv.int_range(min=2, max=10000),
        # Safety net for a hold whose release never arrives. 0 (the default)
        # means a held key stays down until something releases it — a
        # push-to-talk key has no natural maximum, so nothing is imposed.
        cv.Optional(CONF_MAX_KEY_HOLD_MS, default=0): _validate_max_key_hold,
        cv.Optional(CONF_PASSKEY): cv.int_range(min=0, max=999999),
        cv.Optional(CONF_PASSKEY_MODE, default=PASSKEY_MODE_LEGACY): cv.one_of(
            PASSKEY_MODE_LEGACY,
            PASSKEY_MODE_SECURE_CONNECTIONS,
            lower=True,
        ),
        cv.Optional(CONF_WEB_CONTROL, default=False): cv.boolean,
        # Names the progmem array that carries the gzipped control page. Only
        # emitted when web_control is on; harmless when it isn't.
        cv.GenerateID(CONF_WEB_PAGE_DATA_ID): cv.declare_id(cg.uint8),
        # Auto-register the documented HA services (run_action, mouse_move, ...)
        # from C++ — requires the `api:` component. Off by default: configs that
        # pasted the manual `api: services:` snippets would get duplicate names.
        cv.Optional(CONF_API_SERVICES, default=False): cv.boolean,
        # Let `ha_action:` strings fire Home Assistant actions from the device
        # (IR blasters, scripts, scenes) — requires the `api:` component. Off by
        # default: the web page is unauthenticated, so LAN-reachable HA calls
        # must be a deliberate choice, doubly gated by HA's own per-device
        # "allow the device to perform actions" toggle.
        cv.Optional(CONF_HA_ACTION, default=False): cv.boolean,
        cv.Optional(CONF_MOUSE_SENSITIVITY, default=1.0): cv.float_range(min=0.1, max=10.0),
        cv.Optional(CONF_MOUSE_ACCEL, default=0.15): cv.float_range(min=0.0, max=2.0),
        cv.Optional(CONF_MOUSE_MAX_SPEED, default=4.0): cv.float_range(min=0.5, max=20.0),
        cv.Optional(CONF_SCROLL_SENSITIVITY, default=2.0): cv.float_range(min=0.1, max=10.0),
        cv.Optional(CONF_SCREEN_WIDTH, default=1920): cv.int_range(min=1, max=32767),
        cv.Optional(CONF_SCREEN_HEIGHT, default=1080): cv.int_range(min=1, max=32767),
        cv.Optional(CONF_MOUSE_GOTO_SCALE, default=1.0): cv.float_range(min=0.05, max=20.0),
        cv.Optional(CONF_MOUSE_GOTO_SCALE_X): cv.float_range(min=0.05, max=20.0),
        cv.Optional(CONF_MOUSE_GOTO_SCALE_Y): cv.float_range(min=0.05, max=20.0),
        cv.Optional(CONF_MONITORS): cv.ensure_list(MONITOR_SCHEMA),
        cv.Optional(CONF_KEYBOARD_LAYOUT, default="us"): cv.one_of(*SUPPORTED_LAYOUTS, lower=True),
        # Feed the BLE Battery Service, which hosts show in their Bluetooth
        # settings. Any sensor reading 0-100 will do; without this the service
        # is still advertised and reports a fixed 100%.
        cv.Optional(CONF_BATTERY_LEVEL): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_CUSTOM_TEXT_ID): cv.ensure_list(cv.use_id(cg.EntityBase)),
        # Every non-internal ESPHome button in the config is listed on the web
        # control page. use_id (not a name string) so a typo fails the build.
        cv.Optional(CONF_EXPOSE_BUTTONS, default=True): cv.boolean,
        cv.Optional(CONF_HIDE_BUTTONS): cv.ensure_list(cv.use_id(button.Button)),
        cv.Optional(CONF_HOST_SLOTS, default=4): cv.int_range(min=1, max=10),
        cv.Optional(CONF_HOSTS): cv.All(cv.ensure_list(HOST_SCHEMA)),
        cv.Optional(CONF_ON_RSSI_ABOVE): automation.validate_automation({
            cv.GenerateID(automation.CONF_TRIGGER_ID): cv.declare_id(RssiAboveTrigger),
            cv.Required(CONF_THRESHOLD): cv.int_range(min=-127, max=0),
        }),
        cv.Optional(CONF_ON_RSSI_BELOW): automation.validate_automation({
            cv.GenerateID(automation.CONF_TRIGGER_ID): cv.declare_id(RssiBelowTrigger),
            cv.Required(CONF_THRESHOLD): cv.int_range(min=-127, max=0),
        }),
    }).extend(cv.COMPONENT_SCHEMA),
    _web_control_schema,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
    cg.add(var.set_key_delay_ms(config[CONF_KEY_DELAY_MS]))
    cg.add(var.set_max_key_hold_ms(config[CONF_MAX_KEY_HOLD_MS]))

    if CONF_PASSKEY in config:
        cg.add(var.set_passkey(config[CONF_PASSKEY]))

    cg.add(var.set_passkey_secure_connections(
        config[CONF_PASSKEY_MODE] == PASSKEY_MODE_SECURE_CONNECTIONS
    ))

    cg.add(var.set_host_slots(config[CONF_HOST_SLOTS]))
    cg.add(var.set_mouse_sensitivity(config[CONF_MOUSE_SENSITIVITY]))
    cg.add(var.set_mouse_accel(config[CONF_MOUSE_ACCEL]))
    cg.add(var.set_mouse_max_speed(config[CONF_MOUSE_MAX_SPEED]))
    cg.add(var.set_scroll_sensitivity(config[CONF_SCROLL_SENSITIVITY]))
    cg.add(var.set_screen_size(config[CONF_SCREEN_WIDTH], config[CONF_SCREEN_HEIGHT]))
    cg.add(var.set_mouse_goto_scale(config[CONF_MOUSE_GOTO_SCALE]))
    if CONF_MOUSE_GOTO_SCALE_X in config:
        cg.add(var.set_mouse_goto_scale_x(config[CONF_MOUSE_GOTO_SCALE_X]))
    if CONF_MOUSE_GOTO_SCALE_Y in config:
        cg.add(var.set_mouse_goto_scale_y(config[CONF_MOUSE_GOTO_SCALE_Y]))
    for mon in config.get(CONF_MONITORS, []):
        cg.add(var.add_monitor(mon[CONF_X], mon[CONF_Y], mon[CONF_WIDTH], mon[CONF_HEIGHT], mon[CONF_PRIMARY]))
    cg.add(var.set_keyboard_layout(config[CONF_KEYBOARD_LAYOUT]))

    if CONF_BATTERY_LEVEL in config:
        battery = await cg.get_variable(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_sensor(battery))

    if CONF_HOSTS in config:
        for host in config[CONF_HOSTS]:
            if CONF_PASSKEY in host:
                sc = host[CONF_PASSKEY_MODE] == PASSKEY_MODE_SECURE_CONNECTIONS
                cg.add(var.set_host_slot_passkey(host[CONF_SLOT], host[CONF_PASSKEY], sc))
            if CONF_LAYOUT in host:
                cg.add(var.set_host_slot_layout(host[CONF_SLOT], host[CONF_LAYOUT]))
            for name, act in host.get(CONF_ACTIONS, {}).items():
                cg.add(var.set_host_slot_override(host[CONF_SLOT], name, act))

    if CONF_CUSTOM_TEXT_ID in config:
        cg.add_define("USE_TEXT")
        for i, text_id in enumerate(config[CONF_CUSTOM_TEXT_ID]):
            text_entity = await cg.get_variable(text_id)
            cg.add(var.add_custom_text(text_entity))
            cg.add(var.register_button(f"Send {text_id.id}", f"send_custom_text:{i}"))

    cg.add(var.set_expose_buttons(config[CONF_EXPOSE_BUTTONS]))
    for btn_id in config.get(CONF_HIDE_BUTTONS, []):
        cg.add(var.add_hidden_button(await cg.get_variable(btn_id)))

    if config[CONF_API_SERVICES]:
        cg.add(var.set_api_services(True))

    if config[CONF_HA_ACTION]:
        cg.add(var.set_ha_action(True))

    if config[CONF_WEB_CONTROL]:
        cg.add_define("USE_BLE_KEYBOARD_WEB_CONTROL")
        cg.add(var.set_web_control(True))
        from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
        base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
        cg.add(var.set_web_server_base(base))

        # Compress the control page into the firmware. Stored raw it was 243 KB
        # of flash — the single largest thing in the image, 15% of it — and the
        # device also had to push all 243 KB down a TCP connection on every page
        # load. Gzipped it is under a third of that, and the handler serves the
        # bytes straight from flash with Content-Encoding: gzip, which is how
        # ESPHome's own web UI serves its index too.
        #
        # This is deliberately a codegen step rather than a checked-in blob: the
        # page changes often, and a generated file committed beside it would
        # drift the first time someone edited one without the other. It cannot be
        # written into the build tree either — writer.copy_src_tree() deletes any
        # source file there that a component doesn't declare.
        page = Path(__file__).parent / "web_page.html"
        if not page.is_file():
            raise EsphomeError(
                f"web_control needs the control page at {page}, but it is not there. "
                "If this component came from a git source, the checkout is incomplete."
            )
        # Normalised to LF so a Windows checkout with core.autocrlf serves the
        # same bytes as a Linux one, and mtime=0 so the same page always gzips to
        # the same array — otherwise every build would look changed.
        html = page.read_bytes().replace(b"\r\n", b"\n")
        gz = gzip.compress(html, 9, mtime=0)
        arr = cg.progmem_array(
            config[CONF_WEB_PAGE_DATA_ID], [HexInt(b) for b in gz]
        )
        cg.add(var.set_web_page(arr, len(gz)))

    # set_setup_priority() removed in ESPHome 2026.x
    # Priority is now set via get_setup_priority() override in the C++ header

    for conf in config.get(CONF_ON_RSSI_ABOVE, []):
        trigger = cg.new_Pvariable(conf[automation.CONF_TRIGGER_ID], var, conf[CONF_THRESHOLD])
        await automation.build_automation(trigger, [(cg.int_, "rssi")], conf)

    for conf in config.get(CONF_ON_RSSI_BELOW, []):
        trigger = cg.new_Pvariable(conf[automation.CONF_TRIGGER_ID], var, conf[CONF_THRESHOLD])
        await automation.build_automation(trigger, [(cg.int_, "rssi")], conf)

    try:
        from esphome.components.esp32 import include_builtin_idf_component
        include_builtin_idf_component("bt")
        include_builtin_idf_component("nvs_flash")
    except ImportError:
        pass