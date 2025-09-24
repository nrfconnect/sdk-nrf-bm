#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

function(setup_bootconf_data)
    add_custom_target(bootconf_target
      ALL
      DEPENDS ${CMAKE_BINARY_DIR}/bootconf.hex
    )

  dt_nodelabel(boot_partition_node_full_path NODELABEL "boot_partition")
  dt_reg_size(boot_partition_node_size PATH "${boot_partition_node_full_path}")
  if(${boot_partition_node_size} GREATER 0x7c00)
    message(WARNING "boot_partition doesn't fit into protection region.
Protection will be applied over maximum allowed span.")
  endif()

    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bootconf.hex
      COMMAND ${Python3_EXECUTABLE}
        ${ZEPHYR_NRF_MODULE_DIR}/scripts/reglock.py
        --output ${CMAKE_BINARY_DIR}/bootconf.hex
        --size ${boot_partition_node_size}
        --soc ${CONFIG_SOC}
      VERBATIM
    )

endfunction()

setup_bootconf_data()
