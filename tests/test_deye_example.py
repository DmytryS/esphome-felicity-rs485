"""Run with: uvx --offline --from esphome python tests/test_deye_example.py"""
from pathlib import Path
import ast
import re
import subprocess
import sys
import tempfile

import yaml

ROOT = Path(__file__).resolve().parents[1]
template = (ROOT / "examples/felicity-to-deye.yaml").read_text()
package = ROOT / "examples/packages/felicity-deye-battery.yaml"

# Compare against the actual component schema so new scalar fields cannot
# silently disappear from the full-telemetry example.
schema = ast.parse((ROOT / "components/felicity_rs485/sensor.py").read_text())
rows = next(node.value for node in schema.body if isinstance(node, ast.Assign)
            and any(isinstance(target, ast.Name) and target.id == "SENSORS" for target in node.targets))
expected = set()
for row in rows.elts:
    fields = {key.value: value for key, value in zip(row.keys, row.values)}
    key = ast.literal_eval(fields["key"])
    if "count" in fields:
        # Only 16 cell registers are read; all ten temperature slots are read.
        count = 16 if key == "cell_voltage" else ast.literal_eval(fields["count"])
        expected.update(f"{key}_{index}" for index in range(1, count + 1))
    else:
        expected.add(key)
package_config = yaml.safe_load(package.read_text())
metadata = {"platform", "felicity_rs485_id", "address"}
assert set(package_config["sensor"][0]) - metadata == expected
assert set(package_config["binary_sensor"][0]) - metadata == {
    "online", "charging", "discharging", "charge_enabled", "discharge_enabled", "fault"
}
entity_templates = [entity["name"] for section in ("sensor", "binary_sensor")
                    for key, entity in package_config[section][0].items() if key not in metadata]
assert len(entity_templates) == len(set(entity_templates)), "Duplicate entity names"

with tempfile.TemporaryDirectory(prefix="felicity-config-test-") as directory:
    config = Path(directory) / "bridge.yaml"
    (Path(directory) / "secrets.yaml").write_text(
        'wifi_ssid: "validation-only"\nwifi_password: "validation-only-password"\n'
    )
    for addresses in ([1], [1, 2], [1, 3, 7], list(range(1, 17)), [1, 1], []):
        entries = "\n".join(
            f'  battery_{i}: !include {{file: "{package}", vars: {{battery_address: "{address}"}}}}'
            for i, address in enumerate(addresses)
        )
        body = re.sub(r"packages:\n.*?\nesphome:", f"packages:\n{entries}\n\nesphome:", template, flags=re.S)
        body = body.replace("path: ../components", f'path: "{ROOT / "components"}"')
        if not addresses:
            body = body.replace("packages:\n\n", "packages: {}\n")
            body = body.replace("  deye_can_bridge: true", "  deye_can_bridge: true\n  pack_addresses: []")
        config.write_text(body)
        result = subprocess.run(
            [sys.executable, "-m", "esphome", "config", str(config)], capture_output=True, text=True
        )
        if not addresses or len(set(addresses)) != len(addresses):
            assert result.returncode != 0, "Invalid address list unexpectedly accepted"
            expected = "At least one pack address" if not addresses else "Pack addresses must be unique"
            assert expected in result.stdout + result.stderr, result.stdout + result.stderr
        else:
            assert result.returncode == 0, result.stdout + result.stderr
            merged = re.search(r"  pack_addresses:\n((?:    - \d+\n)+)", result.stdout)
            assert merged, result.stdout
            assert sorted(map(int, re.findall(r"\d+", merged[1]))) == sorted(addresses)
            for address in addresses:
                for name in entity_templates:
                    expanded = name.replace("${battery_address}", str(address))
                    assert result.stdout.count(f"name: {expanded}\n") == 1, expanded
            assert result.stdout.count("platform: felicity_rs485") == 2 * len(addresses)
            assert "web_server:" in result.stdout
            assert "port: 80" in result.stdout
            assert "deye_can_bridge: true" in result.stdout
        print(f"PASS example addresses={addresses}")
