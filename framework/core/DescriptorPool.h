/* Copyright (c) 2019, Arm Limited and Contributors
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

#pragma once

#include <unordered_map>

#include "common/helpers.h"
#include "common/vk_common.h"

namespace vkb
{
class Device;
class DescriptorSetLayout;

/**
 * @brief Manages an array of fixed size VkDescriptorPool and is able to allocate descriptor sets
 */
class DescriptorPool
{
  public:
	static const uint32_t MAX_SETS_PER_POOL = 16;

	DescriptorPool(Device& device, const DescriptorSetLayout& descriptorSetLayout, uint32_t poolSize = MAX_SETS_PER_POOL);

	DescriptorPool(const DescriptorPool&) = delete;

	DescriptorPool(DescriptorPool&&) = default;

	~DescriptorPool();

	DescriptorPool& operator=(const DescriptorPool&) = delete;

	DescriptorPool& operator=(DescriptorPool&&) = delete;

	void Reset();

	const DescriptorSetLayout& GetDescriptorSetLayout() const;

	void SetDescriptorSetLayout(const DescriptorSetLayout& setLayout);

	VkDescriptorSet AllocateDescriptorSet();

	VkResult FreeDescriptorSet(VkDescriptorSet descriptorSet);

  private:
	Device& m_device;

	const DescriptorSetLayout* m_descriptorSetLayout{ nullptr };

	// Descriptor pool size
	std::vector<VkDescriptorPoolSize> m_poolSizes;

	// Number of sets to allocate for each pool
	uint32_t m_poolMaxSets{ 0 };

	// Total descriptor pools created
	std::vector<VkDescriptorPool> m_pools;

	// Count sets for each pool
	std::vector<uint32_t> m_poolSetsCount;

	// Current pool index to allocate descriptor set
	uint32_t m_poolIndex{ 0 };

	// Map between descriptor set and pool index
	std::unordered_map<VkDescriptorSet, uint32_t> m_setPoolMapping;

	// Find next pool index or create new pool
	uint32_t FindAvailablePool(uint32_t poolIndex);
};
}        // namespace vkb
