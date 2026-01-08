# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

"""Builder pattern implementation for building Zephyr/NCS projects.

This module provides a builder interface and implementation for building
Nordic Semiconductor projects using the west build system. It includes
support for parallel builds through the BuildDirector orchestrator.
"""

import abc
import concurrent.futures
import logging
import multiprocessing
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

from .common import duration
from .west import west_build

MAX_WORKERS = max(multiprocessing.cpu_count() - 1, 1)

logger = logging.getLogger(__name__)


class Builder(abc.ABC):
    """Interface class for builders."""

    @abc.abstractmethod
    def build(self) -> tuple[Path, Path]:
        """Run build and return source and build paths as tuple."""


@dataclass
class WestBuilder(Builder):
    """Build a sample using west."""

    source_dir: Path
    build_dir: Path
    board: str
    testsuite: str | None = None
    extra_args: str | None = None
    timeout: int = 60

    @duration
    def build(self) -> tuple[Path, Path]:
        """Run west build for the required build configuration.

        :raises WestBuildException: raises when build failed
        """
        return west_build(
            source_dir=self.source_dir,
            board=self.board,
            build_dir=self.build_dir,
            timeout=self.timeout,
            testsuite=self.testsuite,
            extra_args=self.extra_args,
        )


class BuildDirector:
    """Orchestrator for builders to build in parallel."""

    def __init__(self, builders: Sequence[Builder], max_workers: int = MAX_WORKERS) -> None:
        self.builders = builders
        self.max_workers = max_workers
        self.exceptions: list[Exception] = []

    def run(self) -> None:
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            tasks = {executor.submit(builder.build) for builder in self.builders}
            for future in concurrent.futures.as_completed(tasks):
                try:
                    _, build_dir = future.result()
                except Exception as exc:
                    logger.error('Failed to build: %s', exc)
                    self.exceptions.append(exc)
                else:
                    logger.info("Build success: %s", build_dir)
        logger.info("Finished building samples")
