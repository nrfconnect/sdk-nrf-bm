#!/usr/bin/env python3
# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

"""Parse bm_* board DTS files and generate SVG memory layout diagrams.

Usage:
    python gen_memory_layouts.py [--board-dir DIR] [--output-dir DIR]

The script scans for ``bm_*`` board directories under ``boards/nordic/``,
parses every ``.dts`` file that defines flash/SRAM partitions, and writes
one SVG per configuration into the output directory.

Re-run this script whenever devicetree sources change.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass, field
from pathlib import Path

NRF_BM_BASE = Path(__file__).resolve().parents[2]
BOARDS_DIR = NRF_BM_BASE / "boards" / "nordic"
OUTPUT_DIR = NRF_BM_BASE / "doc" / "nrf-bm" / "boards" / "images"

# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------


@dataclass
class Partition:
    name: str
    offset: int
    size: int
    children: list[Partition] = field(default_factory=list)


@dataclass
class MemoryMap:
    label: str
    total: int
    base_addr: int
    partitions: list[Partition] = field(default_factory=list)


@dataclass
class BoardConfig:
    soc: str
    softdevice: str
    mcuboot: bool
    rram: MemoryMap
    sram: MemoryMap

    @property
    def short_label(self) -> str:
        sd = self.softdevice.upper()
        return f"{sd} + MCUboot" if self.mcuboot else sd


# ---------------------------------------------------------------------------
# DTS parsing helpers
# ---------------------------------------------------------------------------

_SIZE_K = re.compile(r"DT_SIZE_K\((\d+)\)")


def _eval_size_expr(raw: str) -> int:
    """Evaluate a DTS size expression that may contain ``DT_SIZE_K()``."""
    expr = _SIZE_K.sub(r"(\1 * 1024)", raw)
    expr = expr.replace(" ", "")
    return int(eval(expr))  # noqa: S307 – trusted input from our own DTS


_SUBPART_NAMES = {"peer_manager", "storage0"}


def _parse_partitions(
    text: str,
    base_offset: int = 0,
    *,
    include_subparts: bool = True,
) -> list[Partition]:
    """Extract partitions from a DTS node block.

    When *include_subparts* is False, partitions whose label matches a known
    sub-partition name (peer_manager, storage0) are skipped so that callers
    can handle them separately.
    """
    parts: list[Partition] = []
    pattern = re.compile(
        r"(\w+):\s*(?:partition|sram)@([0-9a-fA-F]+)\s*\{[^}]*?"
        r'label\s*=\s*"([^"]+)".*?'
        r"reg\s*=\s*<\s*(0x[0-9a-fA-F]+)\s+(.*?)\s*>",
        re.DOTALL,
    )
    for m in pattern.finditer(text):
        name = m.group(3)
        if not include_subparts and name in _SUBPART_NAMES:
            continue
        offset = int(m.group(4), 16)
        size = _eval_size_expr(m.group(5))
        parts.append(Partition(name=name, offset=base_offset + offset, size=size))
    return parts


def _parse_retained_mem(text: str) -> Partition | None:
    """Find a RetainedMem region.

    The ``reg`` and ``zephyr,memory-region`` properties sit at the top level
    of the ``sram@ADDR`` node, before any nested child blocks.  We extract
    only that top-level text (up to the first nested ``{``) to avoid matching
    child node properties.
    """
    node_m = re.search(
        r"sram@([0-9a-fA-F]+)\s*\{([^{}]*)"
        r'zephyr,memory-region\s*=\s*"RetainedMem"',
        text,
        re.DOTALL,
    )
    if not node_m:
        return None
    top_level = node_m.group(2)
    reg_m = re.search(r"reg\s*=\s*<\s*(0x[0-9a-fA-F]+)\s+(.*?)\s*>", top_level)
    if not reg_m:
        return None
    return Partition(
        name="RetainedMem",
        offset=int(reg_m.group(1), 16),
        size=_eval_size_expr(reg_m.group(2)),
    )


# ---------------------------------------------------------------------------
# DTS → BoardConfig
# ---------------------------------------------------------------------------

_SOC_RRAM_TOTAL = {
    "nrf54l15": 1524 * 1024,
    "nrf54l10": 1012 * 1024,
    "nrf54l05": 500 * 1024,
    "nrf54lv10a": 1012 * 1024,
}

_SOC_SRAM_TOTAL = {
    "nrf54l15": 256 * 1024,
    "nrf54l10": 192 * 1024,
    "nrf54l05": 96 * 1024,
    "nrf54lv10a": 191 * 1024,
}


def _detect_soc(stem: str) -> str:
    # Try nrf54lv10a before nrf54l10 so the longer tag matches first
    for soc in ("nrf54l15", "nrf54lv10a", "nrf54l10", "nrf54l05"):
        tag = f"_{soc}_"
        if tag in stem:
            return soc
    raise ValueError(f"Cannot detect SoC from {stem}")


def _detect_softdevice(stem: str) -> str:
    if "_s145_" in stem:
        return "s145"
    if "_s115_" in stem:
        return "s115"
    raise ValueError(f"Cannot detect SoftDevice from {stem}")


def _detect_mcuboot(stem: str) -> bool:
    return "mcuboot" in stem


def _find_last_node_block(pattern: str, text: str) -> str | None:
    """Find the last occurrence of a DTS node override and return its body.

    The common .dtsi may contain a short override like ``&cpuapp_rram { reg = ...; };``
    while the .dts has the full partition definitions. We want the last (most complete) one.
    """
    matches = list(re.finditer(pattern, text, re.DOTALL | re.MULTILINE))
    return matches[-1].group(1) if matches else None


def parse_dts(dts_path: Path, common_text: str = "") -> BoardConfig:
    """Parse a single DTS file into a BoardConfig."""
    text = dts_path.read_text()
    full = common_text + "\n" + text
    stem = dts_path.stem

    soc = _detect_soc(stem)
    sd = _detect_softdevice(stem)
    mcuboot = _detect_mcuboot(stem)

    # ---- RRAM partitions ----
    rram_body = _find_last_node_block(r"&cpuapp_rram\s*\{(.*?)^\};", full)
    rram_parts = _parse_partitions(rram_body, include_subparts=False) if rram_body else []

    # Replace the storage parent with its expanded children (peer_mgr + storage0)
    if rram_body:
        storage_idx = next(
            (i for i, p in enumerate(rram_parts) if p.name == "storage"),
            None,
        )
        if storage_idx is not None:
            storage_parent = rram_parts[storage_idx]
            children = _parse_partitions(
                rram_body,
                base_offset=storage_parent.offset,
                include_subparts=True,
            )
            children = [c for c in children if c.name in _SUBPART_NAMES]
            if children:
                rram_parts[storage_idx : storage_idx + 1] = children

    rram_parts.sort(key=lambda p: p.offset)

    # Fill gaps between partitions
    rram_total = _SOC_RRAM_TOTAL[soc]
    filled: list[Partition] = []
    cursor = 0
    for p in rram_parts:
        if p.offset > cursor:
            filled.append(Partition(name="(unused)", offset=cursor, size=p.offset - cursor))
        filled.append(p)
        cursor = p.offset + p.size
    if cursor < rram_total:
        filled.append(Partition(name="(unused)", offset=cursor, size=rram_total - cursor))
    rram_parts = filled

    rram = MemoryMap(label="RRAM", total=rram_total, base_addr=0x0, partitions=rram_parts)

    # ---- SRAM partitions ----
    sram_total = _SOC_SRAM_TOTAL[soc]
    sram_base = 0x20000000
    kmu_size = 0x80

    sram_body = _find_last_node_block(r"&cpuapp_sram\s*\{(.*?)^\};", full)
    sram_parts_raw = _parse_partitions(sram_body) if sram_body else []
    for p in sram_parts_raw:
        p.offset += kmu_size

    sram_parts: list[Partition] = [Partition(name="KMU reserved", offset=0, size=kmu_size)]
    sram_parts.extend(sram_parts_raw)

    retained = _parse_retained_mem(full)
    if retained:
        retained.offset = retained.offset - sram_base
        sram_parts.append(retained)

    sram_parts.sort(key=lambda p: p.offset)

    sram = MemoryMap(label="SRAM", total=sram_total, base_addr=sram_base, partitions=sram_parts)

    return BoardConfig(soc=soc, softdevice=sd, mcuboot=mcuboot, rram=rram, sram=sram)


# ---------------------------------------------------------------------------
# SVG rendering
# ---------------------------------------------------------------------------

# Nordic Semiconductor brand-aligned palette
COLORS = {
    "boot": "#0032a0",  # Nordic Blueslate
    "peer_manager": "#de823b",  # Nordic Fall
    "storage0": "#f4cb43",  # Nordic Sun
    "slot0": "#6ad1e3",  # Nordic Sky
    "slot1": "#00a2c6",  # Nordic Blue
    "softdevice": "#0077c8",  # Nordic Lake
    "metadata": "#0032a0",  # Nordic Blueslate
    "(unused)": "#d9e1e2",  # Nordic Light Grey
    "KMU reserved": "#768692",  # Nordic Middle Grey
    "softdevice_static_ram": "#0077c8",  # Nordic Lake
    "softdevice_dynamic_ram": "#00a2c6",  # Nordic Blue
    "app_ram": "#6ad1e3",  # Nordic Sky
    "RetainedMem": "#0032a0",  # Nordic Blueslate
}

# Display names shown in the diagram (readable); key is DTS label or synthetic name.
DISPLAY_NAMES = {
    "boot": "MCUboot",
    "peer_manager": "Storage: Peer Manager",
    "storage0": "Storage: User data",
    "slot0": "Application (slot0)",
    "slot1": "Firmware Loader (slot1)",
    "softdevice": "SoftDevice",
    "metadata": "Metadata",
    "(unused)": "Unused",
    "KMU reserved": "KMU reserved",
    "softdevice_static_ram": "SoftDevice Static RAM",
    "softdevice_dynamic_ram": "SoftDevice Dynamic RAM",
    "app_ram": "Application RAM",
    "RetainedMem": "Retained RAM",
}


def _fmt_size(nbytes: int) -> str:
    if nbytes >= 1024 and nbytes % 1024 == 0:
        return f"{nbytes // 1024} KB"
    if nbytes >= 1024:
        return f"{nbytes / 1024:.1f} KB"
    return f"{nbytes} B"


def _hex(val: int) -> str:
    return f"0x{val:08X}"


_MIN_BLOCK_H = 28
_BAR_WIDTH = 200
_BAR_HEIGHT = 420
_ADDR_COL_W = 90
_LABEL_COL_W = 150
_COL_GAP = 40
_TITLE_H = 30


def _compute_heights(partitions: list[Partition], total: int) -> list[float]:
    n = len(partitions)
    reserved = n * _MIN_BLOCK_H
    remaining = max(0.0, _BAR_HEIGHT - reserved)
    total_size = sum(p.size for p in partitions)
    heights = []
    for p in partitions:
        ratio = p.size / total_size if total_size else 1.0 / n
        heights.append(max(_MIN_BLOCK_H, _MIN_BLOCK_H + remaining * ratio))
    scale = _BAR_HEIGHT / sum(heights) if sum(heights) > 0 else 1
    return [h * scale for h in heights]


def _render_mem_column(mm: MemoryMap, x_offset: float) -> tuple[str, float]:
    """Render one memory column (low addresses at bottom), return (svg_fragment, actual_height)."""
    parts = mm.partitions
    heights = _compute_heights(parts, mm.total)

    fragments: list[str] = []
    bx = x_offset + _ADDR_COL_W
    bar_bottom = _TITLE_H + _BAR_HEIGHT

    _TEXT_ON_BLOCK = "#fff"
    _TEXT_ADDR = "#6b7280"
    _TEXT_SIZE = "#6b7280"
    _TEXT_TITLE = "#333f48"
    _STROKE = "#e5e7eb"

    # Title
    fragments.append(
        f'<text x="{bx + _BAR_WIDTH / 2}" y="18" '
        f'text-anchor="middle" font-size="13" font-weight="bold" fill="{_TEXT_TITLE}">'
        f'{mm.label} ({_fmt_size(mm.total)})</text>'
    )

    # Draw blocks bottom-to-top: first partition (lowest address) at the bottom
    y = bar_bottom
    for part, h in zip(parts, heights, strict=True):
        y -= h
        color = COLORS.get(part.name, "#94a3b8")
        display = DISPLAY_NAMES.get(part.name, part.name)
        addr_start = mm.base_addr + part.offset
        is_light_block = part.name == "(unused)"
        label_fill = "#667085" if is_light_block else _TEXT_ON_BLOCK

        fragments.append(
            f'<rect x="{bx}" y="{y}" width="{_BAR_WIDTH}" height="{h}" '
            f'fill="{color}" rx="3" stroke="{_STROKE}" stroke-width="1"/>'
        )

        if h >= 20:
            fragments.append(
                f'<text x="{bx + _BAR_WIDTH / 2}" y="{y + h / 2 + 4}" '
                f'text-anchor="middle" font-size="10" font-weight="600" '
                f'fill="{label_fill}">{display}</text>'
            )

        if h >= 18:
            fragments.append(
                f'<text x="{bx - 4}" y="{y + h - 3}" '
                f'text-anchor="end" font-size="10" font-weight="bold" font-family="monospace" '
                f'fill="{_TEXT_ADDR}">{_hex(addr_start)}</text>'
            )

            fragments.append(
                f'<text x="{bx + _BAR_WIDTH + 5}" y="{y + h / 2 + 3}" '
                f'text-anchor="start" font-size="10" font-weight="bold" fill="{_TEXT_SIZE}">'
                f"{_fmt_size(part.size)}</text>"
            )

    # Top-end address (highest address, shown above the topmost block)
    last = parts[-1] if parts else None
    if last:
        end_addr = mm.base_addr + last.offset + last.size
        fragments.append(
            f'<text x="{bx - 4}" y="{y - 3}" '
            f'text-anchor="end" font-size="10" font-weight="bold" font-family="monospace" '
            f'fill="{_TEXT_ADDR}">{_hex(end_addr)}</text>'
        )

    return "\n".join(fragments), bar_bottom + 16


def render_svg(cfg: BoardConfig) -> str:
    """Render a complete SVG for one board configuration."""
    col_w = _ADDR_COL_W + _BAR_WIDTH + _LABEL_COL_W

    rram_svg, rram_h = _render_mem_column(cfg.rram, 0)
    sram_svg, sram_h = _render_mem_column(cfg.sram, col_w + _COL_GAP)

    total_w = 2 * col_w + _COL_GAP
    total_h = max(rram_h, sram_h) + 10

    total_h = round(total_h)
    return (
        f'<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {total_w} {total_h}"\n'
        f'     width="{total_w}" height="{total_h}"\n'
        f'     font-family="Inter, -apple-system, sans-serif">\n'
        f'  <rect width="100%" height="100%" fill="#ffffff" rx="8"/>\n'
        f"  {rram_svg}\n"
        f"  {sram_svg}\n"
        f"</svg>\n"
    )


# ---------------------------------------------------------------------------
# Board discovery & main
# ---------------------------------------------------------------------------


def discover_boards(boards_dir: Path) -> dict[str, Path]:
    """Return mapping of board_name → board_dir for all bm_* boards."""
    result = {}
    for d in sorted(boards_dir.iterdir()):
        if d.is_dir() and d.name.startswith("bm_"):
            result[d.name] = d
    return result


def find_common_dtsi(board_dir: Path, soc: str) -> str:
    """Read the common .dtsi for a given SoC variant."""
    pattern = f"*_{soc}_cpuapp_common.dtsi"
    matches = list(board_dir.glob(pattern))
    return matches[0].read_text() if matches else ""


def process_board(board_dir: Path, output_dir: Path) -> list[BoardConfig]:
    """Process all DTS files for a board and generate SVGs."""
    dts_files = sorted(board_dir.glob("*.dts"))
    configs: list[BoardConfig] = []

    for dts in dts_files:
        try:
            soc = _detect_soc(dts.stem)
        except ValueError:
            continue
        common = find_common_dtsi(board_dir, soc)
        try:
            cfg = parse_dts(dts, common)
        except Exception as exc:
            print(f"  WARN: skipping {dts.name}: {exc}")
            continue

        svg = render_svg(cfg)
        fname = dts.stem + ".svg"
        (output_dir / fname).write_text(svg)
        print(f"  -> {fname}")
        configs.append(cfg)

    return configs


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument(
        "--board-dir",
        type=Path,
        default=BOARDS_DIR,
        help="Parent directory containing bm_* board folders",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=OUTPUT_DIR,
        help="Directory for generated SVG files",
    )
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)

    boards = discover_boards(args.board_dir)
    if not boards:
        print(f"No bm_* boards found in {args.board_dir}")
        return

    for name, board_dir in boards.items():
        print(f"Board: {name}")
        process_board(board_dir, args.output_dir)

    print(f"\nDone. SVGs written to {args.output_dir}")


if __name__ == "__main__":
    main()
