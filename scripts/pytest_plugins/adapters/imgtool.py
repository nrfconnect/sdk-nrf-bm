# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import os
from pathlib import Path

from pytest_plugins.adapters.common import run_command


def _get_imgtool_path() -> Path:
    if zephyr_base := os.environ.get("ZEPHYR_BASE"):
        return Path(f"{Path(zephyr_base).parent}/bootloader/mcuboot/scripts/imgtool.py")
    raise ValueError("Cannot find path to imgtool.py")


def sign_image(
    input_file: str | Path,
    signed_file: str | Path,
    key_file: str,
    header_size: int,
    slot_size: int = 0,
    version: str = "0.0.0+0",
):
    """Sign hex file."""
    cmd = [
        str(_get_imgtool_path()),
        "sign",
        f"--version={version}",
        f"--header-size={header_size}",
        f"--slot-size={slot_size}",
        "--pure",
        "--overwrite-only",
        "--align=1",
        f"--key={key_file}",
        str(input_file),
        str(signed_file),
    ]
    return run_command(cmd)
