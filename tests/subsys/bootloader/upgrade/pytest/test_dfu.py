# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import time
from pathlib import Path
from typing import Any

import pytest
from pytest_plugins.adapters import nrfutil
from pytest_plugins.adapters.builder import WestBuilder
from pytest_plugins.adapters.dts import EdtPickleReader
from pytest_plugins.adapters.imgtool import sign_image
from pytest_plugins.utils.expand_firmware import convert_bin_to_hex, expand_hex_file
from twister_harness import DeviceAdapter, MCUmgr

TEST_ROOT_DIR: Path = Path(__file__).parents[1]
APPLICATION_NAME: str = "mcuboot_recovery_retention"
EXTRA_IMAGE_SIZE = 0x1000  # 4KB extra to exceed partition size
TLV_TRAILER_SIZE = 8 * 1024  # 8KB for MCUboot metadata (TLV + trailer)


def get_available_ports(dut: DeviceAdapter) -> list[str]:
    """Return list of UART ports."""
    serial_number, port = dut.device_config.id, dut.device_config.serial_configs[0].port
    devices: list[dict[str, Any]] = nrfutil.list_devices()["devices"]

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
    nrfutil.reset_board(dut.device_config.id)
    lines = dut.readlines_until(
        regex="Booting firmware loader due to missing application image", timeout=10
    )
    pytest.LineMatcher(lines).fnmatch_lines(["*Failed loading application/installer image header*"])

    # Upload again all images and verify it DUT boots correctly
    mcumgr.image_upload(dut.device_config.build_dir / "installer_softdevice_firmware_loader.bin")
    dut.clear_buffer()
    nrfutil.reset_board(dut.device_config.id)

    lines = dut.readlines_until(
        regex="Booting firmware loader due to missing application image", timeout=10
    )

    # upload application
    time.sleep(1)
    mcumgr.image_upload(
        dut.device_config.build_dir / f"{APPLICATION_NAME}/zephyr/zephyr.signed.bin"
    )
    dut.clear_buffer()
    nrfutil.reset_board(dut.device_config.id)
    dut.readlines_until(regex="Boot mode set to bootloader", timeout=10)
    lines = dut.readlines_until(regex="Jumping to the first image slot", timeout=10)
    pytest.LineMatcher(lines).fnmatch_lines(
        [
            "*Starting bootloader*",
            "*Booting firmware loader*",
            "*Jumping to the first image slot*",
        ]
    )


def test_dfu_upload_too_large_app(dut: DeviceAdapter, zephyr_base, tmp_path, config_reader):
    """Test uploading too large application.

    Should not be possible to upload an image which does not fit size of a partition.

    - Build updated SoftDevice image which is too large to fit partition size for SoftDevice
        (e.g. manipulate partition size to get such image).
    - Program the initial application to the device.
    - Verify if booted correctly.
    - Enter DFU.
    - Upload a firmware image.
    - Verify that uploading an image that is too large for slot0 partition is not possible.
    - Reset DUT.
    - Verify if DUT boots correctly.
    """
    app_name = "mcuboot_recovery_retention"
    bin_path = dut.device_config.build_dir / "mcuboot_recovery_retention/zephyr/zephyr.bin"
    tmp_hex_path = tmp_path / "expanded.zephyr.temp.hex"
    output_hex = dut.device_config.build_dir / "expanded.zephyr.hex"
    signed_output_zephyr = dut.device_config.build_dir / "expanded.zephyr.signed.hex"

    config = config_reader(dut.device_config.build_dir / "zephyr" / ".config")
    key_file = config.read("SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE")
    app_config = config_reader(dut.device_config.build_dir / app_name / "zephyr" / ".config")
    header_size = app_config.read("CONFIG_ROM_START_OFFSET")

    partitions = EdtPickleReader(dut.device_config.build_dir / app_name / "zephyr" / "edt.pickle")
    partition_address = partitions.slot0_partition.address
    partition_size = partitions.slot0_partition.size

    assert convert_bin_to_hex(str(bin_path), str(tmp_hex_path), partition_address), (
        f"Cannot convert {bin_path}"
    )

    assert expand_hex_file(str(tmp_hex_path), str(output_hex), partition_size, EXTRA_IMAGE_SIZE), (
        f"Cannot expand the hex file: {tmp_hex_path}"
    )

    slot_size = partition_size + EXTRA_IMAGE_SIZE + TLV_TRAILER_SIZE
    sign_image(
        input_file=output_hex,
        signed_file=signed_output_zephyr,
        header_size=header_size,
        key_file=key_file,
        slot_size=slot_size,
        version="2.0.0+0",
    )

    dut.readlines_until(regex="Boot mode set to bootloader", timeout=10)
    dut.readlines_until(regex="Jumping to the first image slot", timeout=5)

    time.sleep(3)  # wait for the application to start
    serial_port = get_available_ports(dut)[0]
    # using nrfutil to upload an image due to mcumgr hangs on upload too large image
    ret = nrfutil.image_upload(serial_port, signed_output_zephyr, check=False)
    assert ret.returncode == 1, f"Expected return code 1, but was {ret.returncode}"
    assert "SMP request returned error code 1 (MGMT_ERR_EUNKNOWN: Unknown error)" in ret.stdout

    nrfutil.reset_board(dut.device_config.id)
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
