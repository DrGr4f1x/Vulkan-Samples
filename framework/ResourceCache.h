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

#include <string>
#include <unordered_map>
#include <vector>

#include "common/helpers.h"
#include "core/DescriptorPool.h"
#include "core/DescriptorSet.h"
#include "core/DescriptorSetLayout.h"
#include "core/framebuffer.h"
#include "core/pipeline.h"
#include "ResourceRecord.h"
#include "resource_replay.h"

namespace vkb
{
class Device;

namespace core
{
class ImageView;
}

/**
 * @brief Struct to hold the internal state of the Resource Cache
 *
 */
struct ResourceCacheState
{
	std::unordered_map<std::size_t, ShaderModule> shader_modules;

	std::unordered_map<std::size_t, PipelineLayout> pipeline_layouts;

	std::unordered_map<std::size_t, DescriptorSetLayout> descriptor_set_layouts;

	std::unordered_map<std::size_t, DescriptorPool> descriptor_pools;

	std::unordered_map<std::size_t, RenderPass> render_passes;

	std::unordered_map<std::size_t, GraphicsPipeline> graphics_pipelines;

	std::unordered_map<std::size_t, ComputePipeline> compute_pipelines;

	std::unordered_map<std::size_t, DescriptorSet> descriptor_sets;

	std::unordered_map<std::size_t, Framebuffer> framebuffers;
};

/**
 * @brief Cache all sorts of Vulkan objects specific to a Vulkan device.
 * Supports serialization and deserialization of cached resources.
 * There is only one cache for all these objects, with several unordered_map of hash indices
 * and objects. For every object requested, there is a templated version on request_resource.
 * Some objects may need building if they are not found in the cache.
 *
 * The resource cache is also linked with ResourceRecord and ResourceReplay. Replay can warm-up
 * the cache on app startup by creating all necessary objects.
 * The cache holds pointers to objects and has a mapping from such pointers to hashes.
 * It can only be destroyed in bulk, single elements cannot be removed.
 */
class ResourceCache
{
  public:
	ResourceCache(Device& device);

	ResourceCache(const ResourceCache&) = delete;

	ResourceCache(ResourceCache&&) = delete;

	ResourceCache& operator=(const ResourceCache&) = delete;

	ResourceCache& operator=(ResourceCache&&) = delete;

	void Warmup(const std::vector<uint8_t>& data);

	std::vector<uint8_t> Serialize();

	void SetPipelineCache(VkPipelineCache pipelineCache);

	ShaderModule& RequestShaderModule(VkShaderStageFlagBits stage, const ShaderSource& glslSource, const ShaderVariant& shaderVariant = {});

	PipelineLayout& RequestPipelineLayout(const std::vector<ShaderModule*>& shaderModules);

	DescriptorSetLayout& RequestDescriptorSetLayout(const uint32_t setIndex, const std::vector<ShaderModule*>& shaderModules, const std::vector<ShaderResource>& setResources);

	GraphicsPipeline& RequestGraphicsPipeline(PipelineState& pipelineState);

	ComputePipeline& RequestComputePipeline(PipelineState& pipelineState);

	DescriptorSet& RequestDescriptorSet(DescriptorSetLayout& descriptorSetLayout, const BindingMap<VkDescriptorBufferInfo>& bufferInfos, const BindingMap<VkDescriptorImageInfo>& imageInfos);

	RenderPass& RequestRenderPass(const std::vector<Attachment>& attachments, const std::vector<LoadStoreInfo>& loadStoreInfos, const std::vector<SubpassInfo>& subpasses);

	Framebuffer& RequestFramebuffer(const RenderTarget& renderTarget, const RenderPass& renderPass);

	void ClearPipelines();

	/// @brief Update those descriptor sets referring to old views
	/// @param old_views Old image views referred by descriptor sets
	/// @param new_views New image views to be referred
	void UpdateDescriptorSets(const std::vector<core::ImageView>& oldViews, const std::vector<core::ImageView>& newViews);

	void ClearFramebuffers();

	void Clear();

	const ResourceCacheState& GetInternalState() const;

  private:
	Device& m_device;

	ResourceRecord m_recorder;

	ResourceReplay m_replayer;

	VkPipelineCache m_pipelineCache{ VK_NULL_HANDLE };

	ResourceCacheState m_state;

	std::mutex m_descriptorSetMutex;

	std::mutex m_pipelineLayoutMutex;

	std::mutex m_shaderModuleMutex;

	std::mutex m_descriptorSetLayoutMutex;

	std::mutex m_graphicsPipelineMutex;

	std::mutex m_renderPassMutex;

	std::mutex m_computePipelineMutex;

	std::mutex m_framebufferMutex;
};
}        // namespace vkb
