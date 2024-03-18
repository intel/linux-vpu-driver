/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "umd_common.hpp"

#include "level_zero_driver/core/source/device/device.hpp"
#include "level_zero_driver/core/source/cmdlist/cmdlist.hpp"
#include "level_zero_driver/core/source/cmdqueue/cmdqueue.hpp"
#include "level_zero_driver/ext/source/graph/graph.hpp"
#include "level_zero_driver/core/source/driver/driver_handle.hpp"
#include "level_zero_driver/tools/source/metrics/metric.hpp"

#include "vpu_driver/source/utilities/log.hpp"
#include "vpu_driver/source/device/vpu_device.hpp"

#include <algorithm>
#include <bitset>
#include <string.h>

namespace L0 {

Device::Device(DriverHandle *driverHandle, std::unique_ptr<VPU::VPUDevice> device)
    : driverHandle(driverHandle)
    , vpuDevice(std::move(device))
    , metricContext(std::make_shared<MetricContext>(this)) {
    if (vpuDevice != nullptr) {
        Driver *pDriver = Driver::getInstance();
        if (pDriver && pDriver->getEnvVariables().metrics) {
            std::vector<VPU::GroupInfo> metricGroupsInfo = vpuDevice->getMetricGroupsInfo();
            loadMetricGroupsInfo(metricGroupsInfo);
        }
        if (!Compiler::compilerInit(vpuDevice->getHwInfo().compilerPlatform)) {
            LOG_W("Failed to initialize VPU compiler.");
        }
    }
}

DriverHandle *Device::getDriverHandle() {
    return driverHandle;
}

ze_result_t Device::getP2PProperties(ze_device_handle_t hPeerDevice,
                                     ze_device_p2p_properties_t *pP2PProperties) {
    if (nullptr == hPeerDevice) {
        LOG_E("Invalid PeerDevice handle.");
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    if (nullptr == pP2PProperties) {
        LOG_E("Invalid pP2PProperties pointer.");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    // Setting property flag
    pP2PProperties->flags = 0;
    return ZE_RESULT_SUCCESS;
}

ze_result_t Device::getProperties(ze_device_properties_t *pDeviceProperties) {
    if (pDeviceProperties == nullptr) {
        LOG_E("Invalid pDeviceProperties pointer.");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    auto vpuDevice = getVPUDevice();
    if (vpuDevice == nullptr) {
        LOG_E("Failed to get VPUDevice instance.");
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    const auto &hwInfo = vpuDevice->getHwInfo();

    pDeviceProperties->type = ZE_DEVICE_TYPE_VPU;
    pDeviceProperties->vendorId = INTEL_PCI_VENDOR_ID;
    pDeviceProperties->deviceId = hwInfo.deviceId;
    pDeviceProperties->subdeviceId = hwInfo.deviceRevision;
    pDeviceProperties->coreClockRate = hwInfo.coreClockRate;
    pDeviceProperties->maxMemAllocSize = hwInfo.maxMemAllocSize;
    pDeviceProperties->maxHardwareContexts = hwInfo.maxHardwareContexts;
    pDeviceProperties->maxCommandQueuePriority = hwInfo.maxCommandQueuePriority;
    pDeviceProperties->numThreadsPerEU = hwInfo.numThreadsPerEU;
    pDeviceProperties->physicalEUSimdWidth = hwInfo.physicalEUSimdWidth;
    pDeviceProperties->numEUsPerSubslice = hwInfo.nExecUnits;
    pDeviceProperties->numSubslicesPerSlice = hwInfo.numSubslicesPerSlice;
    pDeviceProperties->numSlices =
        static_cast<uint32_t>(std::bitset<32>(hwInfo.tileConfig).count());

    if (pDeviceProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2) {
        // the units are in cycles/sec
        pDeviceProperties->timerResolution = 38'400'000;
    } else if (pDeviceProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES) {
        // the units are in nanoseconds
        pDeviceProperties->timerResolution = NS_IN_SEC / 38'400'000;
    }

    pDeviceProperties->timestampValidBits = 64u;
    pDeviceProperties->kernelTimestampValidBits = 0u;

    strncpy(pDeviceProperties->name, hwInfo.name, ZE_MAX_DEVICE_NAME - 1);
    pDeviceProperties->name[ZE_MAX_DEVICE_NAME - 1] = '\0';

    pDeviceProperties->flags = ZE_DEVICE_PROPERTY_FLAG_INTEGRATED;
    pDeviceProperties->uuid = ze_intel_vpu_device_uuid;

    // Using the structure ze_device_ip_version_ext_t to store the platformType value
    if (pDeviceProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES &&
        pDeviceProperties->pNext != nullptr) {
        ze_device_ip_version_ext_t *deviceDetails =
            reinterpret_cast<ze_device_ip_version_ext_t *>(pDeviceProperties->pNext);

        if (deviceDetails->stype == ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT) {
            deviceDetails->ipVersion = hwInfo.platformType;
        }
    }

    LOG_I("Returning device properties.");
    return ZE_RESULT_SUCCESS;
}

ze_result_t Device::getSubDevices(uint32_t *pCount, ze_device_handle_t *phSubdevices) {
    if (nullptr == pCount) {
        LOG_E("Invalid pCount pointer.");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (*pCount > 0)
        *pCount = 0;

    return ZE_RESULT_SUCCESS;
}

ze_result_t Device::setIntermediateCacheConfig(ze_cache_config_flags_t cacheConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t Device::setLastLevelCacheConfig(ze_cache_config_flags_t cacheConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

const char *Device::getDeviceMemoryName() const {
    return "DDR";
}

ze_result_t Device::getMemoryProperties(uint32_t *pCount,
                                        ze_device_memory_properties_t *pMemProperties) {
    if (nullptr == pCount) {
        LOG_E("Invalid memory properties count pointer.");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (*pCount == 0) {
        *pCount = 1;
        return ZE_RESULT_SUCCESS;
    }

    *pCount = 1;

    if (nullptr == vpuDevice) {
        LOG_E("VPU device instance is invalid.");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    if (nullptr == pMemProperties) {
        LOG_I("Input memory properties pointer is NULL.");
    } else {
        const auto &hwInfo = vpuDevice->getHwInfo();

        pMemProperties->flags = 0u;
        pMemProperties->maxClockRate = hwInfo.coreClockRate;
        pMemProperties->maxBusWidth = 32u;
        pMemProperties->totalSize = hwInfo.maxMemAllocSize;
        strncpy(pMemProperties->name, getDeviceMemoryName(), ZE_MAX_DEVICE_NAME - 1);
        pMemProperties->name[ZE_MAX_DEVICE_NAME - 1] = '\0';
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t
Device::getMemoryAccessProperties(ze_device_memory_access_properties_t *pMemAccessProperties) {
    if (pMemAccessProperties == nullptr) {
        LOG_E("Invalid pMemAccessProperties pointer.");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    pMemAccessProperties->hostAllocCapabilities =
        ZE_MEMORY_ACCESS_CAP_FLAG_RW | ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC;
    pMemAccessProperties->deviceAllocCapabilities =
        ZE_MEMORY_ACCESS_CAP_FLAG_RW | ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC;
    pMemAccessProperties->sharedSingleDeviceAllocCapabilities = 0;
    pMemAccessProperties->sharedCrossDeviceAllocCapabilities = 0;
    pMemAccessProperties->sharedSystemAllocCapabilities = 0;
    return ZE_RESULT_SUCCESS;
}

ze_result_t Device::getDeviceImageProperties(ze_device_image_properties_t *pDeviceImageProperties) {
    if (pDeviceImageProperties == nullptr) {
        LOG_E("Invalid pDeviceImageProperties pointer.");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    pDeviceImageProperties->maxImageDims1D = 0u;
    pDeviceImageProperties->maxImageDims2D = 0u;
    pDeviceImageProperties->maxImageDims3D = 0u;
    pDeviceImageProperties->maxImageBufferSize = 0u;
    pDeviceImageProperties->maxImageArraySlices = 0u;
    pDeviceImageProperties->maxSamplers = 0u;
    pDeviceImageProperties->maxReadImageArgs = 0u;
    pDeviceImageProperties->maxWriteImageArgs = 0u;

    return ZE_RESULT_SUCCESS;
}

ze_result_t
Device::getDeviceComputeProperties(ze_device_compute_properties_t *pDeviceComputeProperties) {
    if (pDeviceComputeProperties == nullptr) {
        LOG_E("Invalid pDeviceComputeProperties pointer.");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    // Maximum items per compute group
    pDeviceComputeProperties->maxTotalGroupSize = 0u;

    // Maximum items for X dimension in group
    pDeviceComputeProperties->maxGroupSizeX = 0u;

    // Maximum items for Y dimension in group
    pDeviceComputeProperties->maxGroupSizeY = 0u;

    // Maximum items for Z dimension in group
    pDeviceComputeProperties->maxGroupSizeZ = 0u;

    // Maximum groups that can be launched for x dimension
    pDeviceComputeProperties->maxGroupCountX = 0u;

    // Maximum groups that can be launched for y dimension
    pDeviceComputeProperties->maxGroupCountY = 0u;

    // Maximum groups that can be launched for z dimension
    pDeviceComputeProperties->maxGroupCountZ = 0u;

    // Maximum shared local memory per group
    pDeviceComputeProperties->maxSharedLocalMemory = 0u;

    // Number of subgroup sizes supported
    pDeviceComputeProperties->numSubGroupSizes = 0u;

    memset(pDeviceComputeProperties->subGroupSizes, 0, sizeof(uint32_t) * ZE_SUBGROUPSIZE_COUNT);

    return ZE_RESULT_SUCCESS;
}

ze_result_t Device::getCommandQueueGroupProperties(
    uint32_t *pCount,
    ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    if (pCount == nullptr) {
        LOG_E("Invalid queue group properties count pointer.");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    auto vpuDevice = getVPUDevice();
    if (vpuDevice == nullptr) {
        LOG_E("Failed to get VPUDevice instance.");
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    uint32_t count = safe_cast<uint32_t>(vpuDevice->getNumberOfEngineGroups());
    // Set engine group counts.
    if (*pCount == 0) {
        *pCount = count;
        return ZE_RESULT_SUCCESS;
    }

    if (*pCount > count)
        *pCount = count;

    if (pCommandQueueGroupProperties != nullptr) {
        // Set queue group properties.
        for (uint32_t i = 0; i < *pCount; i++) {
            auto egProp = &(pCommandQueueGroupProperties[i]);

            // Set flags.
            egProp->flags = getCommandQeueueGroupFlags(i);

            // Number of engines in the group.
            egProp->numQueues = 1u;

            // Maximum memory fill patern size.
            egProp->maxMemoryFillPatternSize = vpuDevice->getEngineMaxMemoryFillSize();
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_command_queue_group_property_flags_t Device::getCommandQeueueGroupFlags(uint32_t ordinal) {
    auto type = vpuDevice->getEngineType(ordinal);
    if (type == VPU::EngineType::INVALID) {
        LOG_W("Invalid ordinal");
        return 0;
    }

    ze_command_queue_group_property_flags_t flags = 0;
    if (vpuDevice->engineSupportCompute(type)) {
        flags |= ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE;
    }
    if (vpuDevice->engineSupportCopy(type)) {
        flags |= ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
    }
    if (vpuDevice->engineSupportCooperativeKernel(type)) {
        flags |= ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS;
    }
    if (vpuDevice->engineSupportMetrics(type)) {
        flags |= ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS;
    }

    return flags;
}

ze_result_t Device::getStatus() const {
    if (vpuDevice == nullptr) {
        LOG_W("VPU device instance is invalid.");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return vpuDevice->isConnected() ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_DEVICE_LOST;
}

VPU::VPUDevice *Device::getVPUDevice() {
    return vpuDevice.get();
}

void Device::loadMetricGroupsInfo(std::vector<VPU::GroupInfo> &metricGroupsInfo) {
    size_t numberOfMetricGroups = metricGroupsInfo.size();
    LOG_I("Number of metric groups: %lu", numberOfMetricGroups);

    metricGroups.reserve(metricGroupsInfo.size());

    for (auto const &metricGroupInfo : metricGroupsInfo) {
        zet_metric_group_properties_t groupProperties = {};

        groupProperties.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
        strncpy(groupProperties.name,
                metricGroupInfo.metricGroupName.c_str(),
                ZET_MAX_METRIC_GROUP_NAME - 1);
        groupProperties.name[ZET_MAX_METRIC_GROUP_NAME - 1] = '\0';
        groupProperties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED |
                                       ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED;
        groupProperties.domain = metricGroupInfo.domain;
        groupProperties.metricCount = metricGroupInfo.metricCount;

        std::vector<std::shared_ptr<Metric>> metrics;
        metrics.reserve(groupProperties.metricCount);

        size_t allocationSize = 0u;

        for (auto const &counter : metricGroupInfo.counterInfo) {
            zet_metric_properties_t properties = {};

            properties.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
            strncpy(properties.name, counter.metricName.c_str(), ZET_MAX_METRIC_NAME - 1);
            properties.name[ZET_MAX_METRIC_NAME - 1] = '\0';
            properties.metricType = Metric::getMetricType(counter.metricType);
            properties.resultType = Metric::getValueType(counter.valueType);

            allocationSize += Metric::getMetricValueSize(counter.valueType);

            auto pMetric = std::make_shared<Metric>(properties);
            metrics.push_back(pMetric);
        }

        auto pMetricGroup = std::make_shared<MetricGroup>(groupProperties,
                                                          allocationSize,
                                                          metrics,
                                                          metricGroupInfo.groupIndex,
                                                          numberOfMetricGroups);
        metricGroups.push_back(pMetricGroup);
    }

    metricsLoaded = true;
}

bool Device::isMetricGroupAvailable(MetricGroup *metricGroup) const {
    for (auto &group : metricGroups) {
        if (group.get() == metricGroup) {
            LOG_I("MetricGroup is available on device.");
            return true;
        }
    }

    return false;
}

ze_result_t Device::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {
    if (!getVPUDevice()->getCapMetricStreamer()) {
        LOG_E("Metrics are not supported.");
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (!metricsLoaded) {
        LOG_E("Metrics data not loaded for device (%p)", this);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    if (pCount == nullptr) {
        LOG_E("pCount is NULL.");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (*pCount == 0) {
        *pCount = safe_cast<uint32_t>(metricGroups.size());
        return ZE_RESULT_SUCCESS;
    } else if (*pCount > metricGroups.size()) {
        *pCount = safe_cast<uint32_t>(metricGroups.size());
    }

    if (phMetricGroups != nullptr) {
        for (uint32_t i = 0; i < *pCount; i++) {
            phMetricGroups[i] = metricGroups[i]->toHandle();
        }
    } else {
        LOG_I("Input metric group handle pointer is NULL.");
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t
Device::activateMetricGroups(int vpuFd, uint32_t count, zet_metric_group_handle_t *phMetricGroups) {
    if (metricContext == nullptr) {
        LOG_E("MetricContext not initialized.");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return metricContext->activateMetricGroups(vpuFd, count, phMetricGroups);
}

const std::shared_ptr<MetricContext> Device::getMetricContext() const {
    return metricContext;
}

} // namespace L0
