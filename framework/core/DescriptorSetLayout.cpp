/* Copyright (c) 2019-2024, Arm Limited and Contributors
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

#include "DescriptorSetLayout.h"

#include "device.h"
#include "physical_device.h"
#include "shader_module.h"

namespace vkb
{

namespace
{

inline VkDescriptorType FindDescriptorType(ShaderResourceType resourceType, bool isDynamic)
{
	switch (resourceType)
	{
		case ShaderResourceType::InputAttachment:
			return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			break;
		case ShaderResourceType::Image:
			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			break;
		case ShaderResourceType::ImageSampler:
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			break;
		case ShaderResourceType::ImageStorage:
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			break;
		case ShaderResourceType::Sampler:
			return VK_DESCRIPTOR_TYPE_SAMPLER;
			break;
		case ShaderResourceType::BufferUniform:
			if (isDynamic)
			{
				return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			}
			else
			{
				return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			}
			break;
		case ShaderResourceType::BufferStorage:
			if (isDynamic)
			{
				return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
			}
			else
			{
				return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			}
			break;
		default:
			throw std::runtime_error("No conversion possible for the shader resource type.");
			break;
	}
} 


inline bool ValidateBinding(const VkDescriptorSetLayoutBinding& binding, const std::vector<VkDescriptorType>& blacklist)
{
	return !(std::find_if(blacklist.begin(), blacklist.end(), [binding](const VkDescriptorType& type) { return type == binding.descriptorType; }) != blacklist.end());
}


inline bool ValidateFlags(const PhysicalDevice& gpu, const std::vector<VkDescriptorSetLayoutBinding>& bindings, const std::vector<VkDescriptorBindingFlagsEXT>& flags)
{
	// Assume bindings are valid if there are no flags
	if (flags.empty())
	{
		return true;
	}

	// Binding count has to equal flag count as its a 1:1 mapping
	if (bindings.size() != flags.size())
	{
		LOGE("Binding count has to be equal to flag count.");
		return false;
	}

	return true;
}

} // anonymous namespace


DescriptorSetLayout::DescriptorSetLayout(Device& device, const uint32_t setIndex, const std::vector<ShaderModule*>& shaderModules, const std::vector<ShaderResource>& resourceSet) 
	: m_device{ device }
	, m_setIndex{ setIndex }
	, m_shaderModules{ shaderModules }
{
	// NOTE: `shader_modules` is passed in mainly for hashing their handles in `request_resource`.
	//        This way, different pipelines (with different shaders / shader variants) will get
	//        different descriptor set layouts (incl. appropriate name -> binding lookups)

	for (auto& resource : resourceSet)
	{
		// Skip shader resources whitout a binding point
		if (resource.type == ShaderResourceType::Input ||
		    resource.type == ShaderResourceType::Output ||
		    resource.type == ShaderResourceType::PushConstant ||
		    resource.type == ShaderResourceType::SpecializationConstant)
		{
			continue;
		}

		// Convert from ShaderResourceType to VkDescriptorType.
		auto descriptor_type = FindDescriptorType(resource.type, resource.mode == ShaderResourceMode::Dynamic);

		if (resource.mode == ShaderResourceMode::UpdateAfterBind)
		{
			m_bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);
		}
		else
		{
			// When creating a descriptor set layout, if we give a structure to create_info.pNext, each binding needs to have a binding flag
			// (pBindings[i] uses the flags in pBindingFlags[i])
			// Adding 0 ensures the bindings that don't use any flags are mapped correctly.
			m_bindingFlags.push_back(0);
		}

		// Convert ShaderResource to VkDescriptorSetLayoutBinding
		VkDescriptorSetLayoutBinding layout_binding{};

		layout_binding.binding         = resource.binding;
		layout_binding.descriptorCount = resource.array_size;
		layout_binding.descriptorType  = descriptor_type;
		layout_binding.stageFlags      = static_cast<VkShaderStageFlags>(resource.stages);

		m_bindings.push_back(layout_binding);

		// Store mapping between binding and the binding point
		m_bindingsLookup.emplace(resource.binding, layout_binding);

		m_bindingFlagsLookup.emplace(resource.binding, m_bindingFlags.back());

		m_resourcesLookup.emplace(resource.name, resource.binding);
	}

	VkDescriptorSetLayoutCreateInfo create_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	create_info.flags        = 0;
	create_info.bindingCount = to_u32(m_bindings.size());
	create_info.pBindings    = m_bindings.data();

	// Handle update-after-bind extensions
	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT binding_flags_create_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
	if (std::find_if(resourceSet.begin(), resourceSet.end(),
	                 [](const ShaderResource& shaderResource) { return shaderResource.mode == ShaderResourceMode::UpdateAfterBind; }) != resourceSet.end())
	{
		// Spec states you can't have ANY dynamic resources if you have one of the bindings set to update-after-bind
		if (std::find_if(resourceSet.begin(), resourceSet.end(),
		                 [](const ShaderResource& shaderResource) { return shaderResource.mode == ShaderResourceMode::Dynamic; }) != resourceSet.end())
		{
			throw std::runtime_error("Cannot create descriptor set layout, dynamic resources are not allowed if at least one resource is update-after-bind.");
		}

		if (!ValidateFlags(device.get_gpu(), m_bindings, m_bindingFlags))
		{
			throw std::runtime_error("Invalid binding, couldn't create descriptor set layout.");
		}

		binding_flags_create_info.bindingCount  = to_u32(m_bindingFlags.size());
		binding_flags_create_info.pBindingFlags = m_bindingFlags.data();

		create_info.pNext = &binding_flags_create_info;
		create_info.flags |= std::find(m_bindingFlags.begin(), m_bindingFlags.end(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT) != m_bindingFlags.end() ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT : 0;
	}

	// Create the Vulkan descriptor set layout handle
	VkResult result = vkCreateDescriptorSetLayout(device.get_handle(), &create_info, nullptr, &m_handle);

	if (result != VK_SUCCESS)
	{
		throw VulkanException{result, "Cannot create DescriptorSetLayout"};
	}
}


DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other) 
	: m_device{ other.m_device }
	, m_shaderModules{ other.m_shaderModules }
	, m_handle{ other.m_handle }
	, m_setIndex{ other.m_setIndex}
	, m_bindings{ std::move(other.m_bindings) }
	, m_bindingFlags{ std::move(other.m_bindingFlags) }
	, m_bindingsLookup{ std::move(other.m_bindingsLookup) }
	, m_bindingFlagsLookup{ std::move(other.m_bindingFlagsLookup) }
	, m_resourcesLookup{ std::move(other.m_resourcesLookup) }
{
	other.m_handle = VK_NULL_HANDLE;
}


DescriptorSetLayout::~DescriptorSetLayout()
{
	// Destroy descriptor set layout
	if (m_handle != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(m_device.get_handle(), m_handle, nullptr);
	}
}

VkDescriptorSetLayout DescriptorSetLayout::GetHandle() const
{
	return m_handle;
}


const uint32_t DescriptorSetLayout::GetIndex() const
{
	return m_setIndex;
}


const std::vector<VkDescriptorSetLayoutBinding> &DescriptorSetLayout::GetBindings() const
{
	return m_bindings;
}


const std::vector<VkDescriptorBindingFlagsEXT> &DescriptorSetLayout::GetBindingFlags() const
{
	return m_bindingFlags;
}


std::unique_ptr<VkDescriptorSetLayoutBinding> DescriptorSetLayout::GetLayoutBinding(uint32_t bindingIndex) const
{
	auto it = m_bindingsLookup.find(bindingIndex);

	if (it == m_bindingsLookup.end())
	{
		return nullptr;
	}

	return std::make_unique<VkDescriptorSetLayoutBinding>(it->second);
}


std::unique_ptr<VkDescriptorSetLayoutBinding> DescriptorSetLayout::GetLayoutBinding(const std::string& name) const
{
	auto it = m_resourcesLookup.find(name);

	if (it == m_resourcesLookup.end())
	{
		return nullptr;
	}

	return GetLayoutBinding(it->second);
}


VkDescriptorBindingFlagsEXT DescriptorSetLayout::GetLayoutBindingFlag(const uint32_t bindingIndex) const
{
	auto it = m_bindingFlagsLookup.find(bindingIndex);

	if (it == m_bindingFlagsLookup.end())
	{
		return 0;
	}

	return it->second;
}


const std::vector<ShaderModule*>& DescriptorSetLayout::GetShaderModules() const
{
	return m_shaderModules;
}

}        // namespace vkb
