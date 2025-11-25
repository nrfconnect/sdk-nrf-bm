# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import logging
from collections.abc import Callable
from pathlib import Path

import pytest
from pytest_plugins.utils.cli_commands import (
    BuildDirector,
    WestBuilder,
    provision_keys_for_kmu,
    reset_board,
    west_flash,
)
from twister_harness import DeviceAdapter

KMU_ROOT: Path = Path(__file__).parents[1]

LINES_FOR_CORRECT_BOOT = [
    "*Starting bootloader*",
    "*Booting main application*",
    "*Jumping to the first image slot*",
    "*Hello World*",
]
LINES_FOR_REVOCED_KEYS = [
    "*Starting bootloader*",
    "*ED25519 signature verification failed -140*",
    "*Error: no bootable configuration found*",
    "*Unable to find bootable image*",
]
LINES_FOR_KEY_VERIFICATION_FAIL = [
    "*Starting bootloader*",
    "*ED25519 signature verification failed -149*",
    "*Error: no bootable configuration found*",
    "*Unable to find bootable image*",
]

logger = logging.getLogger(__name__)


@pytest.mark.usefixtures("no_reset")
@pytest.mark.parametrize(
    "keys_order",
    [
        ("valid",),
        ("valid", "valid", "valid"),
        ("valid", "invalid-1", "invalid-2"),
        ("invalid-1", "valid", "invalid-2"),
        ("invalid-1", "invalid-2", "valid"),
    ],
    ids=("one-valid-key", "all-valid", "first-valid", "second-valid", "third-valid"),
)
def test_if_kmu_keys_revocation_with_three_slots(
    dut: DeviceAdapter, config_reader: Callable, nrf_bm_path: Path, keys_order: tuple[str, ...]
):
    """Verify if KMU keys are provisioned correctly."""
    sysbuild_config = Path(dut.device_config.build_dir) / "zephyr" / ".config"
    valid_key_file = config_reader(sysbuild_config).read(
        "SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE"
    )
    keys_dict = {
        "valid": valid_key_file,
        "invalid-1": nrf_bm_path / "tests/subsys/kmu/keys/ed25519-2.pem",
        "invalid-2": nrf_bm_path / "tests/subsys/kmu/keys/ed25519-3.pem",
    }
    keys = [keys_dict[key] for key in keys_order]

    provision_keys_for_kmu(keys=keys, keyname="BL_PUBKEY", dev_id=dut.device_config.id)
    dut.clear_buffer()
    reset_board(dut.device_config.id)

    lines = dut.readlines_until(regex="Hello World!", print_output=True, timeout=20)
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_CORRECT_BOOT)


@pytest.mark.usefixtures("no_reset")
@pytest.mark.parametrize(
    "keys_order",
    [
        ("valid", "invalid-1", "invalid-2"),
        ("invalid-1", "valid", "invalid-2"),
    ],
    ids=("first-valid", "second-valid"),
)
def test_if_kmu_keys_revocation_with_two_slots(
    dut: DeviceAdapter, config_reader: Callable, nrf_bm_path: Path, keys_order: tuple[str, ...]
):
    """Verify if KMU keys are provisioned correctly."""
    sysbuild_config = Path(dut.device_config.build_dir) / "zephyr" / ".config"
    valid_key_file = config_reader(sysbuild_config).read(
        "SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE"
    )
    keys_dict = {
        "valid": valid_key_file,
        "invalid-1": nrf_bm_path / "tests/subsys/kmu/keys/ed25519-2.pem",
        "invalid-2": nrf_bm_path / "tests/subsys/kmu/keys/ed25519-3.pem",
    }
    keys = [keys_dict[key] for key in keys_order]

    provision_keys_for_kmu(keys=keys, keyname="BL_PUBKEY", dev_id=dut.device_config.id)
    dut.clear_buffer()
    reset_board(dut.device_config.id)

    lines = dut.readlines_until(regex="Hello World!", print_output=True, timeout=20)
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_CORRECT_BOOT)


@pytest.mark.usefixtures("no_reset")
def test_if_kmu_keys_revocation_with_two_slots_third_key_valid(
    dut: DeviceAdapter, config_reader: Callable, nrf_bm_path: Path
):
    """Verify if KMU keys are provisioned correctly."""
    sysbuild_config = Path(dut.device_config.build_dir) / "zephyr" / ".config"
    valid_key_file = config_reader(sysbuild_config).read(
        "SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE"
    )
    keys_dict = {
        "valid": valid_key_file,
        "invalid-1": nrf_bm_path / "tests/subsys/kmu/keys/ed25519-2.pem",
        "invalid-2": nrf_bm_path / "tests/subsys/kmu/keys/ed25519-3.pem",
    }
    keys_order = ("invalid-1", "invalid-2", "valid")
    keys = [keys_dict[key] for key in keys_order]

    provision_keys_for_kmu(keys=keys, keyname="BL_PUBKEY", dev_id=dut.device_config.id)
    dut.clear_buffer()
    reset_board(dut.device_config.id)

    lines = dut.readlines_until(
        regex="Unable to find bootable image", print_output=True, timeout=20
    )
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_KEY_VERIFICATION_FAIL)


@pytest.mark.xfail(reason="Won't work until https://github.com/nrfconnect/sdk-mcuboot/pull/564")
@pytest.mark.usefixtures("no_reset")
def test_if_previous_key_is_revoked_when_flashing_new_image(
    dut: DeviceAdapter, config_reader: Callable, nrf_bm_path: Path, request: pytest.FixtureRequest
):
    """Test if previous key is revoked when flashing with new image."""
    sysbuild_config = Path(dut.device_config.build_dir) / "zephyr" / ".config"
    valid_key_file = config_reader(sysbuild_config).read(
        "SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE"
    )
    keys = {
        1: valid_key_file,
        2: nrf_bm_path / "tests/subsys/kmu/keys/ed25519-2.pem",
        3: nrf_bm_path / "tests/subsys/kmu/keys/ed25519-3.pem",
    }

    build_dir_1 = dut.device_config.build_dir
    # build second image signed with different key
    build_dir_2 = dut.device_config.build_dir.parent / f"{request.node.name}_key2"
    testsuite: str = dut.device_config.build_dir.name  # "mcuboot.kmu.west.provision.key_revocation"
    WestBuilder(
        source_dir=KMU_ROOT / dut.device_config.build_dir.parent.name,
        build_dir=build_dir_2,
        board=dut.device_config.platform,
        testsuite=testsuite,
        extra_args=f"-DSB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE=\"{keys[2]}\"",
        timeout=120,
    ).build()

    # provision all keys
    provision_keys_for_kmu(
        keys=list(keys.values()), keyname="BL_PUBKEY", dev_id=dut.device_config.id
    )
    dut.clear_buffer()
    reset_board(dut.device_config.id)

    lines = dut.readlines_until(regex="Hello World!", print_output=True, timeout=20)
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_CORRECT_BOOT)

    logger.info("Flash DUT with the second image")
    west_flash(build_dir_2, dut.device_config.id)
    dut.clear_buffer()
    reset_board(dut.device_config.id)
    lines = dut.readlines_until(regex="Hello World!", print_output=True, timeout=20)
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_CORRECT_BOOT)

    logger.info("Flash DUT with the first image")
    west_flash(build_dir_1, dut.device_config.id)
    dut.clear_buffer()
    reset_board(dut.device_config.id)

    # verify that DUT does not boot because the key was revoked in previous step
    lines = dut.readlines_until(
        regex="Unable to find bootable image|Hello World!", print_output=True, timeout=20
    )
    pytest.LineMatcher(lines).no_fnmatch_line("*Hello World!*")
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_KEY_VERIFICATION_FAIL)


@pytest.mark.usefixtures("no_reset")
def test_if_revocation_of_last_remaining_key_is_not_allowed(
    dut: DeviceAdapter, config_reader: Callable, nrf_bm_path: Path, request: pytest.FixtureRequest
):
    """Prevent Revocation of Last Remaining Key.

    - Build images signed with key1, key2 and key3
    - Provison Dut with 2 keys (key1 and key2)
    - Flash device with the image signed with key1 and reset
    - Flash device with the image signed with key2 and reset
    - Flash device with the image signed with key3 and reset
    - Verified that Dut does not boot due to the 3rd image is not signed with valid key
    - Flash device with image signed with key2
    - Verify that Dut boots correctly and `Hello World` is printed to UART console
    """
    sysbuild_config = Path(dut.device_config.build_dir) / "zephyr" / ".config"
    valid_key_file = config_reader(sysbuild_config).read(
        "SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE"
    )

    keys = {
        1: valid_key_file,
        2: nrf_bm_path / "tests/subsys/kmu/keys/ed25519-2.pem",
        3: nrf_bm_path / "tests/subsys/kmu/keys/ed25519-3.pem",
    }

    # build images signed with different keys
    source_dir = KMU_ROOT / dut.device_config.build_dir.parent.name
    build_dir_2 = dut.device_config.build_dir.parent / f"{request.node.name}_key2"
    build_dir_3 = dut.device_config.build_dir.parent / f"{request.node.name}_key3"
    testsuite: str = "mcuboot.kmu.west.provision.key_revocation"
    builders = [
        WestBuilder(
            source_dir=source_dir,
            build_dir=build_dir_2,
            board=dut.device_config.platform,
            testsuite=testsuite,
            extra_args=f"-DSB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE=\"{keys[2]}\"",
            timeout=120,
        ),
        WestBuilder(
            source_dir=source_dir,
            build_dir=build_dir_3,
            board=dut.device_config.platform,
            testsuite=testsuite,
            extra_args=f"-DSB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE=\"{keys[3]}\"",
            timeout=120,
        ),
    ]

    build_director = BuildDirector(builders)
    build_director.run()
    assert not build_director.excptions, "Building samples failed"

    logger.info("Provision DUT with two keys")
    provision_keys_for_kmu(
        keys=list(keys.values())[:2], keyname="BL_PUBKEY", dev_id=dut.device_config.id
    )
    dut.clear_buffer()
    reset_board(dut.device_config.id)

    lines = dut.readlines_until(regex="Hello World!", print_output=True, timeout=20)
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_CORRECT_BOOT)

    logger.info("Flash DUT with the second image")
    west_flash(build_dir_2, dut.device_config.id, extra_args="--no-reset")
    dut.clear_buffer()
    reset_board(dut.device_config.id)
    lines = dut.readlines_until(regex="Hello World!", print_output=True, timeout=20)
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_CORRECT_BOOT)

    logger.info("Flash DUT with the third image")
    west_flash(build_dir_3, dut.device_config.id, extra_args="--no-reset")
    dut.clear_buffer()
    reset_board(dut.device_config.id)
    lines = dut.readlines_until(
        regex="Unable to find bootable image|Hello World!", print_output=True, timeout=20
    )
    pytest.LineMatcher(lines).no_fnmatch_line("*Hello World!*")
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_REVOCED_KEYS)

    logger.info("Flash DUT with the second image")
    west_flash(build_dir_2, dut.device_config.id, extra_args="--no-reset")
    dut.clear_buffer()
    reset_board(dut.device_config.id)
    # Check if Dut boots correctly and `Hello World` is printed
    lines = dut.readlines_until(
        regex="Unable to find bootable image|Hello World!", print_output=True, timeout=20
    )
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_CORRECT_BOOT)


@pytest.mark.usefixtures("no_reset")
def test_boot_failure_when_kmu_key_is_missing(
    dut: DeviceAdapter, config_reader: Callable, nrf_bm_path: Path, request: pytest.FixtureRequest
):
    """Verify if DUT does not boot when the key was not provisioned.

    - Flash DUT with signed image
    - Reset DUT without provisinonig any KMU keys
    - Verify if DUT does not boot
    """
    dut.clear_buffer()
    reset_board(dut.device_config.id)
    # Check if Dut boots correctly
    lines = dut.readlines_until(
        regex="Unable to find bootable image|Hello World!", print_output=True, timeout=20
    )
    pytest.LineMatcher(lines).no_fnmatch_line("*Hello World!*")
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_REVOCED_KEYS)


@pytest.mark.usefixtures("no_reset")
def test_dut_does_not_boot_when_flashed_with_image_signed_with_wrong_key(
    dut: DeviceAdapter, nrf_bm_path: Path
):
    """Verify if DUT does not boot when flashed with an image signed with wrong key."""
    keys_dict = {
        1: nrf_bm_path / "tests/subsys/kmu/keys/ed25519-1.pem",
        2: nrf_bm_path / "tests/subsys/kmu/keys/ed25519-2.pem",
        3: nrf_bm_path / "tests/subsys/kmu/keys/ed25519-3.pem",
    }
    keys = [str(value) for value in keys_dict.values()]

    provision_keys_for_kmu(keys=keys, keyname="BL_PUBKEY", dev_id=dut.device_config.id)
    reset_board(dut.device_config.id)
    lines = dut.readlines_until(
        regex="Unable to find bootable image|Hello World!", print_output=True, timeout=20
    )
    pytest.LineMatcher(lines).no_fnmatch_line("*Hello World!*")
    pytest.LineMatcher(lines).fnmatch_lines(LINES_FOR_KEY_VERIFICATION_FAIL)
