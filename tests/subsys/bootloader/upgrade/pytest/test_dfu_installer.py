# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import logging
import time

import pytest
from pytest_plugins.adapters import nrfutil
from twister_harness import DeviceAdapter

INSTALER_SOFTDEVICE_FIRMWARE_LOADER_NAME: str = "installer_softdevice_firmware_loader.bin"
APPLICATION_NAME: str = "mcuboot_recovery_retention"

logger = logging.getLogger(__name__)


@pytest.mark.parametrize(
    "wait_message",
    [
        "V_DELAY_IMAGE_CHUNK_COPY",
        "V_DELAY_NEXT_IMAGE_COPY",
        "V_DELAY_METADATA_UPDATE",
        "V_DELAY_SELF_DESTRUCTION",
    ],
    ids=[
        "first_image_copy_process",
        "second_image_copy_process",
        "metadata_update",
        "self_destruction",
    ],
)
def test_breaking_image_copy_process_by_installer(
    dut: DeviceAdapter, serial_port: str, wait_message: str
):
    """Break an image copy process by Installer.

    Verify behaviour when copy of an image by Installer has been interrupted.

    - Build a sample with manipulated Installer so it starts uploading with a delay.
    - Program the initial application to the device.
    - Reset the device.
    - Verify if booted correctly.
    - Enter DFU.
    - Upload an updated firmware and reset the device.
    - Reset the device again immediately when installer is copying the updated firmware.
    - Verify if the device repeats copying the updated firmware, and after restart
      boots correctly.
    """
    dut.readlines_until(regex="Boot mode set to bootloader", timeout=10)
    # wait for a reset and boot in Boot mode
    dut.readlines_until(regex="Jumping to the first image slot", timeout=10)

    time.sleep(1)
    nrfutil.image_upload(
        serial_port,
        dut.device_config.build_dir / INSTALER_SOFTDEVICE_FIRMWARE_LOADER_NAME,
        timeout=60,
    )
    nrfutil.reset_board(dut.device_config.id)

    dut.readlines_until(regex=wait_message, timeout=40)

    # give a time for uploading the image to start and reset board before it is in progress
    if wait_message != "V_DELAY_SELF_DESTRUCTION":
        time.sleep(2)
    nrfutil.reset_board(dut.device_config.id)

    dut.readlines_until(regex="Starting bootloader", timeout=30)

    # wait until installer is copied and board is restarted
    dut.readlines_until(regex="Clear installer header OK", timeout=60)

    # Wait for bootloader to start due to missing an application image
    dut.readlines_until(regex="Starting bootloader", timeout=30)
    dut.readlines_until(
        regex="Booting firmware loader due to missing application image", timeout=30
    )

    # upload the application image to make sure that the device can boot
    # after the interrupted copy process
    time.sleep(2)
    nrfutil.image_upload(
        serial_port,
        dut.device_config.build_dir / f"{APPLICATION_NAME}/zephyr/zephyr.signed.bin",
        timeout=60,
    )
    nrfutil.reset_board(dut.device_config.id)

    lines = dut.readlines_until(regex="Boot mode set to bootloader", timeout=10)
    lines += dut.readlines_until(regex="Jumping to the first image slot", timeout=10)
    pytest.LineMatcher(lines).fnmatch_lines(
        [
            "*Starting bootloader*",
            "*Booting firmware loader*",
            "*Jumping to the first image slot*",
        ]
    )
