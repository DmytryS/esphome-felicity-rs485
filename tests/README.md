# Bridge regression checks

Validate the separate read-only inverter example (no hardware access):

```sh
uvx --offline --from esphome python tests/test_deye_rs485_example.py
```

This checks configuration validity, unique entity names/IDs, decimal register
addresses, representative scaling/word order, serial logging disabled, and
absence of writable controls, clock writes and inline credentials.

Check that the production cell decoder does not read beyond the 16 returned
cell registers into its 20-slot storage:

```sh
python3 tests/test_cell_decoder.py
```

This compiles the actual decoder method and pack structure in a temporary
host harness with address/undefined-behavior sanitizers. It also checks that
unreturned slots are cleared instead of retaining stale values.

From the repository root, run the host aggregation test:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_deye_bank.cpp -o /tmp/felicity-test-deye-bank
/tmp/felicity-test-deye-bank
```

It tests the same aggregation helper used by the firmware with simulated
1, 2, 3, 16, 32, and 248-pack inputs, mixed capacities/current limits,
stale/missing data, permission intersection, fault aggregation, timestamp
wraparound, temperature-count overflow, and CAN field bounds. Large simulated
counts test arithmetic, not RS485 throughput or supported physical bank size.

Validate the example's package expansion and address handling:

```sh
uvx --offline --from esphome python tests/test_deye_example.py
```

This uses temporary configs and placeholder Wi-Fi secrets, validates 1, 2,
3 (nonconsecutive addresses), and 16 packs, and checks rejection of duplicate
and empty address lists. It does not flash hardware. Offline mode requires
the ESPHome environment already cached on the machine.
It also checks all scalar sensor fields, 16 cell voltages, ten temperature
slots and six binary flags against the package/schema, verifies unique
per-battery entity names after merging, and checks the web server and bridge
remain enabled.

The actual ESP32 build is checked separately with:

```sh
uvx --offline --from esphome esphome compile examples/felicity-to-deye.yaml
```

Supply `examples/secrets.yaml` for that command. A compile-only run with
placeholder secrets must not be used as an installation firmware.
