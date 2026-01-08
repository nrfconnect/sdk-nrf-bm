# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
import os
import sys
from pathlib import Path

# the `nrf-bm/scripts` directory must be added to the PYTHONPATH
SCRIPTS_DIR = Path(os.environ.get("ZEPHYR_BASE", "")).parent.joinpath("nrf-bm/scripts")
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

pytest_plugins = ["pytest_plugins.plugin"]
