# Copyright (c) 2025 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import os
import sys
from pathlib import Path

# Paths ------------------------------------------------------------------------

NRF_BM_BASE = Path(__file__).absolute().parents[2]

sys.path.insert(0, str(NRF_BM_BASE / "doc" / "_utils"))
import redirects  # noqa: E402
import utils  # noqa: E402

ZEPHYR_BASE = NRF_BM_BASE / ".." / "zephyr"

# General configuration --------------------------------------------------------

project = "nRF Connect SDK Bare Metal"
copyright = "2025, Nordic Semiconductor"
author = "Nordic Semiconductor"
version = release = os.environ.get("DOCSET_VERSION")

sys.path.insert(0, str(ZEPHYR_BASE / "doc" / "_extensions"))

extensions = [
    "zephyr.html_redirects",
    "zephyr.kconfig",
    "zephyr.external_content",
    "zephyr.doxyrunner",
    "zephyr.doxybridge",
    "sphinx_tabs.tabs",
    "sphinx_togglebutton",
    "sphinx_copybutton",
    "sphinx.ext.intersphinx",
]

rst_epilog = """
.. include:: /substitutions.txt
.. include:: /links.txt
.. include:: /shortcuts.txt
"""

# -- Options for HTML output -------------------------------------------------

html_theme = "sphinx_ncs_theme"
html_static_path = [str(NRF_BM_BASE / "doc" / "_static")]
html_last_updated_fmt = "%b %d, %Y"
html_show_sourcelink = True
html_show_sphinx = False

html_theme_options = {"docset": "nrf-bm", "docsets": utils.ALL_DOCSETS}

# Options for intersphinx ------------------------------------------------------

intersphinx_mapping = dict()

kconfig_mapping = utils.get_intersphinx_mapping("kconfig")
if kconfig_mapping:
    intersphinx_mapping["kconfig"] = kconfig_mapping

# Options for external_content -------------------------------------------------
SOFTDEVICE_DIR = NRF_BM_BASE / "components" / "softdevice"

external_content_contents = [
    (NRF_BM_BASE / "doc" / "nrf-bm", "*"),
    (NRF_BM_BASE, "samples/**/*.rst"),
    (SOFTDEVICE_DIR / "v9" / "nrf54l" / "s115" / "doc", "s115*.main.rst"),
    (SOFTDEVICE_DIR / "v9" / "nrf54l" / "s145" / "doc", "s145*.main.rst"),
]

external_content_keep = ["versions.txt"]
# -- Options for doxyrunner plugin ---------------------------------------------

_doxyrunner_outdir = utils.get_builddir() / "html" / "nrf-bm" / "doxygen"

doxyrunner_doxygen = os.environ.get("DOXYGEN_EXECUTABLE", "doxygen")
doxyrunner_projects = {
    "nrf": {
        "doxyfile": NRF_BM_BASE / "doc" / "nrf-bm" / "nrf-bm.doxyfile.in",
        "outdir": _doxyrunner_outdir,
        "fmt": True,
        "fmt_vars": {
            "NRF_BM_BASE": str(NRF_BM_BASE),
            "DOCSET_SOURCE_BASE": str(NRF_BM_BASE),
            "DOCSET_BUILD_DIR": str(_doxyrunner_outdir),
            "DOCSET_VERSION": version,
        },
    }
}

# -- Options for doxybridge plugin ---------------------------------------------

doxybridge_projects = {
    "nrf-bm": _doxyrunner_outdir,
    "s115": utils.get_builddir() / "html" / "s115",
    "s145": utils.get_builddir() / "html" / "s145",
}

# Options for html_redirect ----------------------------------------------------

html_redirect_pages = redirects.NRF_BM
