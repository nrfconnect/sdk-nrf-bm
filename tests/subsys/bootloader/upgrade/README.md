# DFU tests

This directory contains test scenarios for Bare Metal DFU.
It is based on the code from `nrf-bm/samples/boot/mcuboot_recovery_entry`.

## Run tests

To run tests with twister execute the example command:

```sh
west twister -v -ll DEBUG -T nrf-bm/tests/subsys/bootloader/upgrade -p bm_nrf54l15dk/nrf54l15/cpuapp/s115_softdevice/mcuboot --device-testing --device-serial /dev/ttyACM1 --west-flash="--erase"
```
