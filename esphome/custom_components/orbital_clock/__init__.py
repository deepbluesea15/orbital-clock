import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import http_request, select, sun, text, time
from esphome.const import CONF_ID

DEPENDENCIES = ["http_request", "sun", "time", "wifi"]

orbital_clock_ns = cg.esphome_ns.namespace("orbital_clock")
OrbitalClock = orbital_clock_ns.class_("OrbitalClock", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OrbitalClock),
        cv.Required("face_select"): cv.use_id(select.Select),
        cv.Required("format_select"): cv.use_id(select.Select),
        cv.Required("units_select"): cv.use_id(select.Select),
        cv.Required("timezone_select"): cv.use_id(select.Select),
        cv.Required("city_text"): cv.use_id(text.Text),
        cv.Required("time_id"): cv.use_id(time.RealTimeClock),
        cv.Required("http_request_id"): cv.use_id(http_request.HttpRequestComponent),
        cv.Required("sun_id"): cv.use_id(sun.Sun),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    for key, setter in (
        ("face_select", "set_face_select"),
        ("format_select", "set_format_select"),
        ("units_select", "set_units_select"),
        ("timezone_select", "set_timezone_select"),
        ("city_text", "set_city_text"),
        ("time_id", "set_time_source"),
        ("http_request_id", "set_http_request"),
        ("sun_id", "set_sun"),
    ):
        entity = await cg.get_variable(config[key])
        cg.add(getattr(var, setter)(entity))