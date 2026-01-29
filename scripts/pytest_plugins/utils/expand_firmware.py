#!/usr/bin/env python3
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
"""
Script to expand firmware image for Nordic nrf54L15DK board
to test board behavior when attempting to flash an image that exceeds
the partition size via DFU.
"""

import argparse
import logging
import os
import random
import shutil
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path

from intelhex import IntelHex

ARM_EABI_OBJCOPY = shutil.which("arm-zephyr-eabi-objcopy") or os.path.join(
    os.environ["ZEPHYR_SDK_INSTALL_DIR"], "arm-zephyr-eabi/bin", "arm-zephyr-eabi-objcopy"
)

logger = logging.getLogger(__name__)


def convert_bin_to_hex(bin_file: str | Path, hex_file: str | Path, start_address: int) -> bool:
    """
    Convert binary file to Intel HEX format using arm-none-eabi-objcopy
    with specified start address.

    :param bin_file: path to binary file
    :param hex_file: path to output hex file
    :param start_address: start address in hex format (e.g., '0x148000')
    :returns: True if successful, False otherwise
    """
    cmd = [
        ARM_EABI_OBJCOPY,
        "-I",
        "binary",
        "-O",
        "ihex",
        "--change-address",
        hex(start_address),
        str(bin_file),
        str(hex_file),
    ]

    try:
        logger.info("Running Command: %s", " ".join(cmd))
        subprocess.run(cmd, check=True, capture_output=True, text=True)
        logger.info(f"Converted {bin_file} to {hex_file} with address {start_address}")
        return True
    except subprocess.CalledProcessError as e:
        logger.error(f"Error during conversion: {e.stderr}")
        return False
    except FileNotFoundError:
        logger.error(
            f"Tool '{ARM_EABI_OBJCOPY}' not found. "
            "Make sure it is installed and available in PATH.",
        )
        return False


def expand_hex_file(
    hex_file: str | Path,
    output_file: str | Path,
    partition_size: int,
    extra_size: int = 0x1000,
) -> bool:
    """
    Expand hex file with random data, exceeding the partition size.

    :params hex_file: path to input hex file
    :params output_file: path to output hex file
    :params partition_size: partition size in bytes
    :params extra_size: additional size to add (default: 4KB)
    :returns: True if successful, False otherwise
    """
    try:
        ih = IntelHex(hex_file)
    except Exception as e:
        logger.error(f"Error loading hex file: {e}")
        return False

    min_addr = ih.minaddr()
    max_addr = ih.maxaddr()
    if min_addr is None or max_addr is None:
        logger.error("Hex file is empty")
        return False

    base_address = min_addr
    max_address = max_addr + 1  # maxaddr() returns last address, not size
    current_size = max_address - base_address

    # Calculate how much data to add
    target_size = partition_size + extra_size
    bytes_to_add = max(0, target_size - current_size)

    print(f"  Current image size: {current_size} bytes")
    print(f"  Partition size: {partition_size} bytes")
    print(f"  Target size: {target_size} bytes")
    print(f"  Adding: {bytes_to_add} bytes of random data")

    random_data = bytes([random.randint(1, 254) for _ in range(bytes_to_add)])

    for i, byte_value in enumerate(random_data):
        ih[max_address + i] = byte_value

    try:
        if str(output_file)[-3:] == "hex":
            ih.write_hex_file(output_file)
        else:
            ih.tofile(output_file, "bin")
        logger.info(f"Expanded hex file: {output_file}")
        return True
    except Exception as e:
        logger.error(f"Error writing hex file: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Expand firmware image exceeding partition size",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""\
            Example usage:
                %(prog)s zephyr.bin --partition-address 0x148000 --partition-size 0x10000
        """),
        allow_abbrev=False,
    )
    parser.add_argument("bin_file", type=str, help="Path to signed binary file (.bin)")
    parser.add_argument(
        "--partition-address",
        type=str,
        help="Partition start address in hex format",
    )
    parser.add_argument(
        "--partition-size",
        type=str,
        help="Partition size in hex format",
    )
    parser.add_argument(
        "--extra-size",
        type=str,
        default="0x1000",
        help="Additional size to add in hex format (default: 0x1000 = 4KB)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        help="Path to output hex file (default: <bin_file>.expanded.hex)",
    )
    logging.basicConfig(level=logging.DEBUG)

    args = parser.parse_args()

    bin_path = Path(args.bin_file)
    if not bin_path.exists():
        print(f"✗ File does not exist: {bin_path}", file=sys.stderr)
        sys.exit(1)

    if args.output:
        output_hex = Path(args.output)
    else:
        output_hex = bin_path.with_suffix(".expanded.hex")

    try:
        partition_size = int(args.partition_size, 16)
        extra_size = int(args.extra_size, 16)
    except ValueError as e:
        print(f"✗ Invalid size format: {e}", file=sys.stderr)
        sys.exit(1)

    if not args.partition_address.startswith("0x"):
        print(
            "✗ Partition address must be in hex format (e.g., 0x148000)",
            file=sys.stderr,
        )
        sys.exit(1)

    partition_address = int(args.partition_address, 16)

    print(f"Processing file: {bin_path}")
    print(f"  Partition address: {partition_address}")
    print(f"  Partition size: {partition_size} ({partition_size} bytes)")
    print(f"  Extra size: {extra_size} ({extra_size} bytes)")
    print()

    with tempfile.NamedTemporaryFile(mode="w", suffix=".hex", delete=False) as tmp_hex:
        tmp_hex_path = tmp_hex.name

    try:
        if not convert_bin_to_hex(str(bin_path), tmp_hex_path, partition_address):
            sys.exit(1)

        if not expand_hex_file(tmp_hex_path, str(output_hex), partition_size, extra_size):
            sys.exit(1)

        print()
        print(f"✓ Done! Expanded hex file: {output_hex}")

    finally:
        if os.path.exists(tmp_hex_path):
            os.unlink(tmp_hex_path)


if __name__ == "__main__":
    main()
