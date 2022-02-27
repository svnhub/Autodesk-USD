//
// Copyright 2020 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/imaging/hgiVulkan/accelerationStructure.h"
#include "pxr/imaging/hgiVulkan/conversions.h"
#include "pxr/imaging/hgiVulkan/buffer.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgiVulkan/device.h"

PXR_NAMESPACE_OPEN_SCOPE


VkDeviceOrHostAddressConstKHR GetBufferAddress(HgiBufferHandle buffer) {
    VkDeviceOrHostAddressConstKHR res;
    if (!buffer)
    {
        res.deviceAddress = (VkDeviceAddress)0;
        return res;
    };
    HgiVulkanBuffer* pBufferVk = (HgiVulkanBuffer*)buffer.Get();

    return pBufferVk->GetConstDeviceAddress();
}

HgiVulkanAccelerationStructureGeometry::HgiVulkanAccelerationStructureGeometry(
    HgiVulkanDevice* device,
    HgiAccelerationStructureTriangleGeometryDesc const& desc) : HgiAccelerationStructureGeometry(desc) {
    _accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    _accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    _accelerationStructureGeometry.pNext = nullptr;
    _accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    _accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    _accelerationStructureGeometry.geometry.triangles.vertexFormat = HgiVulkanConversions::GetFormat(desc.vertexFormat);;
    _accelerationStructureGeometry.geometry.triangles.vertexData = GetBufferAddress(desc.vertexData);
    _accelerationStructureGeometry.geometry.triangles.maxVertex = desc.maxVertex;
    _accelerationStructureGeometry.geometry.triangles.vertexStride = desc.vertexStride;
    _accelerationStructureGeometry.geometry.triangles.indexType = desc.indexType== HgiIndexTypeUInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32; //TODO respect format
    _accelerationStructureGeometry.geometry.triangles.indexData = GetBufferAddress(desc.indexData);;
    _accelerationStructureGeometry.geometry.triangles.transformData = GetBufferAddress(desc.transformData);

}


uint64_t
HgiVulkanAccelerationStructure::GetRawResource() const
{
    return (uint64_t)_accelerationStructure;
}


HgiVulkanAccelerationStructure::HgiVulkanAccelerationStructure(
    HgiVulkanDevice* device,
    HgiAccelerationStructureDesc const& desc): HgiAccelerationStructure(desc) {

}


PXR_NAMESPACE_CLOSE_SCOPE
