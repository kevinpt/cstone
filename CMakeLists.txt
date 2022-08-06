file(GLOB_RECURSE CSTONE_SOURCE
  "src/*.c"
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
    "../../include"
    "../../${CMSIS_ROOT}/Device/ST/${DEVICE_FAMILY_UC}xx/Include"
    "../../${CMSIS_ROOT}/Core/Include"
    "../../${FREERTOS_ROOT}/include"
    "../../${FREERTOS_PORT_INCLUDE}"
    "../../${FREERTOS_CFG_INCLUDE}"
    "../heatshrink"
)

target_compile_definitions(cstone
  PUBLIC
    USE_FREERTOS
    ${DEVICE_FAMILY_UC}
    ${DEVICE_MODEL_UC}xx
)
