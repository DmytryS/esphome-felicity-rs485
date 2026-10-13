"""Exercise the production cell-register decoder with host sanitizers.

Only the pack structure and apply_registers_ method are extracted; hardware
and ESPHome sensor declarations are stubbed so no MCU SDK is needed.
Run: python3 tests/test_cell_decoder.py
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "components/felicity_rs485/felicity_rs485.h").read_text()
source = (ROOT / "components/felicity_rs485/felicity_rs485.cpp").read_text()
pack = header.split("static constexpr uint8_t FELICITY_MAX_CELLS", 1)[1]
pack = "static constexpr uint8_t FELICITY_MAX_CELLS" + pack.split("class FelicityRS485", 1)[0]
decoder = source.split("void FelicityRS485::apply_registers_", 1)[1]
decoder = "void FelicityRS485::apply_registers_" + decoder.split("void FelicityRS485::publish_block_", 1)[0]

harness = r"""
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace sensor { class Sensor; }
namespace binary_sensor { class BinarySensor; }
static uint32_t millis() { return 1000; }
""" + pack + r"""
class FelicityRS485 {
 public:
  void apply_registers_(FelicityPack &, uint16_t, const std::vector<uint16_t> &);
};
""" + decoder + r"""
int main() {
  FelicityRS485 decoder;
  FelicityPack pack;
  // Exact production response: count register followed by 16 cell values.
  std::vector<uint16_t> registers(17, 3300);
  registers[0] = 16;
  for (size_t i = 1; i < registers.size(); ++i)
    registers[i] += i;
  std::fill(std::begin(pack.cells_raw), std::end(pack.cells_raw), 9999);
  decoder.apply_registers_(pack, 16, registers);
  assert(pack.cell_count == 16);
  for (size_t i = 0; i < 16; ++i)
    assert(pack.cells_raw[i] == 3301 + i);
  for (size_t i = 16; i < FELICITY_MAX_CELLS; ++i)
    assert(pack.cells_raw[i] == 0);
  // Reserved storage also tolerates shorter or full-width response data.
  decoder.apply_registers_(pack, 16, {1, 3400});
  assert(pack.cell_count == 1 && pack.cells_raw[0] == 3400);
  for (size_t i = 1; i < FELICITY_MAX_CELLS; ++i)
    assert(pack.cells_raw[i] == 0);
  registers.assign(21, 3350);
  registers[0] = 20;
  decoder.apply_registers_(pack, 16, registers);
  assert(pack.cell_count == 20);
  for (auto cell : pack.cells_raw)
    assert(cell == 3350);
}
"""

with tempfile.TemporaryDirectory(prefix="felicity-cell-test-") as directory:
    fixture = Path(directory) / "cell_decoder.cpp"
    binary = Path(directory) / "cell_decoder"
    fixture.write_text(harness)
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", str(fixture), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS production cell decoder: response bounds and clearing unreturned slots")
