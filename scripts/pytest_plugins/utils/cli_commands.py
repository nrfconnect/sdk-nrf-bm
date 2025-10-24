# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import logging
import os
import shlex
import subprocess
from pathlib import Path
from typing import Literal

logger = logging.getLogger(__name__)


def normalize_path(path: str) -> str:
    path = os.path.expanduser(os.path.expandvars(path))
    path = os.path.normpath(os.path.abspath(path))
    return path


def run_command(command: list[str], timeout: int = 30) -> None:
    logger.info(f"CMD: {shlex.join(command)}")
    ret: subprocess.CompletedProcess = subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )
    if ret.returncode:
        logger.error(f"Failed command: {shlex.join(command)}")
        logger.info(ret.stdout)
        raise subprocess.CalledProcessError(ret.returncode, command)


def reset_board(dev_id: str | None = None):
    """Reset device."""
    command = ["nrfutil", "device", "reset"]
    if dev_id:
        command.extend(["--serial-number", dev_id])
    run_command(command)


def erase_board(dev_id: str | None):
    """Run nrfutil device erase command."""
    command = ["nrfutil", "device", "erase"]
    if dev_id:
        command.extend(["--serial-number", dev_id])
    run_command(command)


def provision_keys_for_kmu(
    keys: list[str] | list[Path] | str | Path,
    *,
    keyname: Literal["UROT_PUBKEY", "BL_PUBKEY", "APP_PUBKEY"] = "BL_PUBKEY",
    policy: Literal["revokable", "lock", "lock-last"] | None = None,
    dev_id: str | None = None,
):
    """Upload keys with west provision command."""
    logger.info("Provision keys using west command.")
    command = ["west", "ncs-provision", "upload", "--keyname", keyname]
    if policy:
        command += ["--policy", policy]
    if dev_id:
        command += ["--dev-id", dev_id]
    if isinstance(keys, list):
        list_of_keys = keys
    else:
        list_of_keys = [keys]
    for key in list_of_keys:
        assert os.path.exists(key), f"Key file does not exist: {key}"
        command += ["--key", normalize_path(str(key))]

    run_command(command)
    logger.info("Keys provisioned successfully")
