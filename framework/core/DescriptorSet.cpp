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

#include "DescriptorSet.h"

#include "common/resource_caching.h"
#include "core/util/logging.hpp"
#include "DescriptorPool.h"
#include "DescriptorSetLayout.h"
#include "device.h"

namespace vkb
{
DescriptorSet::DescriptorSet(Device& device,
                             const DescriptorSetLayout& descriptorSetLayout,
                             DescriptorPool& descriptorPool,
                             const BindingMap<VkDescriptorBufferInfo>& bufferInfos,
                             const BindingMap<VkDescriptorImageInfo>& imageInfos)
	: m_device{ device }
	, m_descriptorSetLayout{ descriptorSetLayout }
	, m_descriptorPool{ descriptorPool }
	, m_bufferInfos{ bufferInfos }
	, m_imageInfos{ imageInfos }
	, m_handle{ descriptorPool.AllocateDescriptorSet() }
{
	Prepare();
}


void DescriptorSet::Reset(const BindingMap<VkDescriptorBufferInfo>& newBufferInfos, const BindingMap<VkDescriptorImageInfo>& newImageInfos)
{
	if (!newBufferInfos.empty() || !newImageInfos.empty())
	{
		m_bufferInfos = newBufferInfos;
		m_imageInfos  = newImageInfos;
	}
	else
	{
		LOGW("Calling reset on Descriptor Set with no new buffer infos and no new image infos.");
	}

	m_writeDescriptorSets.clear();
	m_updatedBindings.clear();

	Prepare();
}


void DescriptorSet::Prepare()
{
	// We don't want to prepare twice during the life cycle of a Descriptor Set
	if (!m_writeDescriptorSets.empty())
	{
		LOGW("Trying to prepare a descriptor set that has already been prepared, skipping.");
		return;
	}

	// Iterate over all buffer bindings
	for (auto& bindingIt : m_bufferInfos)
	{
		auto bindingIndex   = bindingIt.first;
		auto& bufferBindings = bindingIt.second;

		if (auto bindingInfo = m_descriptorSetLayout.GetLayoutBinding(bindingIndex))
		{
			// Iterate over all binding buffers in array
			for (auto& elementIt : bufferBindings)
			{
				auto& bufferInfo = elementIt.second;

				const size_t uniformBufferRangeLimit = m_device.get_gpu().get_properties().limits.maxUniformBufferRange;
				const size_t storageBufferRangeLimit = m_device.get_gpu().get_properties().limits.maxStorageBufferRange;

				size_t bufferRangeLimit = static_cast<size_t>(bufferInfo.range);

				if ((bindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || bindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) && bufferRangeLimit > uniformBufferRangeLimit)
				{
					LOGE("Set {} binding {} cannot be updated: buffer size {} exceeds the uniform buffer range limit {}", m_descriptorSetLayout.GetIndex(), bindingIndex, bufferInfo.range, uniformBufferRangeLimit);
					bufferRangeLimit = uniformBufferRangeLimit;
				}
				else if ((bindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || bindingInfo->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) && bufferRangeLimit > storageBufferRangeLimit)
				{
					LOGE("Set {} binding {} cannot be updated: buffer size {} exceeds the storage buffer range limit {}", m_descriptorSetLayout.GetIndex(), bindingIndex, bufferInfo.range, storageBufferRangeLimit);
					bufferRangeLimit = storageBufferRangeLimit;
				}

				// Clip the buffers range to the limit if one exists as otherwise we will receive a Vulkan validation error
				bufferInfo.range = bufferRangeLimit;

				VkWriteDescriptorSet write_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

				write_descriptor_set.dstBinding      = bindingIndex;
				write_descriptor_set.descriptorType  = bindingInfo->descriptorType;
				write_descriptor_set.pBufferInfo     = &bufferInfo;
				write_descriptor_set.dstSet          = m_handle;
				write_descriptor_set.dstArrayElement = elementIt.first;
				write_descriptor_set.descriptorCount = 1;

				m_writeDescriptorSets.push_back(write_descriptor_set);
			}
		}
		else
		{
			LOGE("Shader layout set does not use buffer binding at #{}", bindingIndex);
		}
	}

	// Iterate over all image bindings
	for (auto& bindingIt : m_imageInfos)
	{
		auto bindingIndex     = bindingIt.first;
		auto& bindingResources = bindingIt.second;

		if (auto bindingInfo = m_descriptorSetLayout.GetLayoutBinding(bindingIndex))
		{
			// Iterate over all binding images in array
			for (auto& elementIt : bindingResources)
			{
				auto& imageInfo = elementIt.second;

				VkWriteDescriptorSet write_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

				write_descriptor_set.dstBinding      = bindingIndex;
				write_descriptor_set.descriptorType  = bindingInfo->descriptorType;
				write_descriptor_set.pImageInfo      = &imageInfo;
				write_descriptor_set.dstSet          = m_handle;
				write_descriptor_set.dstArrayElement = elementIt.first;
				write_descriptor_set.descriptorCount = 1;

				m_writeDescriptorSets.push_back(write_descriptor_set);
			}
		}
		else
		{
			LOGE("Shader layout set does not use image binding at #{}", bindingIndex);
		}
	}
}

void DescriptorSet::Update(const std::vector<uint32_t>& bindingsToUpdate)
{
	std::vector<VkWriteDescriptorSet> writeOperations;
	std::vector<size_t> writeOperationHashes;

	// If the 'bindings_to_update' vector is empty, we want to write to all the bindings
	// (but skipping all to-update bindings that haven't been written yet)
	if (bindingsToUpdate.empty())
	{
		for (size_t i = 0; i < m_writeDescriptorSets.size(); i++)
		{
			const auto& writeOperation = m_writeDescriptorSets[i];

			size_t writeOperationHash = 0;
			hash_param(writeOperationHash, writeOperation);

			auto updatePairIt = m_updatedBindings.find(writeOperation.dstBinding);
			if (updatePairIt == m_updatedBindings.end() || updatePairIt->second != writeOperationHash)
			{
				writeOperations.push_back(writeOperation);
				writeOperationHashes.push_back(writeOperationHash);
			}
		}
	}
	else
	{
		// Otherwise we want to update the binding indices present in the 'bindings_to_update' vector.
		// (again, skipping those to update but not updated yet)
		for (size_t i = 0; i < m_writeDescriptorSets.size(); i++)
		{
			const auto& writeOperation = m_writeDescriptorSets[i];

			if (std::find(bindingsToUpdate.begin(), bindingsToUpdate.end(), writeOperation.dstBinding) != bindingsToUpdate.end())
			{
				size_t writeOperationHash = 0;
				hash_param(writeOperationHash, writeOperation);

				auto updatePairIt = m_updatedBindings.find(writeOperation.dstBinding);
				if (updatePairIt == m_updatedBindings.end() || updatePairIt->second != writeOperationHash)
				{
					writeOperations.push_back(writeOperation);
					writeOperationHashes.push_back(writeOperationHash);
				}
			}
		}
	}

	// Perform the Vulkan call to update the DescriptorSet by executing the write operations
	if (!writeOperations.empty())
	{
		vkUpdateDescriptorSets(m_device.get_handle(),
		                       to_u32(writeOperations.size()),
		                       writeOperations.data(),
		                       0,
		                       nullptr);
	}

	// Store the bindings from the write operations that were executed by vkUpdateDescriptorSets (and their hash)
	// to prevent overwriting by future calls to "update()"
	for (size_t i = 0; i < writeOperations.size(); i++)
	{
		m_updatedBindings[writeOperations[i].dstBinding] = writeOperationHashes[i];
	}
}


void DescriptorSet::ApplyWrites() const
{
	vkUpdateDescriptorSets(m_device.get_handle(),
	                       to_u32(m_writeDescriptorSets.size()),
	                       m_writeDescriptorSets.data(),
	                       0,
	                       nullptr);
}

DescriptorSet::DescriptorSet(DescriptorSet&& other) 
	: m_device{ other.m_device }
	, m_descriptorSetLayout{ other.m_descriptorSetLayout }
	, m_descriptorPool{ other.m_descriptorPool }
	, m_bufferInfos{ std::move(other.m_bufferInfos) }
	, m_imageInfos{ std::move(other.m_imageInfos) }
	, m_handle{ other.m_handle }
	, m_writeDescriptorSets{ std::move(other.m_writeDescriptorSets) }
	, m_updatedBindings{ std::move(other.m_updatedBindings) }
{
	other.m_handle = VK_NULL_HANDLE;
}


VkDescriptorSet DescriptorSet::GetHandle() const
{
	return m_handle;
}


const DescriptorSetLayout &DescriptorSet::GetLayout() const
{
	return m_descriptorSetLayout;
}


BindingMap<VkDescriptorBufferInfo>& DescriptorSet::GetBufferInfos()
{
	return m_bufferInfos;
}


BindingMap<VkDescriptorImageInfo>& DescriptorSet::GetImageInfos()
{
	return m_imageInfos;
}

}        // namespace vkb
