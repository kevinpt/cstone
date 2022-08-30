set(CSTONE_SOURCE_COMMON
    src/tasks_core.c
    src/debug.c
    src/led_blink.c
    src/isr_queue.c
    src/console.c
    src/console_shell.c
    src/blocking_io.c
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
    src/util/histogram.c
    src/util/bsd_string.c
    src/util/search.c
    src/util/dhash.c
    src/util/range_strings.c
    src/util/getopt_r.c
    src/util/mempool.c
    src/util/crc8.c
    src/util/crc16.c
    src/util/hex_dump.c
    src/util/intmath.c
    src/util/string_ops.c
    src/util/glob.c
    src/util/stats.c
)

set(CSTONE_SOURCE_STM32
    src/platform/stm32/uart_stm32.c
    src/platform/stm32/gpio_stm32.c
    src/platform/stm32/target_stm32.c
    src/platform/stm32/rtos_stm32.c
    src/platform/stm32/cmds_stm32.c
    src/platform/stm32/log_stm32.c
    src/platform/stm32/faults.c
)

set(CSTONE_SOURCE_POSIX
    src/platform/posix/target_posix.c
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
    "include/util"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/lib_cfg"
    "${CMAKE_SOURCE_DIR}/include/stm32"
    "${CMAKE_BINARY_DIR}/include"
    "${CMAKE_BINARY_DIR}/include/lib_cfg"
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


