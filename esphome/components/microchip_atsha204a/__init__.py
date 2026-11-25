from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_ID
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

CODEOWNERS = ["@inventor7777"]
DOMAIN = "microchip_atsha204a"
MULTI_CONF = True

CONF_ATSHA204A_ID = "atsha204a_id"
CONF_END = "end"
CONF_EXPECT_RESPONSE = "expect_response"
CONF_ON_RESULT = "on_result"
CONF_OPCODE = "opcode"
CONF_PARAM1 = "param1"
CONF_PARAM2 = "param2"
CONF_SESSION = "session"
CONF_TRANSPORT_ID = "transport_id"

I2C = "i2c"
MAX_COMMAND_DATA = 77

microchip_atsha204a_ns = cg.esphome_ns.namespace(DOMAIN)
ATSHA204A = microchip_atsha204a_ns.class_("ATSHA204A", cg.PollingComponent)
I2CTransport = microchip_atsha204a_ns.class_("I2CTransport", i2c.I2CDevice)
CommandAction = microchip_atsha204a_ns.class_("CommandAction", automation.Action)
CommandResponse = microchip_atsha204a_ns.class_("CommandResponse")
Opcode = microchip_atsha204a_ns.enum("Opcode", is_class=True)
EndMode = microchip_atsha204a_ns.enum("EndMode", is_class=True)


BASE_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(ATSHA204A)}).extend(
    cv.polling_component_schema("never")
)
CONFIG_SCHEMA = cv.typed_schema(
    {
        I2C: BASE_SCHEMA.extend(i2c.i2c_device_schema(0x64)).extend(
            {cv.GenerateID(CONF_TRANSPORT_ID): cv.declare_id(I2CTransport)}
        ),
    },
    lower=True,
)

FINAL_VALIDATE_SCHEMA = i2c.final_validate_device_schema(DOMAIN, max_frequency="100kHz")


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_MICROCHIP_ATSHA204A_I2C")
    transport = cg.new_Pvariable(config[CONF_TRANSPORT_ID])
    await i2c.register_i2c_device(transport, config)
    var = cg.new_Pvariable(config[CONF_ID], transport)
    await cg.register_component(var, config)


OPCODES = Opcode.options(
    "pause",
    "read",
    "mac",
    "hmac",
    "write",
    "gen_dig",
    "nonce",
    "lock",
    "random",
    "derive_key",
    "update_extra",
    "check_mac",
    "dev_rev",
    "sha",
    prefix="OPCODE_",
)
END_MODES = EndMode.options("sleep", "idle", "keep_awake", prefix="END_MODE_")


def _validate_command(config: ConfigType) -> ConfigType:
    if not config[CONF_EXPECT_RESPONSE] and (
        config[CONF_OPCODE] != "pause" or config[CONF_END] != "sleep"
    ):
        raise cv.Invalid(
            "Only a Pause command with end: sleep can omit the response",
            [CONF_EXPECT_RESPONSE],
        )
    return config


@automation.register_action(
    f"{DOMAIN}.command",
    CommandAction,
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(ATSHA204A),
                cv.Required(CONF_OPCODE): cv.enum(OPCODES, lower=True),
                cv.Optional(CONF_PARAM1, default=0): cv.templatable(cv.hex_uint8_t),
                cv.Optional(CONF_PARAM2, default=0): cv.templatable(cv.hex_uint16_t),
                cv.Optional(CONF_DATA, default=[]): cv.templatable(
                    cv.All(
                        cv.ensure_list(cv.hex_uint8_t), cv.Length(max=MAX_COMMAND_DATA)
                    )
                ),
                cv.Optional(CONF_END, default="sleep"): cv.enum(END_MODES, lower=True),
                cv.Optional(CONF_SESSION, default=0): cv.templatable(cv.uint32_t),
                cv.Optional(CONF_EXPECT_RESPONSE, default=True): cv.boolean,
                cv.Optional(CONF_ON_RESULT): automation.validate_automation(
                    single=True
                ),
            }
        ),
        _validate_command,
    ),
    synchronous=True,
)
async def command_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(
        action_id,
        template_arg,
        parent,
        config[CONF_OPCODE],
        config[CONF_END],
        config[CONF_EXPECT_RESPONSE],
    )
    cg.add(var.set_param1(await cg.templatable(config[CONF_PARAM1], args, cg.uint8)))
    cg.add(var.set_param2(await cg.templatable(config[CONF_PARAM2], args, cg.uint16)))
    cg.add(var.set_session(await cg.templatable(config[CONF_SESSION], args, cg.uint32)))
    await automation.templatable_bytes(
        config[CONF_DATA],
        args,
        var.set_data_template,
        var.set_data_static,
        f"{action_id}_data",
    )
    if (result := config.get(CONF_ON_RESULT)) is not None:
        await automation.build_callback_automation(
            var,
            "set_result_callback",
            [(cg.int_, "status"), (CommandResponse, "data"), (cg.uint32, "session")],
            result,
        )
    return var


automation.register_apply_action(
    f"{DOMAIN}.end_session",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ATSHA204A),
            cv.Required(CONF_SESSION): cv.templatable(cv.uint32_t),
        }
    ),
    automation.ApplyField(CONF_SESSION, "end_session", cg.uint32),
)
