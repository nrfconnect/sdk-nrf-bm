#!/usr/bin/env python3
#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import os
import struct
from intelhex import IntelHex
import zlib
import sys
import argparse
import pathlib
import pickle
from dataclasses import dataclass

@dataclass
class PartitionInfo:
    partition_name: str
    start_address: int = 0
    size: int = 0
    found: bool = False

class CMakeCache(dict):
    @classmethod
    def from_file(cls, filename):
        """
        Extracts content of 'CMakeCache.txt' file into a dictionary

        Parameters:
            config_path (str): Path to 'CMakeCache.txt' file used by a firmware
        Returns:
            cls: A CMakeCache object where keys are CMake option names and values
            are option values, parsed from the config file at config_path.
        """
        configs = cls()
        try:
            with open(filename, 'r') as config:
                for line in config:
                    if (line.startswith("#") or line.startswith("//") or len(line) < 4):
                        continue
                    key_combined, value = line.split("=", 1)
                    key_name, _key_type = key_combined.split(":", 1)
                    configs[key_name] = value.strip()
                return configs
        except Exception as err:
            raise Exception("Unable to parse CMakeCache.txt file") from err

class KConfig(dict):
    @classmethod
    def from_file(cls, filename):
        """
        Extracts content of '.config' file into a dictionary

        Parameters:
            config_path (str): Path to '.config' file used by a firmware
        Returns:
            cls: A KConfig object where keys are Kconfig option names and values
            are option values, parsed from the config file at config_path.
        """
        configs = cls()
        try:
            with open(filename, 'r') as config:
                for line in config:
                    if not (line.startswith("CONFIG_") or line.startswith("SB_CONFIG_")):
                        continue
                    kconfig, value = line.split("=", 1)
                    configs[kconfig] = value.strip()
                return configs
        except Exception as err:
            raise Exception("Unable to parse .config file") from err

def load_zephyr_python_devicetree(zephyr_path):
    sys.path.insert(0, os.path.join(zephyr_path, 'scripts', 'dts', 'python-devicetree', 'src'))

def parse_args():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False)
    parser.add_argument('--build-dir', required=True, type=pathlib.Path,
                        help='Directory of build.')

    main_args = parser.parse_known_args()

    return main_args

def parse_partition_info(edt, partitions):
    for node in edt.nodes:
        if len(node.labels) != 1:
            continue

        for partition in partitions:
            if partition.partition_name in node.labels:
                partition.start_address = node.props['reg'].val[0]
                partition.size = node.props['reg'].val[1]
                partition.found = True

def get_partition_info(partitions, partition_name):
    for partition in partitions:
        if partition.partition_name == partition_name:
            return partition

    return False

def main():
    partitions = [PartitionInfo('boot_partition'), PartitionInfo('slot0_partition'), PartitionInfo('slot1_partition'), PartitionInfo('softdevice_partition'), PartitionInfo('metadata_partition')]
    args = parse_args()[0]
    image_id_softdevice = 0x01
    image_id_firmware_loader = 0x02

    if args.build_dir.is_dir() is False:
        print(f"Specified build directory does not exist: {str(args.build_dir)}")
        sys.exit(1)

    metadata_output_file = args.build_dir / 'metadata.hex'
    combined_update_output_file = args.build_dir / 'combined_signed_update_images.hex'

    # Load sysbuild Kconfig and CMake cache files
    sysbuild_cmake_cache_file = args.build_dir / 'CMakeCache.txt'

    if sysbuild_cmake_cache_file.exists() is False:
        print(f"Sysbuild CMake cache file does not exist: {str(sysbuild_cmake_cache_file)}")
        sys.exit(1)

    sysbuild_kconfig_file = args.build_dir / 'zephyr/.config'

    if sysbuild_kconfig_file.exists() is False:
        print(f"Sysbuild Kconfig file does not exist: {str(sysbuild_kconfig_file)}")
        sys.exit(1)

    sysbuild_cmake_cache = CMakeCache.from_file(sysbuild_cmake_cache_file)
    sysbuild_kconfig_defines = KConfig.from_file(sysbuild_kconfig_file)

    if 'SB_CONFIG_SOFTDEVICE_NONE' in sysbuild_kconfig_defines:
        sys.exit(0)

    # Load application Kconfig Kconfig files
    application_name = pathlib.Path(sysbuild_cmake_cache['APP_DIR']).parts[-1]
    application_build_dir = args.build_dir / application_name / 'zephyr'
    application_kconfig_file = application_build_dir / '.config'

    if application_build_dir.is_dir() is False:
        print(f"Application loader directory does not exist: {str(application_build_dir)}")
        sys.exit(1)

    if application_kconfig_file.exists() is False:
        print(f"Application loader Kconfig file does not exist: {str(application_kconfig_file)}")
        sys.exit(1)

    # Load application devicetree data
    load_zephyr_python_devicetree(sysbuild_cmake_cache['ZEPHYR_BASE'])
    application_pickle_file = application_build_dir / 'edt.pickle'

    with open(application_pickle_file, 'rb') as f:
        edt = pickle.load(f)

    parse_partition_info(edt, partitions)

    softdevice_partition_info = get_partition_info(partitions, 'softdevice_partition')
    softdevice_start_address = softdevice_partition_info.start_address
    softdevice_size = softdevice_partition_info.size

    # Begin metadata construction - header
    metadata_data = struct.pack('<BBBB', 0x92, 0x11, 0xf2, 0xe9)

    # Add signed softdevice file
    ih = IntelHex()
    _softdevice_signed_update_hex = ih.loadhex(args.build_dir / 'softdevice.signed.hex')
    softdevice_signed_update_size = ih.maxaddr() - ih.minaddr() + 1
    softdevice_signed_update_data = ih.tobinarray()

    # Add softdevice data - metadata, installer metadata, checksum, image ID, padding
    metadata_data += struct.pack('<II', softdevice_start_address, softdevice_size)
    metadata_data += struct.pack('<II', 0, softdevice_signed_update_size)
    metadata_data += struct.pack('<I', zlib.crc32(softdevice_signed_update_data))
    metadata_data += struct.pack('<BBBB', image_id_softdevice, 0xff, 0xff, 0xff)

    if 'SB_CONFIG_BM_FIRMWARE_LOADER_NONE' not in sysbuild_kconfig_defines:
        firmware_loader_build_dir = args.build_dir / sysbuild_kconfig_defines['SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME'][1:-1] / 'zephyr'
        firmware_loader_kconfig_file = firmware_loader_build_dir / '.config'

        if firmware_loader_build_dir.is_dir() is False:
            print(f"Firmware loader directory does not exist: {str(firmware_loader_build_dir)}")
            sys.exit(1)

        if firmware_loader_kconfig_file.exists() is False:
            print(f"Firmware loader Kconfig file does not exist: {str(firmware_loader_kconfig_file)}")
            sys.exit(1)

        firmware_loader_kconfig_defines = KConfig.from_file(firmware_loader_kconfig_file)
        firmware_loader_signed_hex_file = firmware_loader_build_dir / str(firmware_loader_kconfig_defines['CONFIG_KERNEL_BIN_NAME'][1:-1] + '.signed.hex')

        firmware_loader_partition_info = get_partition_info(partitions, 'slot1_partition')
        firmware_loader_start_address = firmware_loader_partition_info.start_address
        firmware_loader_size = firmware_loader_partition_info.size

        # Add signed firmware loader file
        ih = IntelHex()
        _firmware_loader_signed_update_hex = ih.loadhex(firmware_loader_signed_hex_file)
        firmware_loader_signed_update_size = ih.maxaddr() - ih.minaddr() + 1
        firmware_loader_signed_update_data = ih.tobinarray()

        # Add firmware loader data - metadata, installer metadata, checksum, image ID, padding
        metadata_data += struct.pack('<II', firmware_loader_start_address, firmware_loader_size)
        metadata_data += struct.pack('<II', softdevice_signed_update_size, firmware_loader_signed_update_size)
        metadata_data += struct.pack('<I', zlib.crc32(firmware_loader_signed_update_data))
        metadata_data += struct.pack('<BBBB', image_id_firmware_loader, 0xff, 0xff, 0xff)

    # Checksum of structure
    metadata_data += struct.pack('<I', zlib.crc32(metadata_data))

    # Load installer Kconfig Kconfig files
    installer_build_dir = args.build_dir / 'installer' / 'zephyr'
    installer_kconfig_file = installer_build_dir / '.config'

    if installer_build_dir.is_dir() is False:
        print(f"Installer directory does not exist: {str(installer_build_dir)}")
        sys.exit(1)

    if installer_kconfig_file.exists() is False:
        print(f"Installer Kconfig file does not exist: {str(installer_kconfig_file)}")
        sys.exit(1)

    installer_kconfig_defines = KConfig.from_file(installer_kconfig_file)
    installer_hex_file = installer_build_dir / str(installer_kconfig_defines['CONFIG_KERNEL_BIN_NAME'][1:-1] + '.hex')

    # Write metadata output data hex file
    ih = IntelHex()
    _installer_hex = ih.loadhex(installer_hex_file)
    metadata_address = ih.maxaddr() + 1

    oh = IntelHex()
    oh.frombytes(metadata_data, offset=metadata_address)
    struct_size = oh.maxaddr() - oh.minaddr() + 1
    oh.write_hex_file(metadata_output_file)

    # Write combined output data hex file
    update_data_pos = metadata_address + struct_size
    oh = IntelHex()
    oh.frombytes(softdevice_signed_update_data + firmware_loader_signed_update_data, offset=update_data_pos)
    oh.write_hex_file(combined_update_output_file)

if __name__ == '__main__':
    main()
