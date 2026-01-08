# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

"""Adapter for nrfutil device management commands.

This module provides functions for interacting with Nordic Semiconductor
devices using the nrfutil command-line tool.
"""

import json
import logging
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
