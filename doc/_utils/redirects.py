"""
Copyright (c) 2025 Nordic Semiconductor
SPDX-License-Identifier: Apache-2.0

This module contains per-docset variables with a list of tuples
(old_url, newest_url) for pages that need a redirect. This list allows redirecting
old URLs (caused by, for example, reorganizing doc directories).

Notes:
    - Keep URLs relative to document root (NO leading slash and
      without the html extension).
    - Keep URLs in the order of pages from the doc hierarchy. Move entry order if hierarchy changes.
    - Comments mention the page name; edit the comment when the page name changes.
    - Each comment is valid for the line where it is added AND all lines without comment after it.
    - If a page was removed, mention in the comment when it was removed, if possible.
      A redirect should still point to another page.

Examples:
    ("old/README", "absolutely/newer/index"), # Name of the page
    ("new/index", "absolutely/newer/index"),
    ("even/newer/index", "absolutely/newer/index"),
"""

NRF_BM = ()
