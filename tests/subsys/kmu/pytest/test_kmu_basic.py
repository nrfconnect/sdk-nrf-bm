# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

from collections.abc import Callable
from pathlib import Path

import pytest
from pytest_plugins.utils.cli_commands import provision_keys_for_kmu, reset_board
from twister_harness import DeviceAdapter


@pytest.mark.usefixtures("no_reset")
def test_if_kmu_key_can_be_uploaded_with_west_provision(
    dut: DeviceAdapter, config_reader: Callable
):
    """Verify if KMU key is provisioned correctly."""
    sysbuild_config = Path(dut.device_config.build_dir) / "zephyr" / ".config"
    valid_key_file = config_reader(sysbuild_config).read(
        "SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE"
    )
    valid_keys = [valid_key_file]

    provision_keys_for_kmu(keys=valid_keys, keyname="BL_PUBKEY", dev_id=dut.device_config.id)
    dut.clear_buffer()
    reset_board(dut.device_config.id)

    lines = dut.readlines_until(regex="Hello World!", print_output=True, timeout=20)
    expected_lines = [
        "*Starting bootloader*",
        "*Booting main application*",
        "*Image version: v0.0.0*",
        "*Jumping to the first image slot*",
    ]
    pytest.LineMatcher(lines).fnmatch_lines(expected_lines)


@pytest.mark.usefixtures("no_reset")
def test_if_board_does_not_boot_when_image_is_not_signed_with_correct_key(
    dut: DeviceAdapter, config_reader: Callable, nrf_bm_path: Path
):
    """Verify if a board does not boot if an image is signed with wrong key."""
    invalid_keys = [
        nrf_bm_path / "tests/subsys/kmu/keys/ed25519-1.pem",
        nrf_bm_path / "tests/subsys/kmu/keys/ed25519-2.pem",
        nrf_bm_path / "tests/subsys/kmu/keys/ed25519-3.pem",
    ]

    provision_keys_for_kmu(keys=invalid_keys, keyname="BL_PUBKEY", dev_id=dut.device_config.id)
    dut.clear_buffer()
    reset_board(dut.device_config.id)

    lines = dut.readlines_until(
        regex="Unable to find bootable image", print_output=True, timeout=20
    )
    expected_lines = [
        "*Starting bootloader*",
        "*ED25519 signature verification failed -149*",
        "*Error: no bootable configuration found*",
        "*Unable to find bootable image*",
    ]
    pytest.LineMatcher(lines).fnmatch_lines(expected_lines)
    pytest.LineMatcher(lines).no_fnmatch_line("*Hello World*")


@pytest.mark.usefixtures("no_reset")
def test_if_board_does_not_boot_when_any_key_was_provisioned(dut: DeviceAdapter):
    """Verify if a board does not boot if any key was provisioned."""
    reset_board(dut.device_config.id)

    lines = dut.readlines_until(
        regex="Unable to find bootable image", print_output=True, timeout=20
    )
    expected_lines = [
        "*Starting bootloader*",
        "*Error: no bootable configuration found*",
        "*Unable to find bootable image*",
    ]
    pytest.LineMatcher(lines).fnmatch_lines(expected_lines)
    pytest.LineMatcher(lines).no_fnmatch_line("*Hello World*")
