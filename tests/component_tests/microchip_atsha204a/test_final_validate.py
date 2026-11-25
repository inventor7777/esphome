"""Tests for the ATSHA204A single-wire RMT UART wiring requirements."""

from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.core import CORE

RMT_CONFIG = """
esphome:
  name: atsha204a-test
esp32:
  variant: esp32
  framework:
    type: esp-idf
uart:
  id: chip_rx
  rx_pin: {rx_pin}
  baud_rate: 230400
  data_bits: 7
  rx_buffer_size: 1024
{uart_extra}
microchip_atsha204a:
  transport: swi_rmt
  uart_id: chip_rx
"""


@pytest.mark.parametrize(
    ("rx_pin", "uart_extra", "error"),
    [
        ("GPIO21", "", None),
        ("GPIO21", "  tx_pin: GPIO22", "requires an RX-only UART"),
        ("GPIO34", "", "Use a non-inverted, output-capable GPIO"),
        (
            "{number: GPIO21, inverted: true}",
            "",
            "Use a non-inverted, output-capable GPIO",
        ),
        ("GPIO16", "", "Use an RX pin other than GPIO3, GPIO9 or GPIO16"),
    ],
)
def test_rmt_uart_wiring(
    rx_pin: str, uart_extra: str, error: str | None, tmp_path: Path
) -> None:
    path = tmp_path / "atsha204a.yaml"
    path.write_text(
        RMT_CONFIG.format(rx_pin=rx_pin, uart_extra=uart_extra), encoding="utf-8"
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})

    errors = [str(err) for err in result.errors]
    if error is None:
        assert errors == []
    else:
        assert any(error in err for err in errors), errors
