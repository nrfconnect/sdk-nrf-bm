#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# nrf documentation build configuration file

import os
from pathlib import Path
import sys
import re


# Paths ------------------------------------------------------------------------

NRF_LITE_DIR = Path(__file__).absolute().parents[2]

sys.path.insert(0, str(NRF_LITE_DIR / "doc" / "_utils"))
import redirects
import utils

ZEPHYR_BASE = utils.get_projdir("zephyr")

# General configuration --------------------------------------------------------

project = "nRF Connect SDK Lite"
copyright = "2019-2025, Nordic Semiconductor"
author = "Nordic Semiconductor"
version = release = os.environ.get("DOCSET_VERSION")

sys.path.insert(0, str(ZEPHYR_BASE / "doc" / "_extensions"))
sys.path.insert(0, str(NRF_LITE_DIR / "doc" / "_extensions"))

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
    "ncs_tool_versions",
    "page_filter",
]

linkcheck_ignore = [
    # relative links (intersphinx, doxygen)
    r"\.\.(\\|/)",
    # used as example in doxygen
    "https://google.com:443",
]

linkcheck_anchors_ignore = [r"page="]

rst_epilog = """
.. include:: /links.txt
.. include:: /shortcuts.txt
"""

# Options for HTML output ------------------------------------------------------

html_theme = "sphinx_ncs_theme"
html_static_path = [str(NRF_LITE_DIR / "doc" / "_static")]
html_last_updated_fmt = "%b %d, %Y"
html_show_sourcelink = True
html_show_sphinx = False

html_theme_options = {"docset": "ncs-lite", "docsets": utils.ALL_DOCSETS}

# Options for intersphinx ------------------------------------------------------

#intersphinx_mapping = dict()

#kconfig_mapping = utils.get_intersphinx_mapping("kconfig")
#if kconfig_mapping:
#    intersphinx_mapping["kconfig"] = kconfig_mapping


# -- Options for doxyrunner plugin ---------------------------------------------

_doxyrunner_outdir = utils.get_builddir() / "html" / "ncs-lite" / "doxygen"

doxyrunner_doxygen = os.environ.get("DOXYGEN_EXECUTABLE", "doxygen")
doxyrunner_projects = {
    "ncs-lite": {
        "doxyfile": NRF_LITE_DIR / "doc" / "ncs-lite" / "ncs-lite.doxyfile.in",
        "outdir": _doxyrunner_outdir,
        "fmt": True,
        "fmt_vars": {
            "NRF_LITE_DIR": str(NRF_LITE_DIR),
            "DOCSET_SOURCE_BASE": str(NRF_LITE_DIR),
            "DOCSET_BUILD_DIR": str(_doxyrunner_outdir),
            "DOCSET_VERSION": version,
        }
    }
}

# -- Options for doxybridge plugin ---------------------------------------------

doxybridge_projects = {
    "ncs-lite": _doxyrunner_outdir,
    "softdevice": utils.get_builddir() / "html" / "softdevice",
}

# Options for ncs_include ------------------------------------------------------

ncs_include_mapping = {
    "ncs-lite": utils.get_srcdir("ncs-lite"),
}

# Options for html_redirect ----------------------------------------------------

html_redirect_pages = redirects.NRF

# Options for zephyr.link-roles ------------------------------------------------

#link_roles_manifest_project = "nrf"
#link_roles_manifest_baseurl = "https://github.com/nrfconnect/sdk-nrf"

# Options for external_content -------------------------------------------------

external_content_contents = [
    (NRF_LITE_DIR / "doc" / "nrf", "*"),
    (NRF_LITE_DIR, "applications/**/*.rst"),
    (NRF_LITE_DIR, "applications/**/doc"),
    (NRF_LITE_DIR, "samples/**/*.rst"),
    (NRF_LITE_DIR, "scripts/**/*.rst"),
    (NRF_LITE_DIR, "tests/**/*.rst"),
]
external_content_keep = ["versions.txt"]

# Options for table_from_rows --------------------------------------------------

table_from_rows_base_dir = NRF_LITE_DIR
table_from_sample_yaml_board_reference = "/includes/sample_board_rows.txt"

# Options for ncs_tool_versions ------------------------------------------------

ncs_tool_versions_host_deps = [
    NRF_LITE_DIR / "scripts" / "tools-versions-win10.yml",
    NRF_LITE_DIR / "scripts" / "tools-versions-linux.yml",
    NRF_LITE_DIR / "scripts" / "tools-versions-darwin.yml",
]
ncs_tool_versions_python_deps = [
#    ZEPHYR_BASE / "scripts" / "requirements-base.txt",
    NRF_LITE_DIR / "doc" / "requirements.txt",
    NRF_LITE_DIR / "scripts" / "requirements-build.txt",
]

# Options for options_from_kconfig ---------------------------------------------

options_from_kconfig_base_dir = NRF_LITE_DIR
options_from_kconfig_zephyr_dir = ZEPHYR_BASE

# Options for manifest_revisions_table -----------------------------------------

# manifest_revisions_table_manifest = NRF_LITE_DIR / "west.yml"

# Options for sphinx_notfound_page ---------------------------------------------

#notfound_urls_prefix = "/nRF_Connect_SDK/doc/{}/nrf/".format(
#    "latest" if version.endswith("99") else version
#)

# -- Options for zephyr.gh_utils -----------------------------------------------

#gh_link_version = "main" if version.endswith("99") else f"v{version}"
#gh_link_base_url = f"https://github.com/nrfconnect/sdk-nrf"
#gh_link_prefixes = {
#    "applications/.*": "",
#    "samples/.*": "",
#    "scripts/.*": "",
#    "tests/.*": "",
#    ".*": "doc/nrf",
#}


def setup(app):
    app.add_css_file("css/nrf.css")

    utils.add_google_analytics(app, html_theme_options)
    utils.add_announcement_banner(html_theme_options)
