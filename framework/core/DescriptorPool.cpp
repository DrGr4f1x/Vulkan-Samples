/* Copyright (c) 2019-2022, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DescriptorPool.h"

#include "DescriptorSetLayout.h"
#include "device.h"

namespace vkb
{
DescriptorPool::DescriptorPool(Device& device, const DescriptorSetLayout& descriptorSetLayout, uint32_t poolSize) 
	: m_device{ device }
	, m_descriptorSetLayout{ &descriptorSetLayout}
{
	const auto& bindings = descriptorSetLayout.GetBindings();

	std::map<VkDescriptorType, std::uint32_t> descriptorTypeCounts;

	// Count each type of descriptor set
	for (auto& binding : bindings)
	{
		descriptorTypeCounts[binding.descriptorType] += binding.descriptorCount;
	}

	// Allocate pool sizes array
	m_poolSizes.resize(descriptorTypeCounts.size());

	auto poolSizeIt = m_poolSizes.begin();

	// Fill pool size for each descriptor type count multiplied by the pool size
	for (auto& it : descriptorTypeCounts)
	{
		poolSizeIt->type = it.first;

		poolSizeIt->descriptorCount = it.second * poolSize;

		++poolSizeIt;
	}

	m_poolMaxSets = poolSize;
}


DescriptorPool::~DescriptorPool()
{
	// Destroy all descriptor pools
	for (auto pool : m_pools)
	{
		vkDestroyDescriptorPool(m_device.get_handle(), pool, nullptr);
	}
}


void DescriptorPool::Reset()
{
	// Reset all descriptor pools
	for (auto pool : m_pools)
	{
		vkResetDescriptorPool(m_device.get_handle(), pool, 0);
	}

	// Clear internal tracking of descriptor set allocations
	std::fill(m_poolSetsCount.begin(), m_poolSetsCount.end(), 0);
	m_setPoolMapping.clear();

	// Reset the pool index from which descriptor sets are allocated
	m_poolIndex = 0;
}

const DescriptorSetLayout& DescriptorPool::GetDescriptorSetLayout() const
{
	assert(m_descriptorSetLayout && "Descriptor set layout is invalid");
	return *m_descriptorSetLayout;
}


void DescriptorPool::SetDescriptorSetLayout(const DescriptorSetLayout& setLayout)
{
	m_descriptorSetLayout = &setLayout;
}

VkDescriptorSet DescriptorPool::AllocateDescriptorSet()
{
	m_poolIndex = FindAvailablePool(m_poolIndex);

	// Increment allocated set count for the current pool
	++m_poolSetsCount[m_poolIndex];

	VkDescriptorSetLayout vkSetLayout = GetDescriptorSetLayout().GetHandle();

	VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	alloc_info.descriptorPool     = m_pools[m_poolIndex];
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts        = &vkSetLayout;

	VkDescriptorSet handle = VK_NULL_HANDLE;

	// Allocate a new descriptor set from the current pool
	auto result = vkAllocateDescriptorSets(m_device.get_handle(), &alloc_info, &handle);

	if (result != VK_SUCCESS)
	{
		// Decrement allocated set count for the current pool
		--m_poolSetsCount[m_poolIndex];

		return VK_NULL_HANDLE;
	}

	// Store mapping between the descriptor set and the pool
	m_setPoolMapping.emplace(handle, m_poolIndex);

	return handle;
}


VkResult DescriptorPool::FreeDescriptorSet(VkDescriptorSet descriptorSet)
{
	// Get the pool index of the descriptor set
	auto it = m_setPoolMapping.find(descriptorSet);

	if (it == m_setPoolMapping.end())
	{
		return VK_INCOMPLETE;
	}

	auto descPoolIndex = it->second;

	// Free descriptor set from the pool
	vkFreeDescriptorSets(m_device.get_handle(), m_pools[descPoolIndex], 1, &descriptorSet);

	// Remove descriptor set mapping to the pool
	m_setPoolMapping.erase(it);

	// Decrement allocated set count for the pool
	--m_poolSetsCount[descPoolIndex];

	// Change the current pool index to use the available pool
	m_poolIndex = descPoolIndex;

	return VK_SUCCESS;
}


std::uint32_t DescriptorPool::FindAvailablePool(std::uint32_t searchIndex)
{
	// Create a new pool
	if (m_pools.size() <= searchIndex)
	{
		VkDescriptorPoolCreateInfo create_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};

		create_info.poolSizeCount = to_u32(m_poolSizes.size());
		create_info.pPoolSizes    = m_poolSizes.data();
		create_info.maxSets       = m_poolMaxSets;

		// We do not set FREE_DESCRIPTOR_SET_BIT as we do not need to free individual descriptor sets
		create_info.flags = 0;

		// Check descriptor set layout and enable the required flags
		auto& bindingFlags = m_descriptorSetLayout->GetBindingFlags();
		for (auto bindingFlag : bindingFlags)
		{
			if (bindingFlag & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT)
			{
				create_info.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
			}
		}

		VkDescriptorPool handle = VK_NULL_HANDLE;

		// Create the Vulkan descriptor pool
		auto result = vkCreateDescriptorPool(m_device.get_handle(), &create_info, nullptr, &handle);

		if (result != VK_SUCCESS)
		{
			return 0;
		}

		// Store internally the Vulkan handle
		m_pools.push_back(handle);

		// Add set count for the descriptor pool
		m_poolSetsCount.push_back(0);

		return searchIndex;
	}
	else if (m_poolSetsCount[searchIndex] < m_poolMaxSets)
	{
		return searchIndex;
	}

	// Increment pool index
	return FindAvailablePool(++searchIndex);
}
}        // namespace vkb
