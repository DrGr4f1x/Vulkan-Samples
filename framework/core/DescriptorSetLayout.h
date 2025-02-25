/* Copyright (c) 2019-2020, Arm Limited and Contributors
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
class DescriptorPool;
class Device;
class ShaderModule;

struct ShaderResource;

/**
 * @brief Caches DescriptorSet objects for the shader's set index.
 *        Creates a DescriptorPool to allocate the DescriptorSet objects
 */
class DescriptorSetLayout
{
  public:
	/**
	 * @brief Creates a descriptor set layout from a set of resources
	 * @param device A valid Vulkan device
	 * @param set_index The descriptor set index this layout maps to
	 * @param shader_modules The shader modules this set layout will be used for
	 * @param resource_set A grouping of shader resources belonging to the same set
	 */
	DescriptorSetLayout(Device& device,
	                    const uint32_t setIndex,
	                    const std::vector<ShaderModule*>& shaderModules,
	                    const std::vector<ShaderResource>& resourceSet);

	DescriptorSetLayout(const DescriptorSetLayout&) = delete;

	DescriptorSetLayout(DescriptorSetLayout&& other);

	~DescriptorSetLayout();

	DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

	DescriptorSetLayout& operator=(DescriptorSetLayout&&) = delete;

	VkDescriptorSetLayout GetHandle() const;

	const uint32_t GetIndex() const;

	const std::vector<VkDescriptorSetLayoutBinding>& GetBindings() const;

	std::unique_ptr<VkDescriptorSetLayoutBinding> GetLayoutBinding(const uint32_t bindingIndex) const;

	std::unique_ptr<VkDescriptorSetLayoutBinding> GetLayoutBinding(const std::string& name) const;

	const std::vector<VkDescriptorBindingFlagsEXT>& GetBindingFlags() const;

	VkDescriptorBindingFlagsEXT GetLayoutBindingFlag(const uint32_t bindingIndex) const;

	const std::vector<ShaderModule*>& GetShaderModules() const;

  private:
	Device& m_device;

	VkDescriptorSetLayout m_handle{VK_NULL_HANDLE};

	const uint32_t m_setIndex;

	std::vector<VkDescriptorSetLayoutBinding> m_bindings;

	std::vector<VkDescriptorBindingFlagsEXT> m_bindingFlags;

	std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindingsLookup;

	std::unordered_map<uint32_t, VkDescriptorBindingFlagsEXT> m_bindingFlagsLookup;

	std::unordered_map<std::string, uint32_t> m_resourcesLookup;

	std::vector<ShaderModule*> m_shaderModules;
};
}        // namespace vkb
