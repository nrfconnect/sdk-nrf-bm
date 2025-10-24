# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

"""Pytest plugin to share common fixtures among the tests."""

import os
from pathlib import Path

import pytest
from twister_harness.device.device_adapter import DeviceAdapter
from twister_harness.fixtures import determine_scope


@pytest.fixture(scope="session")
def nrf_bm_path() -> Path:
    """Return path to sdk-bm repository."""
    return Path(__file__).parents[2]


@pytest.fixture(scope="session")
def zephyr_base() -> Path:
    """Return path to zephyr repository."""
    return Path(os.getenv("ZEPHYR_BASE", ""))


@pytest.fixture(scope=determine_scope)
def no_reset(device_object: DeviceAdapter):
    """Do not reset after flashing."""
    device_object.device_config.west_flash_extra_args.append("--no-reset")
    yield
    device_object.device_config.west_flash_extra_args.remove("--no-reset")
