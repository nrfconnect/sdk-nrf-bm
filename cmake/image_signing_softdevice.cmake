# Copyright (c) 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

function(softdevice_tasks output_hex output_bin)
  set(dependencies ${CMAKE_BINARY_DIR}/../zephyr/.config)

  find_program(IMGTOOL imgtool.py HINTS ${ZEPHYR_MCUBOOT_MODULE_DIR}/scripts/ NAMES imgtool NAMES_PER_DIR)
  set(keyfile "${CONFIG_BOOT_SIGNATURE_KEY_FILE}")
  string(CONFIGURE "${keyfile}" keyfile)

  if(NOT "${CONFIG_BOOT_SIGNATURE_TYPE_NONE}")
    # Check for misconfiguration.
    if("${keyfile}" STREQUAL "")
      # No signature key file, no signed binaries. No error, though:
      # this is the documented behavior.
      message(WARNING "Neither CONFIG_BOOT_SIGNATURE_TYPE_NONE or "
                      "CONFIG_BOOT_SIGNATURE_KEY_FILE are set, the generated build will not be "
                      "bootable by MCUboot unless it is signed manually/externally.")
      return()
    endif()
  endif()

  # No imgtool, no signed binaries.
  if(NOT DEFINED IMGTOOL)
    message(FATAL_ERROR "Can't sign images for MCUboot: can't find imgtool. To fix, install imgtool with pip3, or add the mcuboot repository to the west manifest and ensure it has a scripts/imgtool.py file.")
    return()
  endif()

  set(output_file ${CONFIG_SOFTDEVICE_FILE})

  if(CONFIG_SOFTDEVICE_PAD_HEADER)
    set(pad_header --pad-header)
  else()
    set(pad_header)
  endif()

  # Fetch devicetree details for flash and slot information
  dt_chosen(flash_node TARGET ${DEFAULT_IMAGE} PROPERTY "zephyr,flash")
  dt_prop(write_block_size TARGET ${DEFAULT_IMAGE} PATH "${flash_node}" PROPERTY "write-block-size")

  if(NOT write_block_size)
    set(write_block_size 4)
    message(WARNING "slot0_partition write block size devicetree parameter is missing, assuming write block size is 4")
  endif()

  dt_nodelabel(softdevice_partition_node_full_path NODELABEL "softdevice_partition")
  dt_reg_size(softdevice_partition_node_size PATH "${softdevice_partition_node_full_path}")

  set(imgtool_sign ${PYTHON_EXECUTABLE} ${IMGTOOL} sign --version ${CONFIG_SOFTDEVICE_VERSION} --align ${write_block_size} --slot-size ${softdevice_partition_node_size} --header-size ${CONFIG_SOFTDEVICE_START_OFFSET} ${pad_header})

#  if(CONFIG_MCUBOOT_HARDWARE_DOWNGRADE_PREVENTION)
#    set(imgtool_args --security-counter ${CONFIG_MCUBOOT_HW_DOWNGRADE_PREVENTION_COUNTER_VALUE})
#  endif()

  # Set proper hash calculation algorithm for signing
  if(CONFIG_BOOT_SIGNATURE_TYPE_PURE)
    set(imgtool_args --pure ${imgtool_args})
  elseif(CONFIG_BOOT_IMG_HASH_ALG_SHA512)
    set(imgtool_args --sha 512 ${imgtool_args})
  endif()

  if(NOT "${keyfile}" STREQUAL "")
    set(imgtool_args -k "${keyfile}" ${imgtool_args})
  endif()

  # Set up outputs
  add_custom_target(signed_softdevice_target
    ALL
    DEPENDS ${output_hex} ${output_bin}
  )

  add_custom_command(OUTPUT ${output_hex}
    COMMAND
    ${imgtool_sign} ${imgtool_args} ${output_file} ${output_hex}
    DEPENDS ${output_file} ${dependencies}
  )

  add_custom_command(OUTPUT ${output_bin}
    COMMAND
    ${imgtool_sign} ${imgtool_args} ${output_file} ${output_bin}
    DEPENDS ${output_file} ${dependencies}
  )
endfunction()

softdevice_tasks(${CMAKE_BINARY_DIR}/../softdevice.signed.hex ${CMAKE_BINARY_DIR}/../softdevice.signed.bin)
