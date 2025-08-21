# Copyright (c) 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

function(bm_install_tasks output_hex output_bin)
  set(dependencies ${PROJECT_BINARY_DIR}/.config)

  find_program(IMGTOOL imgtool.py HINTS ${ZEPHYR_MCUBOOT_MODULE_DIR}/scripts/ NAMES imgtool NAMES_PER_DIR)
  set(keyfile "${SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE}")
  string(CONFIGURE "${keyfile}" keyfile)

  if(NOT "${SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_NONE}")
    # Check for misconfiguration.
    if("${keyfile}" STREQUAL "")
      # No signature key file, no signed binaries. No error, though:
      # this is the documented behavior.
      message(WARNING "Neither SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_NONE or "
                      "SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE are set, the generated build will not be "
                      "bootable by MCUboot unless it is signed manually/externally.")
      return()
    endif()
  endif()

  # No imgtool, no signed binaries.
  if(NOT DEFINED IMGTOOL)
    message(FATAL_ERROR "Can't sign images for MCUboot: can't find imgtool. To fix, install imgtool with pip3, or add the mcuboot repository to the west manifest and ensure it has a scripts/imgtool.py file.")
    return()
  endif()

  # Softdevice and firmware loader
  list(APPEND dependencies installer_extra_byproducts mcuboot_extra_byproducts)

  sysbuild_get(installer_binary_dir IMAGE installer VAR APPLICATION_BINARY_DIR CACHE)
  sysbuild_get(installer_kernel_name IMAGE installer VAR CONFIG_KERNEL_BIN_NAME KCONFIG)
  set(input ${installer_binary_dir}/zephyr/${installer_kernel_name}.hex)
  list(APPEND input ${PROJECT_BINARY_DIR}/../metadata.hex)
  list(APPEND input ${PROJECT_BINARY_DIR}/../combined_signed_update_images.hex)
  set(output_file ${PROJECT_BINARY_DIR}/../installer_merged.hex)

  # Merge files
  add_custom_target(combined_images_target
    DEPENDS ${output_file}
  )

  add_custom_command(OUTPUT ${output_file}
    COMMAND
    ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/scripts/build/mergehex.py -o ${output_file} ${input}
    DEPENDS ${input} ${dependencies}
  )

  # Fetch devicetree details for flash and slot information
  dt_chosen(flash_node TARGET ${DEFAULT_IMAGE} PROPERTY "zephyr,flash")
  dt_prop(write_block_size TARGET ${DEFAULT_IMAGE} PATH "${flash_node}" PROPERTY "write-block-size")

  if(NOT write_block_size)
    set(write_block_size 4)
    message(WARNING "slot0_partition write block size devicetree parameter is missing, assuming write block size is 4")
  endif()

  sysbuild_get(CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION IMAGE ${DEFAULT_IMAGE} VAR CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION KCONFIG)
  sysbuild_get(CONFIG_ROM_START_OFFSET IMAGE ${DEFAULT_IMAGE} VAR CONFIG_ROM_START_OFFSET KCONFIG)
  sysbuild_get(CONFIG_FLASH_LOAD_SIZE IMAGE ${DEFAULT_IMAGE} VAR CONFIG_FLASH_LOAD_SIZE KCONFIG)
#  sysbuild_get(CONFIG_MCUBOOT_HARDWARE_DOWNGRADE_PREVENTION IMAGE ${DEFAULT_IMAGE} VAR CONFIG_MCUBOOT_HARDWARE_DOWNGRADE_PREVENTION KCONFIG)

  # Custom TLV 0x0a is set to 0x01 to indicate that this is an installer image
  set(imgtool_sign ${PYTHON_EXECUTABLE} ${IMGTOOL} sign --version ${CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION} --align ${write_block_size} --slot-size ${CONFIG_FLASH_LOAD_SIZE} --header-size ${CONFIG_ROM_START_OFFSET} --overwrite-only --custom-tlv 0xa0 0x01)

#  if(CONFIG_MCUBOOT_HARDWARE_DOWNGRADE_PREVENTION)
#    set(imgtool_args --security-counter ${CONFIG_MCUBOOT_HW_DOWNGRADE_PREVENTION_COUNTER_VALUE})
#  endif()

  # Set proper hash calculation algorithm for signing
  if(SB_CONFIG_BM_BOOT_IMG_HASH_ALG_PURE)
    set(imgtool_args --pure ${imgtool_args})
  elseif(SB_CONFIG_BM_BOOT_IMG_HASH_ALG_SHA512)
    set(imgtool_args --sha 512 ${imgtool_args})
  endif()

  if(NOT "${keyfile}" STREQUAL "")
    set(imgtool_args -k "${keyfile}" ${imgtool_args})
  endif()

  # Set up outputs
  if(SB_CONFIG_INSTALLER_HEX_OUTPUT)
    add_custom_target(signed_installer_target
      ALL
      DEPENDS ${output_hex} ${output_bin}
    )

    add_custom_command(OUTPUT ${output_hex}
      COMMAND
      ${imgtool_sign} ${imgtool_args} ${output_file} ${output_hex}
      DEPENDS ${output_file} ${dependencies}
    )
  else()
    add_custom_target(signed_installer_target
      ALL
      DEPENDS ${output_bin}
    )
  endif()

  add_custom_command(OUTPUT ${output_bin}
    COMMAND
    ${imgtool_sign} ${imgtool_args} ${output_file} ${output_bin}
    DEPENDS ${output_file} ${dependencies}
  )
endfunction()

bm_install_tasks(${CMAKE_BINARY_DIR}/installer_softdevice_firmware_loader.hex ${CMAKE_BINARY_DIR}/installer_softdevice_firmware_loader.bin)
