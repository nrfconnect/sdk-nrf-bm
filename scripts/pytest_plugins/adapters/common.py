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


def run_command(command: list[str], timeout: int = 30) -> None:
    """Run command in subprocess."""
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
