# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
from pathlib import Path
import sys
import re

# Paths ------------------------------------------------------------------------

NRF_BASE = Path(__file__).absolute().parents[2]

sys.path.insert(0, str(NRF_BASE / "doc" / "_utils"))
import utils

ZEPHYR_BASE = utils.get_projdir("zephyr")

# General configuration --------------------------------------------------------

project = "nRF Connect SDK Bare Metal option"
copyright = "2019-2025, Nordic Semiconductor"
author = "Nordic Semiconductor"
version = release = os.environ.get("DOCSET_VERSION")

sys.path.insert(0, str(ZEPHYR_BASE / "doc" / "_extensions"))
sys.path.insert(0, str(NRF_BASE / "doc" / "_extensions"))

extensions = [
    "sphinx.ext.intersphinx",
    "table_from_rows",
    "options_from_kconfig",
    "ncs_include",
    "manifest_revisions_table",
    "sphinxcontrib.mscgen",
    "zephyr.html_redirects",
    "zephyr.kconfig",
    "zephyr.external_content",
    "zephyr.doxyrunner",
    "zephyr.doxybridge",
    "zephyr.link-roles",
    "zephyr.dtcompatible-role",
    "zephyr.domain",
    "zephyr.gh_utils",
    "sphinx_tabs.tabs",
    "software_maturity_table",
    "sphinx_togglebutton",
    "sphinx_copybutton",
    "notfound.extension",
    "page_filter",
    "sphinxcontrib.plantuml",
]

rst_epilog = """
.. include:: /substitutions.txt
.. include:: /links.txt
.. include:: /shortcuts.txt
"""

# Options for HTML output ------------------------------------------------------

html_theme = "sphinx_ncs_theme"
html_static_path = [str(NRF_BASE / "doc" / "_static")]
html_last_updated_fmt = "%b %d, %Y"
html_show_sourcelink = True
html_show_sphinx = False

html_theme_options = {"docset": "nrf-bm", "docsets": utils.ALL_DOCSETS}

# Options for intersphinx ------------------------------------------------------

intersphinx_mapping = dict()

kconfig_mapping = utils.get_intersphinx_mapping("kconfig")
if kconfig_mapping:
    intersphinx_mapping["kconfig"] = kconfig_mapping

# -- Options for doxyrunner plugin ---------------------------------------------

_doxyrunner_outdir = utils.get_builddir() / "html" / "nrf-bm" / "doxygen"

doxyrunner_doxygen = os.environ.get("DOXYGEN_EXECUTABLE", "doxygen")
doxyrunner_projects = {
    "nrf": {
        "doxyfile": NRF_BASE / "doc" / "nrf-bm" / "nrf-bm.doxyfile.in",
        "outdir": _doxyrunner_outdir,
        "fmt": True,
        "fmt_vars": {
            "NRF_BASE": str(NRF_BASE),
            "DOCSET_SOURCE_BASE": str(NRF_BASE),
            "DOCSET_BUILD_DIR": str(_doxyrunner_outdir),
            "DOCSET_VERSION": version,
        }
    }
}

doxybridge_projects = {
    "nrf-bm": _doxyrunner_outdir,
    "s115": utils.get_builddir() / "html" / "s115",
}

# Options for ncs_include ------------------------------------------------------

ncs_include_mapping = {
    "nrf-bm": utils.get_srcdir("nrf-bm"),
    "zephyr": utils.get_srcdir("zephyr"),
}

# Options for external_content -------------------------------------------------

external_content_contents = [
    (NRF_BASE / "doc" / "nrf-bm", "*"),
    (NRF_BASE, "samples/**/*.rst"),
]
external_content_keep = ["versions.txt"]

# Options for options_from_kconfig ---------------------------------------------

options_from_kconfig_base_dir = NRF_BASE
options_from_kconfig_zephyr_dir = ZEPHYR_BASE

# Options for sphinx_notfound_page ---------------------------------------------

notfound_urls_prefix = "/nRF_Connect_SDK/doc/{}/nrf/".format(
    "latest" if version.endswith("99") else version
)

# -- Options for zephyr.gh_utils -----------------------------------------------

gh_link_version = "main" if version.endswith("99") else f"v{version}"
gh_link_base_url = f"https://github.com/nrfconnect/sdk-nrf-bm"
gh_link_prefixes = {
    "samples/.*": "",
    ".*": "doc/nrf-bm",
}
