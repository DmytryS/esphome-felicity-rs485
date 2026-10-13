"""Run: uvx --offline --from esphome python tests/test_deye_rs485_example.py"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

import yaml

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "examples/deye-rs485.yaml").read_text()
# BaseLoader inspects !secret references without resolving real credentials.
raw = yaml.load(source, Loader=yaml.BaseLoader)
assert not ({"switch", "number", "select", "button", "interval", "time",
             "external_components", "canbus", "felicity_rs485"} & raw.keys())
assert "create_write" not in source and "write_lambda" not in source
assert "custom_command" not in source and "custom_pdu" not in source
assert raw["logger"]["baud_rate"] == "0"
assert raw["substitutions"]["deye_tx_pin"] == "GPIO16"
assert raw["substitutions"]["deye_rx_pin"] == "GPIO18"
bench = yaml.load((ROOT / "deye-rs485-usb-test.yaml").read_text(), Loader=yaml.BaseLoader)
assert bench["uart"]["tx_pin"] == "GPIO16"
assert bench["uart"]["rx_pin"] == "GPIO18"
assert "ap" not in raw["wifi"]
assert "!secret deye_api_encryption_key" in source
assert "!secret deye_ota_password" in source
names = []
ids = []
for section in ("sensor", "binary_sensor", "text_sensor"):
    for entity in raw[section]:
        if "name" in entity:
            names.append(entity["name"])
        if "id" in entity:
            ids.append(entity["id"])
        if entity["platform"] == "modbus_controller":
            assert entity["register_type"] == "holding"
            assert entity["modbus_controller_id"] == "deye_inverter"
            assert re.fullmatch(r"0|[1-9][0-9]*", entity["address"])
assert len(names) == len(set(names))
assert len(ids) == len(set(ids))
sensors = {entity["name"]: entity for entity in raw["sensor"]}
for name, address, value_type in (
    ("Battery Voltage", "183", "U_WORD"),
    ("Battery Current", "191", "S_WORD"),
    ("Battery Power", "190", "S_WORD"),
    ("Rated Power", "16", "U_DWORD_R"),
    ("Total Battery Charge", "72", "U_DWORD_R"),
    ("Total PV Energy", "96", "U_DWORD_R"),
):
    assert sensors[name]["address"] == address
    assert sensors[name]["value_type"] == value_type
assert sensors["Battery Voltage"]["filters"] == [{"multiply": "0.01"}]
assert sensors["Battery Temperature"]["filters"] == [{"offset": "-1000"}, {"multiply": "0.1"}]
assert "deye_last_telemetry_at" in str(sensors["Battery Voltage"]["on_value"])

with tempfile.TemporaryDirectory(prefix="deye-rs485-config-test-") as directory:
    directory = Path(directory)
    (directory / "secrets.yaml").write_text(
        'wifi_ssid: "validation-only"\nwifi_password: "validation-only-password"\n'
        'deye_api_encryption_key: "MDEyMzQ1Njc4OWFiY2RlZjAxMjM0NTY3ODlhYmNkZWY="\n'
        'deye_ota_password: "validation-only-ota-password"\n'
    )
    config = directory / "deye.yaml"
    config.write_text(source)
    result = subprocess.run([sys.executable, "-m", "esphome", "config", str(config)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    resolved = yaml.load(result.stdout, Loader=yaml.BaseLoader)
    uart = resolved["uart"][0]
    assert uart["tx_pin"]["number"] == "16"
    assert uart["rx_pin"]["number"] == "18"
    assert "baud_rate: 9600" in result.stdout
    assert "address: 0x01" in result.stdout or "address: 1" in result.stdout
    assert "Deye Telemetry Online" in result.stdout
print(f"PASS read-only Deye example: {len(names)} named entities, schema and register checks")
