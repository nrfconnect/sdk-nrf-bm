string(REPLACE "/" ";" board_qualifer_list "${BOARD_QUALIFIERS}")
list(LENGTH board_qualifer_list board_qualifer_list_len)

if(${board_qualifer_list_len} LESS 4)
  message(FATAL_ERROR "A board target must be supplied")
endif()

set(board_qualifer_list)
set(board_qualifer_list_len)
