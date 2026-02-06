# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

"""Helper module for reading edt files."""

import pickle
from pathlib import Path
from typing import Any

from devicetree.dtlib import DT, Node


class EdtPickleReader:
    """Class for reading data from `edt.pickle` file.

    Usage:
        >>> partitions = EdtPickleReader('edt.pickle')
        >>> print(partitions.slot1_partition.size)
        >>> print(partitions.slot1_partition.address)
    """

    def __init__(self, filepath: str | Path) -> None:
        """
        :param filepath: path to `edt.pickle` file
        """
        self._filepath = filepath
        with open(self._filepath, 'rb') as file:
            data = pickle.load(file)
        self.dts: DT = data

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(filepath='{self._filepath}')"

    def __getattr__(self, name: str, /) -> Any:
        try:
            return DtsPartition(self.dts.label2node[name])
        except KeyError:
            return super().__getattribute__(name)


class DtsPartition:
    """Wrapper for DTS Node instance."""

    def __init__(self, node: Node) -> None:
        """
        :param node: DTS node instance
        """
        self.node: Node = node

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}()"

    @property
    def size(self) -> int:
        """Return partition size."""
        return self.node.regs[0].size  # type: ignore

    @property
    def address(self) -> int:
        """Return partition address."""
        return self.node.regs[0].addr  # type: ignore
