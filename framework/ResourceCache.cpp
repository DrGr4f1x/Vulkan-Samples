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

#include "ResourceCache.h"

#include "common/resource_caching.h"
#include "core/device.h"

namespace vkb
{

namespace
{

template <class T, class... A>
T& RequestResource(Device& device, ResourceRecord& recorder, std::mutex& resourceMutex, std::unordered_map<std::size_t, T>& resources, A &... args)
{
	std::lock_guard<std::mutex> guard(resourceMutex);

	auto &res = request_resource(device, &recorder, resources, args...);

	return res;
}

}        // namespace


ResourceCache::ResourceCache(Device& device) 
	: m_device{ device }
{
}


void ResourceCache::Warmup(const std::vector<uint8_t>& data)
{
	m_recorder.SetData(data);

	m_replayer.play(*this, m_recorder);
}


std::vector<uint8_t> ResourceCache::Serialize()
{
	return m_recorder.GetData();
}


void ResourceCache::SetPipelineCache(VkPipelineCache newPipelineCache)
{
	m_pipelineCache = newPipelineCache;
}


ShaderModule& ResourceCache::RequestShaderModule(VkShaderStageFlagBits stage, const ShaderSource& glslSource, const ShaderVariant& shaderVariant)
{
	std::string entryPoint{ "main" };
	return RequestResource(m_device, m_recorder, m_shaderModuleMutex, m_state.shader_modules, stage, glslSource, entryPoint, shaderVariant);
}


PipelineLayout& ResourceCache::RequestPipelineLayout(const std::vector<ShaderModule*>& shaderModules)
{
	return RequestResource(m_device, m_recorder, m_pipelineLayoutMutex, m_state.pipeline_layouts, shaderModules);
}


DescriptorSetLayout& ResourceCache::RequestDescriptorSetLayout(const uint32_t setIndex, const std::vector<ShaderModule*>& shaderModules, const std::vector<ShaderResource>& setResources)
{
	return RequestResource(m_device, m_recorder, m_descriptorSetLayoutMutex, m_state.descriptor_set_layouts, setIndex, shaderModules, setResources);
}


GraphicsPipeline& ResourceCache::RequestGraphicsPipeline(PipelineState& pipelineState)
{
	return RequestResource(m_device, m_recorder, m_graphicsPipelineMutex, m_state.graphics_pipelines, m_pipelineCache, pipelineState);
}


ComputePipeline& ResourceCache::RequestComputePipeline(PipelineState& pipelineState)
{
	return RequestResource(m_device, m_recorder, m_computePipelineMutex, m_state.compute_pipelines, m_pipelineCache, pipelineState);
}


DescriptorSet& ResourceCache::RequestDescriptorSet(DescriptorSetLayout& descriptorSetLayout, const BindingMap<VkDescriptorBufferInfo>& bufferInfos, const BindingMap<VkDescriptorImageInfo>& imageInfos)
{
	auto& descriptorPool = RequestResource(m_device, m_recorder, m_descriptorSetMutex, m_state.descriptor_pools, descriptorSetLayout);
	return RequestResource(m_device, m_recorder, m_descriptorSetMutex, m_state.descriptor_sets, descriptorSetLayout, descriptorPool, bufferInfos, imageInfos);
}


RenderPass& ResourceCache::RequestRenderPass(const std::vector<Attachment>& attachments, const std::vector<LoadStoreInfo>& loadStoreInfos, const std::vector<SubpassInfo> &subpasses)
{
	return RequestResource(m_device, m_recorder, m_renderPassMutex, m_state.render_passes, attachments, loadStoreInfos, subpasses);
}


Framebuffer& ResourceCache::RequestFramebuffer(const RenderTarget& renderTarget, const RenderPass& renderPass)
{
	return RequestResource(m_device, m_recorder, m_framebufferMutex, m_state.framebuffers, renderTarget, renderPass);
}


void ResourceCache::ClearPipelines()
{
	m_state.graphics_pipelines.clear();
	m_state.compute_pipelines.clear();
}


void ResourceCache::UpdateDescriptorSets(const std::vector<core::ImageView>& oldViews, const std::vector<core::ImageView>& newViews)
{
	// Find descriptor sets referring to the old image view
	std::vector<VkWriteDescriptorSet> setUpdates;
	std::set<size_t> matches;

	for (size_t i = 0; i < oldViews.size(); ++i)
	{
		auto& oldView = oldViews[i];
		auto& newView = newViews[i];

		for (auto& [key, descriptorSet] : m_state.descriptor_sets)
		{
			auto& imageInfos = descriptorSet.GetImageInfos();

			for (auto& [binding, array] : imageInfos)
			{
				for (auto &[arrayElement, imageInfo] : array)
				{
					if (imageInfo.imageView == oldView.get_handle())
					{
						// Save key to remove old descriptor set
						matches.insert(key);

						// Update image info with new view
						imageInfo.imageView = newView.get_handle();

						// Save struct for writing the update later
						{
							VkWriteDescriptorSet write_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

							if (auto binding_info = descriptorSet.GetLayout().GetLayoutBinding(binding))
							{
								write_descriptor_set.dstBinding      = binding;
								write_descriptor_set.descriptorType  = binding_info->descriptorType;
								write_descriptor_set.pImageInfo      = &imageInfo;
								write_descriptor_set.dstSet          = descriptorSet.GetHandle();
								write_descriptor_set.dstArrayElement = arrayElement;
								write_descriptor_set.descriptorCount = 1;

								setUpdates.push_back(write_descriptor_set);
							}
							else
							{
								LOGE("Shader layout set does not use image binding at #{}", binding);
							}
						}
					}
				}
			}
		}
	}

	if (!setUpdates.empty())
	{
		vkUpdateDescriptorSets(m_device.get_handle(), to_u32(setUpdates.size()), setUpdates.data(), 0, nullptr);
	}

	// Delete old entries (moved out descriptor sets)
	for (auto& match : matches)
	{
		// Move out of the map
		auto it = m_state.descriptor_sets.find(match);
		auto descriptorSet = std::move(it->second);
		m_state.descriptor_sets.erase(match);

		// Generate new key
		size_t newKey = 0U;
		hash_param(newKey, descriptorSet.GetLayout(), descriptorSet.GetBufferInfos(), descriptorSet.GetImageInfos());

		// Add (key, resource) to the cache
		m_state.descriptor_sets.emplace(newKey, std::move(descriptorSet));
	}
}


void ResourceCache::ClearFramebuffers()
{
	m_state.framebuffers.clear();
}


void ResourceCache::Clear()
{
	m_state.shader_modules.clear();
	m_state.pipeline_layouts.clear();
	m_state.descriptor_sets.clear();
	m_state.descriptor_set_layouts.clear();
	m_state.render_passes.clear();
	ClearPipelines();
	ClearFramebuffers();
}


const ResourceCacheState& ResourceCache::GetInternalState() const
{
	return m_state;
}

} // namespace vkb