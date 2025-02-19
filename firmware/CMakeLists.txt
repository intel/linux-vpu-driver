#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(fw_vpu_api_headers)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

file(GLOB FIRMWARE_BINARIES ${CMAKE_CURRENT_SOURCE_DIR}/bin/*.bin)
if (FIRMWARE_BINARIES)
  install(FILES ${FIRMWARE_BINARIES}
    DESTINATION lib/firmware/updates/intel/vpu/
    PERMISSIONS OWNER_READ
    COMPONENT fw-npu)
endif()
