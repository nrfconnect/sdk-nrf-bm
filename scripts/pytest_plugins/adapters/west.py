# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

"""Adapter for west commands."""

import logging
import shlex
import shutil
from pathlib import Path
from typing import Literal

from .common import CommandError, CommandTimeoutError, normalize_path, run_command

logger = logging.getLogger(__name__)


class WestFlashException(Exception):
    """Raised when west flash failed."""


class WestBuildException(Exception):
    """Raised when west build failed."""


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
        key_path = Path(key) if not isinstance(key, Path) else key
        assert key_path.exists(), f"Key file does not exist: {key}"
        command += ["--key", normalize_path(str(key_path))]

    run_command(command)
    logger.info("Keys provisioned successfully")


def west_flash(
    build_dir: Path, snr: str | None, *, extra_args: str | None = None, timeout: int = 30
) -> None:
    """Run west flash."""
    command = ["west", "flash", "--skip-rebuild", "--build-dir", str(build_dir)]
    if snr:
        command += ["--dev-id", snr]
    if extra_args:
        command.extend(shlex.split(extra_args, posix=False))

    try:
        ret = run_command(command, timeout=timeout)
        if ret.returncode != 0:
            logger.error("west flash command failed")
            logger.info(ret.stdout.decode())
            raise AssertionError("west flash command failed")
    except CommandTimeoutError:
        logger.error("Timeout flashing device")
        raise WestFlashException("Timeout flashing device") from None
    except CommandError:
        raise WestFlashException("Failed to flash device") from None


def west_build(
    source_dir: Path,
    board: str,
    build_dir: Path,
    *,
    testsuite: str | None = None,
    extra_args: str | None = None,
    timeout: int = 60,
) -> tuple[Path, Path]:
    """Run west build."""
    command = [
        "west",
        "build",
        "-p",
        "-b",
        board,
        str(source_dir),
        "-d",
        str(build_dir),
    ]
    if testsuite:
        command.extend(["-T", testsuite])
    if extra_args:
        command.extend(shlex.split(extra_args, posix=False))

    try:
        ret = run_command(command, timeout=timeout)
        if ret.returncode != 0:
            logger.error("west build command failed")
            logger.info(ret.stdout.decode())
            raise AssertionError("west build command failed")
    except CommandTimeoutError:
        logger.error("Timeout building required app")
        shutil.rmtree(build_dir)
        raise WestBuildException("Timeout building required app") from None
    except CommandError:
        # create empty file to indicate a build error, will be used
        # to avoid unnecessary builds in other tests
        Path(build_dir / "build.error").touch()
        raise WestBuildException("Failed to build required app") from None

    return source_dir, build_dir
