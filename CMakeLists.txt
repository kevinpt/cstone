
project(cstone
  VERSION 0.1.0
  LANGUAGES C CXX
)


function(m4_template TEMPLATE T)
  # Convert a template to source code using m4

  # TEMPLATE should be a file with a extra ".m4" extension on the end.
  # It must have a path component containing the directory "template".
  # You can use any arbitrary directory structure after the "template" part.
  #
  # Ex:
  #   m4_template("/proj/template/xxx/code.h.m4" "uint8_t")
  #   Generates ${CMAKE_BINARY_DIR}/template/xxx/code_uint8_t.h
  #   Use dependency "template/xxx/code_uint8_t.h" in target sources

  cmake_path(GET TEMPLATE PARENT_PATH template_dir)
  cmake_path(GET TEMPLATE STEM template_stem)
  cmake_path(GET TEMPLATE EXTENSION template_ext)
  # Strip ".m4" final extension
  cmake_path(REMOVE_EXTENSION template_ext LAST_ONLY  OUTPUT_VARIABLE dest_ext)
  # Extract template portion of the path
  string(REGEX REPLACE ".*(template[/\\].*)$" "\\1" DEST_DIR ${template_dir})
  # Build destination path
  cmake_path(SET DEST "${CMAKE_BINARY_DIR}/${DEST_DIR}")
  cmake_path(APPEND DEST "${template_stem}_${T}${dest_ext}")
  #message("Generating: ${DEST} > ${DEST_DIR} from ${TEMPLATE}  ${PROJECT_SOURCE_DIR}")

  add_custom_command(
    OUTPUT ${DEST}
    COMMAND
      mkdir -p ${CMAKE_BINARY_DIR}/${DEST_DIR}
    COMMAND
      m4 -s -D "T=${T}" ${PROJECT_SOURCE_DIR}/${TEMPLATE} > ${DEST}
    DEPENDS ${PROJECT_SOURCE_DIR}/${TEMPLATE}
  )
endfunction(m4_template)


m4_template("template/cstone/bipbuf.h.m4"  "char")
m4_template("template/cstone/bipbuf.c.m4"  "char")


set(CSTONE_SOURCE_COMMON
    src/tasks_core.c
    src/debug.c
    src/led_blink.c
    src/sequence_events.c
    src/isr_queue.c
    src/dual_stream.c
    src/console.c
    src/console_shell.c
    src/blocking_io.c
    ${CMAKE_BINARY_DIR}/template/cstone/bipbuf_char.h
    ${CMAKE_BINARY_DIR}/template/cstone/bipbuf_char.c
    src/console_history.cpp
    src/console_uart.c
    src/console_usb.c
    src/console_stdio.c
    src/prop_id.c
    src/prop_db.c
    src/prop_serialize.c
    src/prop_flags.c
    src/storage.c
    src/log_db.c
    src/log_index.c
    src/log_info.c
    src/log_compress.c
    src/log_props.c
    src/log_ram.c
    src/umsg.c
    src/error_log.c
    src/rtos.c
    src/timing.c
    src/cmds_core.c
    src/rtc_device.c
    src/rtc_soft.c
    src/dump_reg.c
    src/profile.c
    src/cron_events.c
    src/util/histogram.c
    src/util/bsd_string.c
    src/util/search.c
    src/util/dhash.c
    src/util/range_strings.c
    src/util/num_format.c
    src/util/getopt_r.c
    src/util/mempool.c
    src/util/crc8.c
    src/util/crc16.c
    src/util/crc32.c
    src/util/hex_dump.c
    src/util/intmath.c
    src/util/string_ops.c
    src/util/glob.c
    src/util/stats.c
    src/util/random.c
)

set(CSTONE_SOURCE_STM32
    src/obj_metadata.c  # FIXME: Move to platform
    src/platform/stm32/core_stm32.c
    src/platform/stm32/uart_stm32.c
    src/platform/stm32/gpio_stm32.c
    src/platform/stm32/target_stm32.c
    src/platform/stm32/rtos_stm32.c
    src/platform/stm32/cmds_stm32.c
    src/platform/stm32/log_stm32.c
    src/platform/stm32/faults.c
    src/platform/stm32/rtc_stm32.c
    src/platform/stm32/crc32_stm32.c
    src/platform/stm32/cycle_counter_cortex.c
)

set(CSTONE_SOURCE_POSIX
    src/platform/posix/target_posix.c
    src/platform/posix/rtc_hosted.c
)


set(CSTONE_SOURCE
  ${CSTONE_SOURCE_COMMON}
  $<$<BOOL:${PLATFORM_STM32}>:${CSTONE_SOURCE_STM32}>
  $<$<BOOL:${PLATFORM_HOSTED}>:${CSTONE_SOURCE_POSIX}>
)


#set_source_files_properties(${XLIB_SOURCE}
#  PROPERTIES
#    COMPILE_FLAGS "-Wno-implicit-fallthrough -Wno-missing-prototypes -Wno-redundant-decls"
#)


add_library(cstone STATIC ${CSTONE_SOURCE})


target_include_directories(cstone
  PUBLIC
    "include"
  PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/lib_cfg"
    "${CMAKE_SOURCE_DIR}/include/stm32"
    "${CMAKE_BINARY_DIR}/include"
    "${CMAKE_BINARY_DIR}/include/lib_cfg"
    "${CMAKE_BINARY_DIR}/template"
    "${CMSIS_ROOT}/Device/ST/${DEVICE_FAMILY_UC}xx/Include"
    "${CMSIS_ROOT}/Core/Include"
    "${HAL_ROOT}/Inc"
    "${FREERTOS_ROOT}/include"
    "${FREERTOS_PORT_INCLUDE}"
#    "${FREERTOS_CFG_INCLUDE}"
    ${HEATSHRINK_ROOT}
)

target_compile_definitions(cstone
  PUBLIC
    USE_FREERTOS
    ${DEVICE_FAMILY_UC}
    ${DEVICE_MODEL_UC}
    USE_HAL_DRIVER
    USE_FULL_LL_DRIVER
#    $<$<BOOL:${USE_MINIMAL_TASKS}>:USE_MINIMAL_TASKS>
)


