#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

function(bm_install_setup)
  if(NOT DEFINED SB_CONFIG_BM_BOOTLOADER_NONE)
    ExternalZephyrProject_Add(
      APPLICATION ${SB_CONFIG_BM_BOOTLOADER_IMAGE_NAME}
      SOURCE_DIR ${SB_CONFIG_BM_BOOTLOADER_IMAGE_PATH}
    )

    # Place MCUBoot first in list to ensure it is configured and flashed before other images.
    sysbuild_add_dependencies(CONFIGURE ${DEFAULT_IMAGE} ${SB_CONFIG_BM_BOOTLOADER_IMAGE_NAME})
    sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} ${SB_CONFIG_BM_BOOTLOADER_IMAGE_NAME})

    if(NOT DEFINED SB_CONFIG_BM_FIRMWARE_LOADER_NONE)
      ExternalZephyrProject_Add(
        APPLICATION ${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME}
        SOURCE_DIR ${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_PATH}
      )

      add_overlay_dts(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/image_configurations/FIRMWARE_LOADER_image_default.overlay)

      ExternalZephyrProject_Add(
        APPLICATION installer
        SOURCE_DIR ${ZEPHYR_NRF_BM_MODULE_DIR}/applications/installer
      )
    endif()
  endif()
endfunction()

function(${SYSBUILD_CURRENT_MODULE_NAME}_pre_cmake)
  cmake_parse_arguments(PRE_CMAKE "" "" "IMAGES" ${ARGN})

  if(NOT DEFINED SB_CONFIG_NCS_BM)
    # Disable bare-metal on all images
    foreach(image ${PRE_CMAKE_IMAGES})
      set_config_int(${image} CONFIG_NCS_BM n)
    endforeach()

    return()
  endif()

  if(NOT DEFINED SB_CONFIG_BM_BOOTLOADER_NONE)
    set(bm_install_images 1)

    if(NOT DEFINED SB_CONFIG_BM_FIRMWARE_LOADER_NONE)
      # Firmware loader present
      set_target_properties(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} PROPERTIES
        IMAGE_CONF_SCRIPT ${ZEPHYR_NRF_BM_MODULE_DIR}/sysbuild/image_configurations/MAIN_image_default.cmake
      )

      set(bm_install_images 2)

      # MCUboot firmware loader entrance mode selection
      if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_FIRMWARE_LOADER_ENTRANCE_BOOT_MODE)
        set_config_bool(mcuboot CONFIG_BOOT_FIRMWARE_LOADER_BOOT_MODE y)
      endif()

      if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_FIRMWARE_LOADER_ENTRANCE_PIN_RESET)
        set_config_bool(mcuboot CONFIG_BOOT_FIRMWARE_LOADER_PIN_RESET y)
      endif()

      if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_FIRMWARE_LOADER_ENTRANCE_GPIO)
        set_config_bool(mcuboot CONFIG_GPIO y)
        set_config_bool(mcuboot CONFIG_BM_TIMER y)
        set_config_bool(mcuboot CONFIG_BM_BUTTONS y)
        set_config_bool(mcuboot CONFIG_BOOT_FIRMWARE_LOADER_ENTRANCE_GPIO y)
      endif()
    endif()

    # Use BM-supplied bootloader image configuration file
    get_property(tmp_conf_scripts TARGET ${SB_CONFIG_BM_BOOTLOADER_IMAGE_NAME} PROPERTY IMAGE_CONF_SCRIPT)
    set(bootloader_script ${ZEPHYR_NRF_BM_MODULE_DIR}/sysbuild/image_configurations/BOOTLOADER_image_default.cmake)

    if(NOT bootloader_script IN_LIST tmp_conf_scripts)
      list(APPEND tmp_conf_scripts ${bootloader_script})
      set_target_properties(${SB_CONFIG_BM_BOOTLOADER_IMAGE_NAME} PROPERTIES IMAGE_CONF_SCRIPT "${tmp_conf_scripts}")
    endif()
  else()
    set(bm_install_images 0)
  endif()

  set_target_properties(${DEFAULT_IMAGE} PROPERTIES
    IMAGE_CONF_SCRIPT ${ZEPHYR_NRF_BM_MODULE_DIR}/sysbuild/image_configurations/MAIN_image_default.cmake
  )

  math(EXPR metadata_area_size "${SB_CONFIG_METADATA_SIZE}" OUTPUT_FORMAT DECIMAL)
  math(EXPR metadata_slots "${metadata_area_size} / ${SB_CONFIG_METADATA_ENTRY_SIZE}" OUTPUT_FORMAT DECIMAL)

  foreach(image ${PRE_CMAKE_IMAGES})
    set_config_int(${image} CONFIG_BM_INSTALL_IMAGES ${bm_install_images})
    set_config_int(${image} CONFIG_BM_INSTALL_ENTRY_SIZE ${SB_CONFIG_METADATA_ENTRY_SIZE})
    set_config_int(${image} CONFIG_BM_INSTALL_ENTRIES ${metadata_slots})

    if(image STREQUAL "installer")
      set_config_int(${image} CONFIG_BM_METADATA_WRITE y)
    else()
      set_config_int(${image} CONFIG_BM_METADATA_WRITE n)
    endif()

    if(NOT DEFINED SB_CONFIG_BM_BOOTLOADER_NONE AND NOT image STREQUAL "mcuboot")
      if(SB_CONFIG_SOC_SERIES_NRF54LX)
        set_config_int(installer CONFIG_ROM_START_OFFSET 0x800)
      else()
        set_config_int(installer CONFIG_ROM_START_OFFSET 0x200)
      endif()
    endif()
  endforeach()
endfunction()

function(${SYSBUILD_CURRENT_MODULE_NAME}_post_cmake)
  if(NOT DEFINED SB_CONFIG_NCS_BM)
    return()
  endif()

  cmake_parse_arguments(PRE_CMAKE "" "" "IMAGES" ${ARGN})

  if(NOT SB_CONFIG_SOFTDEVICE_NONE)
    set(dependency_targets)

    foreach(image ${PRE_CMAKE_IMAGES})
      list(APPEND dependency_targets ${image}_extra_byproducts)
    endforeach()

    # Installer metadata
    add_custom_target(metadata_generation
      ALL
      DEPENDS ${CMAKE_BINARY_DIR}/metadata.hex ${CMAKE_BINARY_DIR}/metadata.bin
      ${CMAKE_BINARY_DIR}/combined_signed_update_images.hex
    )

    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/metadata.hex ${CMAKE_BINARY_DIR}/metadata.bin
      ${CMAKE_BINARY_DIR}/combined_signed_update_images.hex
      COMMAND
      ${PYTHON_EXECUTABLE}
      ${ZEPHYR_NRF_BM_MODULE_DIR}/scripts/generate_metadata.py
      --build-dir ${CMAKE_BINARY_DIR}
      DEPENDS ${dependency_targets} ../zephyr/.config
    )

    if(NOT SB_CONFIG_BM_FIRMWARE_LOADER_NONE)
      include(${ZEPHYR_NRF_BM_MODULE_DIR}/cmake/sysbuild/image_signing_installer.cmake)
    endif()
  endif()
endfunction()

if(SB_CONFIG_NCS_BM)
  bm_install_setup()
endif()
