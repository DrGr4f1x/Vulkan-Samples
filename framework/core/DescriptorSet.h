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

#pragma once

#include "common/helpers.h"
#include "common/vk_common.h"

namespace vkb
{
class Device;
class DescriptorSetLayout;
class DescriptorPool;

/**
 * @brief A descriptor set handle allocated from a \ref DescriptorPool.
 *        Destroying the handle has no effect, as the pool manages the lifecycle of its descriptor sets.
 *
 *        Keeps track of what bindings were written to prevent a double write.
 */
class DescriptorSet
{
  public:
	/**
	 * @brief Constructs a descriptor set from buffer infos and image infos
	 *        Implicitly calls prepare()
	 * @param device A valid Vulkan device
	 * @param descriptor_set_layout The Vulkan descriptor set layout this descriptor set has
	 * @param descriptor_pool The Vulkan descriptor pool the descriptor set is allocated from
	 * @param buffer_infos The descriptors that describe buffer data
	 * @param image_infos The descriptors that describe image data
	 */
	DescriptorSet(Device& device,
	              const DescriptorSetLayout& descriptorSetLayout,
	              DescriptorPool& descriptorPool,
	              const BindingMap<VkDescriptorBufferInfo>& bufferInfos = {},
	              const BindingMap<VkDescriptorImageInfo>& imageInfos  = {});

	DescriptorSet(const DescriptorSet&) = delete;

	DescriptorSet(DescriptorSet&& other);

	// The descriptor set handle is managed by the pool, and will be destroyed when the pool is reset
	~DescriptorSet() = default;

	DescriptorSet& operator=(const DescriptorSet&) = delete;

	DescriptorSet& operator=(DescriptorSet&&) = delete;

	/**
	 * @brief Resets the DescriptorSet state
	 *        Optionally prepares a new set of buffer infos and/or image infos
	 * @param new_buffer_infos A map of buffer descriptors and their respective bindings
	 * @param new_image_infos A map of image descriptors and their respective bindings
	 */
	void Reset(const BindingMap<VkDescriptorBufferInfo>& newBufferInfos = {},
	           const BindingMap<VkDescriptorImageInfo> & newImageInfos  = {});

	/**
	 * @brief Updates the contents of the DescriptorSet by performing the write operations
	 * @param bindings_to_update If empty. we update all bindings. Otherwise, only write the specified bindings if they haven't already been written
	 */
	void Update(const std::vector<uint32_t>& bindingsToUpdate = {});

	/**
	 * @brief Applies pending write operations without updating the state
	 */
	void ApplyWrites() const;

	const DescriptorSetLayout& GetLayout() const;

	VkDescriptorSet GetHandle() const;

	BindingMap<VkDescriptorBufferInfo>& GetBufferInfos();

	BindingMap<VkDescriptorImageInfo>& GetImageInfos();

  protected:
	/**
	 * @brief Prepares the descriptor set to have its contents updated by loading a vector of write operations
	 *        Cannot be called twice during the lifetime of a DescriptorSet
	 */
	void Prepare();

  private:
	Device& m_device;

	const DescriptorSetLayout& m_descriptorSetLayout;

	DescriptorPool& m_descriptorPool;

	BindingMap<VkDescriptorBufferInfo> m_bufferInfos;

	BindingMap<VkDescriptorImageInfo> m_imageInfos;

	VkDescriptorSet m_handle{ VK_NULL_HANDLE };

	// The list of write operations for the descriptor set
	std::vector<VkWriteDescriptorSet> m_writeDescriptorSets;

	// The bindings of the write descriptors that have had vkUpdateDescriptorSets since the last call to update().
	// Each binding number is mapped to a hash of the binding description that it will be updated to.
	std::unordered_map<uint32_t, size_t> m_updatedBindings;
};
}        // namespace vkb
