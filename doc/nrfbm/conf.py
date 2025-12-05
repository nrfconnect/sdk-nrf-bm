# Copyright (c) 2025 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import os
from pathlib import Path
import sys


# Paths ------------------------------------------------------------------------

NRFBM_BASE = Path(__file__).absolute().parents[2]

sys.path.insert(0, str(NRFBM_BASE / "doc" / "_utils"))
import redirects
import utils

ZEPHYR_BASE = NRFBM_BASE / ".." / "zephyr"

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
html_static_path = [str(NRFBM_BASE / "doc" / "_static")]
html_last_updated_fmt = "%b %d, %Y"
html_show_sourcelink = True
html_show_sphinx = False

html_theme_options = {"docset": "nrfbm", "docsets": utils.ALL_DOCSETS}

# Options for intersphinx ------------------------------------------------------

intersphinx_mapping = dict()

kconfig_mapping = utils.get_intersphinx_mapping("kconfig")
if kconfig_mapping:
    intersphinx_mapping["kconfig"] = kconfig_mapping

# Options for external_content -------------------------------------------------

external_content_contents = [
    (NRFBM_BASE / "doc" / "nrfbm", "*"),
    (NRFBM_BASE, "samples/**/*.rst"),
    (NRFBM_BASE / "components" / "softdevice" / "s115" / "doc", "s115_9.0.0-4.*.main.rst"),
    (NRFBM_BASE / "components" / "softdevice" / "s145" / "doc", "s145_9.0.0-4.*.main.rst"),
]

external_content_keep = ["versions.txt"]
# -- Options for doxyrunner plugin ---------------------------------------------

_doxyrunner_outdir = utils.get_builddir() / "html" / "nrfbm" / "doxygen"

doxyrunner_doxygen = os.environ.get("DOXYGEN_EXECUTABLE", "doxygen")
doxyrunner_projects = {
    "nrf": {
        "doxyfile": NRFBM_BASE / "doc" / "nrfbm" / "nrfbm.doxyfile.in",
        "outdir": _doxyrunner_outdir,
        "fmt": True,
        "fmt_vars": {
            "NRFBM_BASE": str(NRFBM_BASE),
            "DOCSET_SOURCE_BASE": str(NRFBM_BASE),
            "DOCSET_BUILD_DIR": str(_doxyrunner_outdir),
            "DOCSET_VERSION": version,
        }
    }
}

# -- Options for doxybridge plugin ---------------------------------------------

doxybridge_projects = {
    "nrfbm": _doxyrunner_outdir,
    "s115": utils.get_builddir() / "html" / "s115",
}

# Options for html_redirect ----------------------------------------------------

html_redirect_pages = redirects.NRFBM
