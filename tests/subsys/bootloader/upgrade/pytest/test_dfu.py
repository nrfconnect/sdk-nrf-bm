# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import time
from pathlib import Path
from typing import Any

import pytest
from pytest_plugins.adapters.builder import WestBuilder
from pytest_plugins.adapters.nrfutil import list_devices, reset_board
from twister_harness import DeviceAdapter, MCUmgr

TEST_ROOT_DIR: Path = Path(__file__).parents[1]
APPLICATION_NAME: str = "mcuboot_recovery_retention"


def get_available_ports(dut: DeviceAdapter) -> list[str]:
    """Return list of UART ports."""
    serial_number, port = dut.device_config.id, dut.device_config.serial
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
def mcumgr(dut: DeviceAdapter) -> MCUmgr:
    # we know that we need the first port for this board and sample
    serial_port = get_available_ports(dut)[0]
    mcumgr = MCUmgr.create_for_serial(serial_port)
    return mcumgr


def test_if_uploading_too_large_softdevice_image_is_not_possible(
    dut: DeviceAdapter, request: pytest.FixtureRequest, mcumgr: MCUmgr
):
    """Uploading too large SoftDevice image.

    Should not be possible to upload an image which does not fit size of a partition.

    - Build updated SoftDevice image which is too large to fit partition size for SoftDevice
      (e.g. manipulate partition size to get such image or using different softdevice version).
    - Program the initial application to the device.
    - Verify if booted correctly.
    - Enter DFU.
    - Upload SoftDevice, firmware loader and Installer.
    - Verify that the device enters DFU mode due to it cannot upload SoftDevice
      because its size is too big to fit the SoftDevice partition size.
    """
    source_dir = TEST_ROOT_DIR / dut.device_config.build_dir.parent.name
    build_dir = dut.device_config.build_dir.parent / f"{request.node.name}_s145_softdevice"
    testsuite = "boot.mcuboot_recovery_retention.uart"

    dut.readlines_until(regex="Boot mode set to bootloader", timeout=10)
    # wait for a reset and boot in Boot mode
    lines = dut.readlines_until(regex="Jumping to the first image slot", timeout=10)
    pytest.LineMatcher(lines).fnmatch_lines(
        [
            "*Starting bootloader*",
            "*Booting firmware loader*",
            "*Jumping to the first image slot*",
        ]
    )

    # build a sample that does not fit a softdevice partition size
    board = dut.device_config.platform.replace("s115_softdevice", "s145_softdevice")
    builder = WestBuilder(
        source_dir=source_dir,
        build_dir=build_dir,
        board=board,
        testsuite=testsuite,
        timeout=120,
    )
    builder.build()

    assert mcumgr.get_image_list()

    mcumgr.image_upload(build_dir / "installer_softdevice_firmware_loader.bin")
    dut.clear_buffer()
    reset_board(dut.device_config.id)
    lines = dut.readlines_until(
        regex="Booting firmware loader due to missing application image", timeout=10
    )
    pytest.LineMatcher(lines).fnmatch_lines(["*Failed loading application/installer image header*"])

    # Upload again all images and verify it DUT boots correctly
    mcumgr.image_upload(dut.device_config.build_dir / "installer_softdevice_firmware_loader.bin")
    dut.clear_buffer()
    reset_board(dut.device_config.id)

    lines = dut.readlines_until(
        regex="Booting firmware loader due to missing application image", timeout=10
    )

    # upload application
    time.sleep(1)
    mcumgr.image_upload(
        dut.device_config.build_dir / f"{APPLICATION_NAME}/zephyr/zephyr.signed.bin"
    )
    dut.clear_buffer()
    reset_board(dut.device_config.id)
    dut.readlines_until(regex="Boot mode set to bootloader", timeout=10)
    lines = dut.readlines_until(regex="Jumping to the first image slot", timeout=10)
    pytest.LineMatcher(lines).fnmatch_lines(
        [
            "*Starting bootloader*",
            "*Booting firmware loader*",
            "*Jumping to the first image slot*",
        ]
    )
