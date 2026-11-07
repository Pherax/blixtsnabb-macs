"""Electrolux MACS hob interface for ESPHome."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_MODE

DEPENDENCIES = ["uart"]
CODEOWNERS = []

CONF_ALLOW_MASTER = "allow_master"
CONF_MAX_LEVEL = "max_level"
CONF_COMMAND_TIMEOUT = "command_timeout"
CONF_REQUIRE_API = "require_api_connection"
CONF_TABLE_INTERVAL = "table_interval"

macs_hob_ns = cg.esphome_ns.namespace("macs_hob")
MacsHob = macs_hob_ns.class_("MacsHob", cg.Component, uart.UARTDevice)

MODES = {"listen": 0, "dev": 1, "master": 2, "auto": 3}

_BASE_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MacsHob),
            # listen: never transmits. dev: explicit injection only, panel
            # stays master. master: this device replaces the touch panel.
            cv.Optional(CONF_MODE, default="listen"): cv.enum(MODES, lower=True),
            # Master mode cannot be selected at runtime unless this is true.
            cv.Optional("pan_absence_timeout", default="60s"): cv.positive_time_period_milliseconds,
            cv.Optional("communication_timeout", default="12s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ALLOW_MASTER, default=False): cv.boolean,
            cv.Optional(CONF_MAX_LEVEL, default=9): cv.int_range(min=0, max=10),
            # Extra limit since the last command of any kind; 0s = off. The per-level
            # automatic switch-off below (like the original panel) is the normal limit.
            cv.Optional(
                CONF_COMMAND_TIMEOUT, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional("auto_switch_off", default=True): cv.boolean,
            cv.Optional(CONF_REQUIRE_API, default=True): cv.boolean,
            cv.Optional(
                CONF_TABLE_INTERVAL, default="4s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


def _validate(config):
    if config[CONF_TABLE_INTERVAL].total_milliseconds > 8000:
        raise cv.Invalid(
            f"{CONF_TABLE_INTERVAL} must stay below 8s: the generator boards stop "
            "have not been validated for longer table intervals"
        )
    if config["communication_timeout"].total_milliseconds < 10000:
        raise cv.Invalid("communication_timeout must be at least 10s for observed 8s status periods")
    # cv.enum yields the mode name, not its number: compare names.
    if str(config[CONF_MODE]) in ("master", "auto") and not config[CONF_ALLOW_MASTER]:
        raise cv.Invalid(
            f"mode: master also requires {CONF_ALLOW_MASTER}: true. Master mode "
            "replaces the touch panel and must be opted into deliberately."
        )
    return config


CONFIG_SCHEMA = cv.All(_BASE_SCHEMA, _validate)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_pan_timeout(config["pan_absence_timeout"].total_milliseconds))
    cg.add(var.set_comm_timeout(config["communication_timeout"].total_milliseconds))
    cg.add(var.set_initial_mode(config[CONF_MODE]))
    cg.add(var.set_allow_master(config[CONF_ALLOW_MASTER]))
    cg.add(var.set_max_level(config[CONF_MAX_LEVEL]))
    cg.add(var.set_command_timeout(config[CONF_COMMAND_TIMEOUT].total_milliseconds))
    cg.add(var.set_auto_switch_off(config["auto_switch_off"]))
    cg.add(var.set_require_api(config[CONF_REQUIRE_API]))
    cg.add(var.set_table_interval(config[CONF_TABLE_INTERVAL].total_milliseconds))


def _final_validate(config):
    from esphome import final_validate as fv
    if config[CONF_REQUIRE_API] and "api" not in fv.full_config.get():
        raise cv.Invalid("require_api_connection requires an api: component")
    return config

FINAL_VALIDATE_SCHEMA = _final_validate
