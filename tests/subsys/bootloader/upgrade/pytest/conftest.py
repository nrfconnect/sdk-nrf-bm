# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
import os
import sys
from pathlib import Path
from typing import Any

import pytest
from pytest_plugins.adapters.nrfutil import list_devices
from twister_harness import DeviceAdapter, MCUmgr

# the `nrf-bm/scripts` directory must be added to the PYTHONPATH
SCRIPTS_DIR = Path(os.environ.get("ZEPHYR_BASE", "")).parent.joinpath("nrf-bm/scripts")
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

pytest_plugins = ["pytest_plugins.plugin"]


def get_available_ports(dut: DeviceAdapter) -> list[str]:
    """Return list of UART ports."""
    serial_number, port = dut.device_config.id, dut.device_config.serial_configs[0].port
    devices: list[dict[str, Any]] = list_devices()["devices"]

    # if device id is not provided try to find it having its serial port
    if serial_number is None and port:
        for device in devices:
            for serial_port in device["serialPorts"]:
                if serial_port["path"] == port:
                    serial_number = device["serialNumber"]
                    break
            if serial_number:
                break

    if serial_number is None:
        raise ValueError(f"Cannot match port {port} to a device")

    for device in devices:
        if device["serialNumber"] == serial_number:
            return [serial_port["path"] for serial_port in device["serialPorts"]]

    raise ValueError(f"Cannot find a device with serial number: {serial_number}")


@pytest.fixture
def serial_port(dut: DeviceAdapter) -> str:
    """Return serial port for smp svr interface."""
    # we know that we need the first port for this board and sample
    return get_available_ports(dut)[0]


@pytest.fixture
def mcumgr(serial_port) -> MCUmgr:
    return MCUmgr.create_for_serial(serial_port)
