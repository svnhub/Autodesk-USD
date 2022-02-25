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
#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiVulkan/buffer.h"
#include "pxr/imaging/hgiVulkan/commandBuffer.h"
#include "pxr/imaging/hgiVulkan/commandQueue.h"
#include "pxr/imaging/hgiVulkan/conversions.h"
#include "pxr/imaging/hgiVulkan/device.h"
#include "pxr/imaging/hgiVulkan/diagnostic.h"
#include "pxr/imaging/hgiVulkan/garbageCollector.h"
#include "pxr/imaging/hgiVulkan/hgi.h"
#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "Fatal : VkResult is \"" << std::to_string(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}


uint32_t getMemoryType(uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& memoryProperties, VkMemoryPropertyFlags properties, VkBool32* memTypeFound=nullptr)
{
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((typeBits & 1) == 1)
        {
            if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                if (memTypeFound)
                {
                    *memTypeFound = true;
                }
                return i;
            }
        }
        typeBits >>= 1;
    }

    if (memTypeFound)
    {
        *memTypeFound = false;
        return 0;
    }
    else
    {
        throw std::runtime_error("Could not find a matching memory type");
    }
}


void HgiVulkanBuffer::allocateDirect(const VkBufferCreateInfo &bufferCreateInfo, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, const void* data)
{

    VkDevice device = _device->GetVulkanDevice();

    VkMemoryAllocateInfo memAlloc{};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &_vkBuffer));

    // Create the memory backing up the buffer handle
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, _vkBuffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    // Find a memory type index that fits the properties of the buffer
    memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, _device->GetDeviceCapabilities().vkMemoryProperties, memoryPropertyFlags);
    // If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
    VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
    if (bufferCreateInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        memAlloc.pNext = &allocFlagsInfo;
    }
    VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &_vkDeviceMemory));
#if 1
    // If a pointer to the buffer data has been passed, map the buffer and copy over the data
    if (data != nullptr)
    {
        void* mapped;
        VK_CHECK_RESULT(vkMapMemory(device, _vkDeviceMemory, 0, size, 0, &mapped));
        memcpy(mapped, data, size);
        // If host coherency hasn't been requested, do a manual flush to make writes visible
        if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            VkMappedMemoryRange mappedRange{};
            mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            mappedRange.memory = _vkDeviceMemory;
            mappedRange.offset = 0;
            mappedRange.size = size;

            vkFlushMappedMemoryRanges(device, 1, &mappedRange);
        }
        vkUnmapMemory(device, _vkDeviceMemory);
    }
#endif

    VK_CHECK_RESULT(vkBindBufferMemory(device, _vkBuffer, _vkDeviceMemory, 0));

}


HgiVulkanBuffer::HgiVulkanBuffer(
    HgiVulkan* hgi,
    HgiVulkanDevice* device,
    HgiBufferDesc const& desc)
    : HgiBuffer(desc)
    , _device(device)
    , _vkBuffer(nullptr)
    , _vmaAllocation(nullptr)
    , _inflightBits(0)
    , _stagingBuffer(nullptr)
    , _cpuStagingAddress(nullptr)
{
    if (desc.byteSize == 0) {
        TF_CODING_ERROR("The size of buffer [%p] is zero.", this);
        return;
    }


    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = desc.byteSize;
    bi.usage = HgiVulkanConversions::GetBufferUsage(desc.usage);
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // gfx queue only

    bool cannotUseVMA = (desc.usage & HgiBufferUsageRayTracingExtensions);
    if (cannotUseVMA) {
        uint32_t memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        allocateDirect(bi, memoryProperties, desc.byteSize, desc.initialData);
    }
    else
    {
        VmaAllocator vma = device->GetVulkanMemoryAllocator();
        // Create buffer with memory allocated and bound.
        // Equivalent to: vkCreateBuffer, vkAllocateMemory, vkBindBufferMemory
        // XXX On VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU it may be beneficial to
        // skip staging buffers and use DEVICE_LOCAL | HOST_VISIBLE_BIT since all
        // memory is shared between CPU and GPU.
        VmaAllocationCreateInfo ai = {};
        ai.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; // GPU efficient

        VkResult createRes = vmaCreateBuffer(vma, &bi, &ai, &_vkBuffer, &_vmaAllocation, 0);
        std::cout << "Create res:" << createRes << std::endl;
        TF_VERIFY(
            createRes == VK_SUCCESS
        );

        if (desc.initialData) {
            // Use a 'staging buffer' to schedule uploading the 'initialData' to
            // the device-local GPU buffer.
            HgiVulkanBuffer* stagingBuffer = CreateStagingBuffer(_device, desc);
            VkBuffer vkStagingBuf = stagingBuffer->GetVulkanBuffer();

            HgiVulkanCommandQueue* queue = device->GetCommandQueue();
            HgiVulkanCommandBuffer* cb = queue->AcquireResourceCommandBuffer();
            VkCommandBuffer vkCmdBuf = cb->GetVulkanCommandBuffer();

            // Copy data from staging buffer to device-local buffer.
            VkBufferCopy copyRegion = {};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = desc.byteSize;
            vkCmdCopyBuffer(vkCmdBuf, vkStagingBuf, _vkBuffer, 1, &copyRegion);

            // We don't know if this buffer is a static (immutable) or
            // dynamic (animated) buffer. We assume that most buffers are
            // static and schedule garbage collection of staging resource.
            HgiBufferHandle stagingHandle(stagingBuffer, 0);
            hgi->TrashObject(
                &stagingHandle,
                hgi->GetGarbageCollector()->GetBufferList());
        }
    }

    // Debug label
    if (!_descriptor.debugName.empty()) {
        std::string debugLabel = "Buffer " + _descriptor.debugName;
        HgiVulkanSetDebugName(
            device,
            (uint64_t)_vkBuffer,
            VK_OBJECT_TYPE_BUFFER,
            debugLabel.c_str());
    }

    _descriptor.initialData = nullptr;
}

HgiVulkanBuffer::HgiVulkanBuffer(
    HgiVulkanDevice* device,
    VkBuffer vkBuffer,
    VmaAllocation vmaAllocation,
    HgiBufferDesc const& desc)
    : HgiBuffer(desc)
    , _device(device)
    , _vkBuffer(vkBuffer)
    , _vmaAllocation(vmaAllocation)
    , _inflightBits(0)
    , _stagingBuffer(nullptr)
    , _cpuStagingAddress(nullptr)
{
}

HgiVulkanBuffer::~HgiVulkanBuffer()
{
    if (_cpuStagingAddress && _stagingBuffer) {
        vmaUnmapMemory(
            _device->GetVulkanMemoryAllocator(),
            _stagingBuffer->GetVulkanMemoryAllocation());
        _cpuStagingAddress = nullptr;
    }

    delete _stagingBuffer;
    _stagingBuffer = nullptr;

    vmaDestroyBuffer(
        _device->GetVulkanMemoryAllocator(),
        _vkBuffer,
        _vmaAllocation);
}

size_t
HgiVulkanBuffer::GetByteSizeOfResource() const
{
    return _descriptor.byteSize;
}

uint64_t
HgiVulkanBuffer::GetRawResource() const
{
    return (uint64_t) _vkBuffer;
}

void*
HgiVulkanBuffer::GetCPUStagingAddress()
{
    if (!_stagingBuffer) {
        HgiBufferDesc desc = _descriptor;
        desc.initialData = nullptr;
        _stagingBuffer = CreateStagingBuffer(_device, desc);
    }

    if (!_cpuStagingAddress) {
        TF_VERIFY(
            vmaMapMemory(
                _device->GetVulkanMemoryAllocator(), 
                _stagingBuffer->GetVulkanMemoryAllocation(), 
                &_cpuStagingAddress) == VK_SUCCESS
        );
    }

    // This lets the client code memcpy into the staging buffer directly.
    // The staging data must be explicitely copied to the device-local
    // GPU buffer via CopyBufferCpuToGpu cmd by the client.
    return _cpuStagingAddress;
}

bool
HgiVulkanBuffer::IsCPUStagingAddress(const void* address) const
{
    return (address == _cpuStagingAddress);
}

VkBuffer
HgiVulkanBuffer::GetVulkanBuffer() const
{
    return _vkBuffer;
}

VmaAllocation
HgiVulkanBuffer::GetVulkanMemoryAllocation() const
{
    return _vmaAllocation;
}

HgiVulkanBuffer*
HgiVulkanBuffer::GetStagingBuffer() const
{
    return _stagingBuffer;
}

HgiVulkanDevice*
HgiVulkanBuffer::GetDevice() const
{
    return _device;
}

uint64_t &
HgiVulkanBuffer::GetInflightBits()
{
    return _inflightBits;
}

HgiVulkanDeviceAddress HgiVulkanBuffer::GetDeviceAddress()
{
    HgiVulkanDeviceAddress addr = {};
    VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
    bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAI.buffer = _vkBuffer;
    addr.deviceAddress = _device->vkGetBufferDeviceAddressKHR(_device->GetVulkanDevice(), &bufferDeviceAI);

    return addr;
}

HgiVulkanConstDeviceAddress HgiVulkanBuffer::GetConstDeviceAddress()
{
    HgiVulkanConstDeviceAddress addr = {};
    VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
    bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAI.buffer = _vkBuffer;
    addr.deviceAddress = _device->vkGetBufferDeviceAddressKHR(_device->GetVulkanDevice(), &bufferDeviceAI);

    return addr;
}


HgiVulkanBuffer* HgiVulkanBuffer::CreateStagingBuffer(
    HgiVulkanDevice* device,
    HgiBufferDesc const& desc)
{
    VmaAllocator vma = device->GetVulkanMemoryAllocator();

    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = desc.byteSize;
    bi.usage = HgiVulkanConversions::GetBufferUsage(desc.usage);
    bi.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // gfx queue only

    VmaAllocationCreateInfo ai = {};
    ai.requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | // CPU access (mem map)
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // Dont have to manually flush

    VkBuffer buffer = 0;
    VmaAllocation alloc = 0;
    TF_VERIFY(
        vmaCreateBuffer(vma, &bi, &ai, &buffer, &alloc, 0) == VK_SUCCESS
    );

    // Map the (HOST_VISIBLE) buffer and upload data
    if (desc.initialData) {
        void* map;
        TF_VERIFY(vmaMapMemory(vma, alloc, &map) == VK_SUCCESS);
        memcpy(map, desc.initialData, desc.byteSize);
        vmaUnmapMemory(vma, alloc);
    }

    // Return new staging buffer (caller manages lifetime)
    return new HgiVulkanBuffer(device, buffer, alloc, desc);
}

PXR_NAMESPACE_CLOSE_SCOPE
