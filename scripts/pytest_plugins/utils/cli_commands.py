# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import abc
import concurrent.futures
import logging
import multiprocessing
import os
import shlex
import shutil
import subprocess
import time
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Literal

MAX_WORKERS = max(multiprocessing.cpu_count() - 1, 1)

logger = logging.getLogger(__name__)


class WestFlashException(Exception):
    """Raised when west flash failed."""


class WestBuildException(Exception):
    """Raised when west build failed."""


def normalize_path(path: str) -> str:
    path = os.path.expanduser(os.path.expandvars(path))
    path = os.path.normpath(os.path.abspath(path))
    return path


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


class Builder(abc.ABC):
    """Interface class for builders."""

    @abc.abstractmethod
    def build(self) -> tuple[Path, Path]:
        """Run build and return source and build paths as tuple."""


@dataclass
class WestBuilder(Builder):
    source_dir: Path
    build_dir: Path
    board: str
    testsuite: str | None = None
    extra_args: str | None = None
    timeout: int = 60

    @duration
    def build(self) -> tuple[Path, Path]:
        """Run west build for the required build configuration."""
        command = [
            "west",
            "build",
            "-p",
            "-b",
            self.board,
            str(self.source_dir),
            "-d",
            str(self.build_dir),
        ]
        if self.testsuite:
            command.extend(["-T", self.testsuite])
        if self.extra_args:
            command.extend(self.extra_args.split(" "))

        try:
            run_command(command, timeout=self.timeout)
        except subprocess.CalledProcessError:
            # create empty file to indicate a build error, will be used
            # to avoid unnecessary builds in other tests
            Path(self.build_dir / "build.error").touch()
            raise WestBuildException("Failed to build required app") from None
        except subprocess.TimeoutExpired:
            logger.error("Timeout building required app")
            shutil.rmtree(self.build_dir)
            raise WestBuildException("Timeout building required app") from None

        return self.source_dir, self.build_dir


class BuildDirector:
    """Orchestrator for builders to build in paraller."""

    def __init__(self, builders: Sequence[Builder], max_workers: int = MAX_WORKERS) -> None:
        self.builders = builders
        self.max_workers = max_workers
        self.excptions: list[Exception] = []

    def run(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            tasks = {executor.submit(builder.build) for builder in self.builders}
            for future in concurrent.futures.as_completed(tasks):
                try:
                    _, build_dir = future.result()
                except Exception as exc:
                    logger.error('Failed to build: %s', exc)
                    self.excptions.append(exc)
                else:
                    logger.info("Build success: %s", build_dir)
        logger.info("Finished building samples")


def west_flash(
    build_dir: Path, snr: str | None, *, extra_args: str | None = None, timeout: int = 30
) -> None:
    """Run west flash."""
    command = ["west", "flash", "--skip-rebuild", "--build-dir", str(build_dir)]
    if snr:
        command += ["--dev-id", snr]
    if extra_args:
        command.extend(extra_args.split(" "))

    try:
        run_command(command, timeout=timeout)
    except subprocess.CalledProcessError:
        raise WestFlashException("Failed to flash device") from None
    except subprocess.TimeoutExpired:
        logger.error("Timeout flaashing device")
        raise WestFlashException("Timeout flaashing device") from None
