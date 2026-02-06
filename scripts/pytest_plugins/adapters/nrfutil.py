# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

"""Adapter for nrfutil device management commands.

This module provides functions for interacting with Nordic Semiconductor
devices using the nrfutil command-line tool.
"""

import json
import logging
import subprocess
from pathlib import Path
from typing import Any, Literal

from .common import run_command

RESET_KIND = Literal[
    "RESET_SYSTEM",
    "RESET_HARD",
    "RESET_SOFT",
    "RESET_DEBUG",
    "RESET_PIN",
    "RESET_VIA_SECDOM",
    "RESET_DEFAULT",
]

logger = logging.getLogger(__name__)


def reset_board(dev_id: str | None = None, reset_kind: RESET_KIND = "RESET_DEFAULT") -> None:
    """Reset device."""
    command = ["nrfutil", "device", "reset", "--reset-kind", reset_kind]
    if dev_id:
        command.extend(["--serial-number", dev_id])
    run_command(command)


def erase_board(dev_id: str | None) -> None:
    """Run nrfutil device erase command."""
    command = ["nrfutil", "device", "erase"]
    if dev_id:
        command.extend(["--serial-number", dev_id])
    run_command(command)


def list_devices() -> dict[str, Any]:
    """Return all devices as dictionary."""
    command = ["nrfutil", "device", "list", "--json-pretty", "--skip-overhead"]
    ret = run_command(command)
    try:
        return json.loads(ret.stdout)
    except json.JSONDecodeError:
        return {"devices": []}


def image_upload(
    port: str, image_path: str | Path, timeout: int = 30, check: bool = True
) -> subprocess.CompletedProcess:
    """Run nrfutil image upload.

    :param port: serial port number
    :param image_path: path to an image
    :param timeout: time out in seconds
    :param check: check return code
    """
    command = [
        "nrfutil",
        "mcu-manager",
        "serial",
        "image-upload",
        "--serial-port",
        port,
        "--firmware",
        str(image_path),
    ]
    return run_command(command, timeout=timeout, check=check)
