# Copyright (c) 2025 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import os
import sys
from pathlib import Path

# Paths ------------------------------------------------------------------------

NRF_BM_BASE = Path(__file__).absolute().parents[2]

sys.path.insert(0, str(NRF_BM_BASE / "doc" / "_utils"))
import utils  # noqa: E402

ZEPHYR_BASE = NRF_BM_BASE / ".." / "zephyr"
NRF_BASE = NRF_BM_BASE / ".." / "nrf"

# General configuration --------------------------------------------------------

project = "Kconfig reference"
copyright = "2025, Nordic Semiconductor"
author = "Nordic Semiconductor"
# NOTE: use blank space as version to preserve space
version = "&nbsp;"

sys.path.insert(0, str(ZEPHYR_BASE / "doc" / "_extensions"))

extensions = ["zephyr.kconfig", "zephyr.external_content"]

# Options for HTML output ------------------------------------------------------

html_theme = "sphinx_ncs_theme"
html_static_path = [str(NRF_BM_BASE / "doc" / "_static")]
html_title = project
html_last_updated_fmt = "%b %d, %Y"
html_show_sourcelink = True
html_show_sphinx = False

html_theme_options = {
    "docset": "kconfig",
    "docsets": utils.ALL_DOCSETS,
    "prev_next_buttons_location": None,
}

# Options for external_content -------------------------------------------------

external_content_contents = [
    (NRF_BM_BASE / "doc" / "kconfig", "*.rst"),
]

# Options for zephyr.kconfig -----------------------------------------------------

kconfig_generate_db = True
kconfig_ext_paths = [ZEPHYR_BASE, NRF_BASE, NRF_BM_BASE]

# Adding NCS_ specific entries. Can be removed when the NCSDK-14227 improvement
# task has been completed.
os.environ["NCS_MEMFAULT_FIRMWARE_SDK_KCONFIG"] = str(
    NRF_BASE / "modules" / "memfault-firmware-sdk" / "Kconfig"
)
os.environ["ZEPHYR_NRF_KCONFIG"] = str(NRF_BASE / "Kconfig.nrf")
os.environ["SYSBUILD_NRF_KCONFIG"] = str(NRF_BASE / "sysbuild" / "Kconfig.sysbuild")

# FIXME: should be automatically added
os.environ["ZEPHYR_NRF_MODULE_DIR"] = str(NRF_BASE)
os.environ["ZEPHYR_NRF_BM_MODULE_DIR"] = str(NRF_BM_BASE)
