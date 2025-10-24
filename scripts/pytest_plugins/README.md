# Shared Python's libraries for the `pytest-twister-harness`

The package contains plugins for pytest that can be used in twister tests
and other shared Python modules for testing framework.
It is required to export PYTHONPATH with the path to `nrf-bm\scripts`
so this shared library is available for pytest tests.

For example:
```sh
PYTHONPATH=$PYTHONPATH:$ZEPHYR_BASE/../nrf-bm/scripts $ZEPHYR_BASE/scripts/twister -T $ZEPHYR_BASE/../nrf-bm/tests/subsys/kmu/hello_for_kmu -p bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot --device-testing --device-serial /dev/ttyACM1 --west-flash="--erase"
```
