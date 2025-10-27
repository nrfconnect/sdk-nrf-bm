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
      add_overlay_config(mcuboot ${CMAKE_CURRENT_SOURCE_DIR}/image_configurations/BOOTLOADER_image_default.conf)

      # Enable LTO on MCUboot image
      add_overlay_config(mcuboot ${CMAKE_CURRENT_SOURCE_DIR}/image_configurations/lto.conf)

      # Set up hash algorithm for MCUboot and images
      if(SB_CONFIG_BM_BOOT_IMG_HASH_ALG_SHA256)
        set_config_bool(mcuboot CONFIG_BOOT_SIGNATURE_TYPE_PURE n)
        set_config_bool(mcuboot CONFIG_BOOT_IMG_HASH_ALG_SHA512 n)
        set_config_bool(${DEFAULT_IMAGE} CONFIG_MCUBOOT_BOOTLOADER_SIGNATURE_TYPE_PURE n)
        set_config_bool(${DEFAULT_IMAGE} CONFIG_MCUBOOT_BOOTLOADER_USES_SHA512 n)
        set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_MCUBOOT_BOOTLOADER_SIGNATURE_TYPE_PURE n)
        set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_MCUBOOT_BOOTLOADER_USES_SHA512 n)
      elseif(SB_CONFIG_BM_BOOT_IMG_HASH_ALG_SHA512)
        set_config_bool(mcuboot CONFIG_BOOT_SIGNATURE_TYPE_PURE n)
        set_config_bool(mcuboot CONFIG_BOOT_IMG_HASH_ALG_SHA512 y)
        set_config_bool(${DEFAULT_IMAGE} CONFIG_MCUBOOT_BOOTLOADER_SIGNATURE_TYPE_PURE n)
        set_config_bool(${DEFAULT_IMAGE} CONFIG_MCUBOOT_BOOTLOADER_USES_SHA512 y)
        set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_MCUBOOT_BOOTLOADER_SIGNATURE_TYPE_PURE n)
        set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_MCUBOOT_BOOTLOADER_USES_SHA512 y)
      elseif(SB_CONFIG_BM_BOOT_IMG_HASH_ALG_PURE)
        set_config_bool(mcuboot CONFIG_BOOT_SIGNATURE_TYPE_PURE y)
        set_config_bool(mcuboot CONFIG_BOOT_IMG_HASH_ALG_SHA512 y)
        set_config_bool(${DEFAULT_IMAGE} CONFIG_MCUBOOT_BOOTLOADER_SIGNATURE_TYPE_PURE y)
        set_config_bool(${DEFAULT_IMAGE} CONFIG_MCUBOOT_BOOTLOADER_USES_SHA512 y)
        set_config_bool(${DEFAULT_IMAGE} CONFIG_MCUBOOT_BOOTLOADER_SIGNATURE_TYPE_ED25519 y)
        set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_MCUBOOT_BOOTLOADER_SIGNATURE_TYPE_PURE y)
        set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_MCUBOOT_BOOTLOADER_USES_SHA512 y)
        set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_MCUBOOT_BOOTLOADER_SIGNATURE_TYPE_ED25519 y)
      endif()

      if(SB_CONFIG_SOC_SERIES_NRF54LX)
        if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_NONE)
          set_config_bool(mcuboot CONFIG_NRF_SECURITY y)
        elseif(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_ED25519)
          set_config_bool(mcuboot CONFIG_NRF_SECURITY y)

          # We are sure that ED25519 signature on MCUboot does not need these
          set_config_bool(mcuboot CONFIG_PSA_USE_CRACEN_AEAD_DRIVER n)
          set_config_bool(mcuboot CONFIG_PSA_USE_CRACEN_PAKE_DRIVER n)
          set_config_bool(mcuboot CONFIG_PSA_USE_CRACEN_CIPHER_DRIVER n)
          set_config_bool(mcuboot CONFIG_PSA_USE_CRACEN_MAC_DRIVER n)
          set_config_bool(mcuboot CONFIG_PSA_USE_CRACEN_KEY_AGREEMENT_DRIVER n)
          set_config_bool(mcuboot CONFIG_PSA_USE_CRACEN_KEY_DERIVATION_DRIVER n)
          set_config_bool(mcuboot CONFIG_BOOT_HMAC_SHA512 n)
          set_config_bool(mcuboot CONFIG_BOOT_KEY_IMPORT_BYPASS_ASN y)

          if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_USING_KMU)
            set_config_bool(mcuboot CONFIG_BOOT_SIGNATURE_USING_KMU y)
            set_config_bool(mcuboot CONFIG_PSA_USE_CRACEN_HASH_DRIVER n)
            set_config_bool(mcuboot CONFIG_MBEDTLS_ENABLE_HEAP n)
            set_config_bool(mcuboot CONFIG_PSA_CORE_LITE y)
            set_config_bool(mcuboot CONFIG_PSA_CORE_LITE_NSIB_ED25519_OPTIMIZATIONS y)
            set_config_bool(mcuboot CONFIG_BOOT_SIGNATURE_TYPE_PURE y)
          else()
            set_config_bool(mcuboot CONFIG_BOOT_SIGNATURE_USING_KMU n)
            set_config_bool(mcuboot CONFIG_PSA_USE_CRACEN_HASH_DRIVER y)
          endif()
        endif()
        if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_KEYS_REVOCATION)
          set_config_bool(mcuboot CONFIG_BOOT_KEYS_REVOCATION y)
        endif()
      endif()

      ExternalZephyrProject_Add(
        APPLICATION installer
        SOURCE_DIR ${ZEPHYR_NRF_BM_MODULE_DIR}/applications/installer
        BUILD_ONLY y
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

      set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_MCUBOOT_APPLICATION_FIRMWARE_UPDATER y)

      # MCUboot firmware loader entrance mode selection
      if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_FIRMWARE_LOADER_ENTRANCE_BOOT_MODE)
        set_config_bool(mcuboot CONFIG_BOOT_FIRMWARE_LOADER_BOOT_MODE y)
        set_config_bool(mcuboot CONFIG_RETAINED_MEM y)
        set_config_bool(mcuboot CONFIG_RETENTION y)
        set_config_bool(mcuboot CONFIG_RETENTION_BOOT_MODE y)

        # Also enable boot mode for main application
        set_config_bool(${DEFAULT_IMAGE} CONFIG_RETAINED_MEM y)
        set_config_bool(${DEFAULT_IMAGE} CONFIG_RETENTION y)
        set_config_bool(${DEFAULT_IMAGE} CONFIG_RETENTION_BOOT_MODE y)
      endif()

      if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_FIRMWARE_LOADER_ENTRANCE_PIN_RESET)
        set_config_bool(mcuboot CONFIG_BOOT_FIRMWARE_LOADER_PIN_RESET y)
      endif()

      if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_FIRMWARE_LOADER_ENTRANCE_GPIO)
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

  if(SB_CONFIG_SOFTDEVICE_NONE)
    set_config_bool(${DEFAULT_IMAGE} CONFIG_SOFTDEVICE n)

    if(NOT DEFINED SB_CONFIG_BM_BOOTLOADER_NONE AND NOT DEFINED SB_CONFIG_BM_FIRMWARE_LOADER_NONE)
      set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_SOFTDEVICE n)
    endif()
  else()
    foreach(option SOFTDEVICE_S115 SOFTDEVICE_S145)
      if(SB_CONFIG_${option})
        set_config_bool(${DEFAULT_IMAGE} CONFIG_${option} y)

        if(NOT DEFINED SB_CONFIG_BM_BOOTLOADER_NONE AND NOT DEFINED SB_CONFIG_BM_FIRMWARE_LOADER_NONE AND NOT DEFINED SB_CONFIG_BM_FIRMWARE_LOADER_UART_MCUMGR)
          set_config_bool(${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME} CONFIG_${option} y)
        endif()
        break()
      endif()
    endforeach()
  endif()
endfunction()

function(${SYSBUILD_CURRENT_MODULE_NAME}_post_cmake)
  if(NOT DEFINED SB_CONFIG_NCS_BM)
    return()
  endif()

  cmake_parse_arguments(PRE_CMAKE "" "" "IMAGES" ${ARGN})

  if(NOT DEFINED SB_CONFIG_BM_BOOTLOADER_NONE)
    set(check_targets mcuboot;${DEFAULT_IMAGE})
    set(dependency_targets ${SB_CONFIG_SOFTDEVICE_FILE})
    set(production_targets mcuboot;${DEFAULT_IMAGE})
    set(production_files)

    if(NOT DEFINED SB_CONFIG_BM_FIRMWARE_LOADER_NONE)
      list(APPEND check_targets installer)
      list(APPEND check_targets ${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME})
      list(APPEND production_targets ${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME})
    endif()

    foreach(image ${check_targets})
      sysbuild_get(${image}_kernel_bin_name IMAGE ${image} VAR CONFIG_KERNEL_BIN_NAME KCONFIG)
      sysbuild_get(${image}_binary_dir IMAGE ${image} VAR APPLICATION_BINARY_DIR CACHE)

      list(APPEND dependency_targets
        ${image}_extra_byproducts
        ${${image}_binary_dir}/zephyr/.config
        ${${image}_binary_dir}/zephyr/${${image}_kernel_bin_name}.bin
      )

      if("${image}" STREQUAL "${SB_CONFIG_BM_FIRMWARE_LOADER_IMAGE_NAME}")
        list(APPEND dependency_targets
          ${${image}_binary_dir}/zephyr/${${image}_kernel_bin_name}.signed.bin
        )
      endif()
    endforeach()

    # Installer metadata
    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/metadata.hex
      ${CMAKE_BINARY_DIR}/combined_signed_update_images.hex
      COMMAND
      ${PYTHON_EXECUTABLE}
      ${ZEPHYR_NRF_BM_MODULE_DIR}/scripts/generate_metadata.py
      --build-dir ${CMAKE_BINARY_DIR}
      # This command depends on softdevice.signed.bin also but a dependency has been added to the
      # MCUboot build target instead
      DEPENDS ${dependency_targets} ${CMAKE_BINARY_DIR}/zephyr/.config
    )

    add_custom_target(metadata_generation
      ALL
      DEPENDS ${CMAKE_BINARY_DIR}/metadata.hex
      ${CMAKE_BINARY_DIR}/combined_signed_update_images.hex
    )

    if(NOT SB_CONFIG_BM_FIRMWARE_LOADER_NONE)
      include(${ZEPHYR_NRF_BM_MODULE_DIR}/cmake/sysbuild/image_signing_installer.cmake)
    endif()

    foreach(image ${production_targets})
      sysbuild_get(${image}_kernel_bin_name IMAGE ${image} VAR CONFIG_KERNEL_BIN_NAME KCONFIG)
      sysbuild_get(${image}_binary_dir IMAGE ${image} VAR APPLICATION_BINARY_DIR CACHE)

      if("${image}" STREQUAL "mcuboot")
        list(APPEND production_files
          ${${image}_binary_dir}/zephyr/${${image}_kernel_bin_name}_signed_softdevice_flash_metadata.hex)
      else()
        list(APPEND production_files
          ${${image}_binary_dir}/zephyr/${${image}_kernel_bin_name}.signed.hex)
      endif()
    endforeach()

    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/production.hex
      COMMAND
      ${PYTHON_EXECUTABLE}
      ${ZEPHYR_BASE}/scripts/build/mergehex.py
      -o ${CMAKE_BINARY_DIR}/production.hex
      ${production_files}
      # This command depends on zephyr_signed_softdevice_flash_metadata.hex also but a dependency
      # has been added to the MCUboot build target instead
      DEPENDS ${dependency_targets} ${production_files} ${CMAKE_BINARY_DIR}/zephyr/.config
    )

    add_custom_target(production_generation
      ALL
      DEPENDS ${CMAKE_BINARY_DIR}/production.hex
    )

    if(SB_CONFIG_BM_BOOTLOADER_MCUBOOT_GENERATE_DEFAULT_KMU_KEYFILE)
      include(${ZEPHYR_NRF_BM_MODULE_DIR}/cmake/sysbuild/generate_default_keyfile.cmake)
    endif()
  endif()
endfunction()

if(SB_CONFIG_NCS_BM)
  bm_install_setup()
endif()
