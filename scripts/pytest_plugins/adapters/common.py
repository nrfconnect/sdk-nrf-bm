# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

"""Common utility functions for adapter modules.

This module provides shared utility functions used across adapter modules.
"""

import logging
import os
import shlex
import subprocess
import time
from collections.abc import Callable
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)


class CommandError(Exception):
    """Raised when subprocess command failed."""


class CommandTimeoutError(Exception):
    """Raised when subprocess command failed due to timeout."""


def normalize_path(path: str) -> str:
    """Normalize a path by expanding user and environment variables."""
    expanded = os.path.expanduser(os.path.expandvars(path))
    return str(Path(expanded).resolve())


def duration(func: Callable) -> Any:
    """Decorator to print duration of calling a function."""

    def _inner(*args, **kwargs):
        start_t = time.monotonic()
        try:
            return func(*args, **kwargs)
        finally:
            logger.debug(
                "The `%s` function took %s seconds to execute",
                func.__name__,
                round(time.monotonic() - start_t, 3),
            )

    return _inner


def run_command(
    command: list[str], timeout: int = 30, *, check: bool = True
) -> subprocess.CompletedProcess:
    """Run command in subprocess.

    :param command: command to run
    :param timeout: time out in seconds
    :param check: if True, returncode will be checked at the end
    :raises CommandError: when command failed
    :raises CommandTimeoutError: when command failed due to time out
    """
    logger.info(f"CMD: {shlex.join(command)}")
    try:
        ret: subprocess.CompletedProcess = subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
        )
    except FileNotFoundError as exc:
        logger.error("Subprocess did not found command: %s", exc)
        raise CommandError(f"Command not found: {command[0]}") from None
    except subprocess.TimeoutExpired as exc:
        logger.error("Command timed out: %s", exc)
        raise CommandTimeoutError(f"Command timed out: {exc}") from None
    except subprocess.SubprocessError as exc:
        logger.error("Subprocess error: %s", exc)
        raise CommandError(f"Command failed due to subprocess error: {exc}") from None

    if ret.returncode:
        logger.info(ret.stdout.rstrip())

    if ret.returncode and check:
        logger.error(f"Failed command: {shlex.join(command)}")
        raise CommandError(f"Command returned code {ret.returncode}: {shlex.join(command)}")
    return ret
