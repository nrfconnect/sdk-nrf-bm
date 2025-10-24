# KMU tests

This directory contains test scenarios for Bare Metal KMU.

## Run tests

To run tests one must export PYTHONPATH with the path to `nrf-bm/scripts` directory,
so all packages from that directory are available for Python (it is exported in `conftest.py`).

To run tests for `hello_for_kmu` with twister execute the example command:
```sh
$ZEPHYR_BASE/scripts/twister -T $ZEPHYR_BASE/../nrf-bm/tests/subsys/kmu/hello_for_kmu -p bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot --device-testing --device-serial /dev/ttyACM1 --west-flash="--erase"
```
