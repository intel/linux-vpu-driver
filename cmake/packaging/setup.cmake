#
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#


if(${CMAKE_VERSION} VERSION_LESS "3.20")
  # Using older version stucks on detecting the package dependencies
  message(STATUS "CPack Debian in driver is not supported by cmake version less than 3.20.1")
  return()
endif()

# Base information
set(CPACK_PACKAGE_NAME "intel")
set(CPACK_PACKAGE_VENDOR "Intel")
set(CPACK_PACKAGE_CONTACT "Intel Corporation")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION}.${BUILD_NUMBER})
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_BINARY_DIR}/CPackProjectConfigFile.cmake)

# Component descriptions
set(CPACK_COMPONENT_DRIVER-COMPILER-NPU_DESCRIPTION "Intel(R) Driver Compiler for NPU hardware\n\
The driver compiler enables compilation of OpenVINO IR models using Level Zero Graph Extension API.")
set(CPACK_COMPONENT_FW-NPU_DESCRIPTION "Intel(R) Firmware package for NPU hardware\n\
Firmware files required by IVPU kernel module.")
set(CPACK_COMPONENT_LEVEL-ZERO-NPU_DESCRIPTION "Intel(R) Level Zero Driver for NPU hardware\n\
Library implements Level Zero API to interract with NPU hardware.")
set(CPACK_COMPONENT_VALIDATION-NPU_DESCRIPTION "Intel(R) Validation applications for NPU\n\
Set of application required for testing of Intel(R) Level Zero Driver for NPU hardware.")

file(WRITE ${CPACK_PROJECT_CONFIG_FILE}
           "set(FW-NPU_POSTINST ${CMAKE_CURRENT_LIST_DIR}/postinst)\n"
           "set(EXTERNAL_COMPONENTS driver-compiler-npu;fw-npu;level-zero-npu)\n")

# Component list
get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL level-zero-devel)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL openvino_standalone_package)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL npu_compiler_package)

if (EXISTS "/etc/debian_version")
    set(PACKAGE_TYPE "deb")
elseif(EXISTS "/etc/portage")
    set(PACKAGE_TYPE "ebuild")
elseif(EXISTS "/etc/redhat-release")
    set(PACKAGE_TYPE "rpm")
else()
    message(WARNING "Unable to detect package type for this system")
    return()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/generators/${PACKAGE_TYPE}.cmake)
include(CPack)
