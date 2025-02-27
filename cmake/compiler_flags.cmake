# Copyright 2022-2024 Intel Corporation.
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise, you may
# not use, modify, copy, publish, distribute, disclose or transmit this
# software or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express
# or implied warranties, other than those that are expressly stated in
# the License.

add_compile_options(
  # Compiler warnings
  -Wall -Wextra -Werror -Wno-unused-parameter -Wvla
  # Control flow integrity
  $<$<CONFIG:Release>:-fcf-protection=full>
  # Format string defence
  -Wformat -Wformat-security -Werror=format-security
  # Stack protection
  $<$<CONFIG:Debug>:-fstack-protector>
  $<$<CONFIG:Release>:-fstack-protector-strong>
  $<$<CONFIG:Release>:-fstack-clash-protection>
  # Use relative paths for debug information
  -fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.
)

if(NOT ${LINUX_SYSTEM_NAME} STREQUAL "cros_sdk")
    add_compile_options(
      # Enable macro security features
      $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=2>
      # TODO: Adding O3 in release build help to overcome issue with FORTIFY_SOURCE
      # issue needs further investigation
      $<$<CONFIG:Release>:-O3>
    )
endif()

add_link_options(
  # Read only relocation (RERLO)
  "SHELL:$<$<CONFIG:Release>:-z relro>"
  # Immediate Binding protection
  "SHELL:$<$<CONFIG:Release>:-z now>"
  # Inexecutable stack
  "SHELL:$<$<CONFIG:Release>:-z noexecstack>"
)
