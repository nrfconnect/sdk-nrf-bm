zephyr_library_sources(soc.c)

set_source_files_properties(
  ${ZEPHYR_BASE}/soc/nordic/nrf54l/soc.c
  DIRECTORY ${ZEPHYR_BASE}/soc/nordic
  PROPERTIES HEADER_FILE_ONLY ON
)
