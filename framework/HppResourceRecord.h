/* Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
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

#include "ResourceRecord.h"
#include <core/hpp_pipeline.h>

namespace vkb
{
namespace common
{
struct HPPLoadStoreInfo;
}

namespace rendering
{
struct HPPAttachment;
}

namespace core
{
class HPPPipelineLayout;
class HPPRenderPass;
class HPPShaderModule;
class HPPShaderSource;
class HPPShaderVariant;
struct HPPSubpassInfo;
}        // namespace core

/**
 * @brief facade class around vkb::ResourceRecord, providing a vulkan.hpp-based interface
 *
 * See vkb::ResourceRecord for documentation
 */
class HPPResourceRecord : private vkb::ResourceRecord
{
  public:
	using vkb::ResourceRecord::GetData;
	using vkb::ResourceRecord::SetData;

	size_t RegisterGraphicsPipeline(vk::PipelineCache pipeline_cache, vkb::rendering::HPPPipelineState &pipeline_state)
	{
		return vkb::ResourceRecord::RegisterGraphicsPipeline(static_cast<VkPipelineCache>(pipeline_cache),
		                                                       reinterpret_cast<vkb::PipelineState &>(pipeline_state));
	}

	size_t RegisterPipelineLayout(const std::vector<vkb::core::HPPShaderModule *> &shader_modules)
	{
		return vkb::ResourceRecord::RegisterPipelineLayout(reinterpret_cast<std::vector<vkb::ShaderModule *> const &>(shader_modules));
	}

	size_t RegisterRenderPass(const std::vector<vkb::rendering::HPPAttachment> &attachments,
	                            const std::vector<vkb::common::HPPLoadStoreInfo> &load_store_infos,
	                            const std::vector<vkb::core::HPPSubpassInfo>     &subpasses)
	{
		return vkb::ResourceRecord::RegisterRenderPass(reinterpret_cast<std::vector<vkb::Attachment> const &>(attachments),
		                                                 reinterpret_cast<std::vector<vkb::LoadStoreInfo> const &>(load_store_infos),
		                                                 reinterpret_cast<std::vector<vkb::SubpassInfo> const &>(subpasses));
	}

	size_t RegisterShaderModule(vk::ShaderStageFlagBits            stage,
	                              const vkb::core::HPPShaderSource  &glsl_source,
	                              const std::string                 &entry_point,
	                              const vkb::core::HPPShaderVariant &shader_variant)
	{
		return vkb::ResourceRecord::RegisterShaderModule(static_cast<VkShaderStageFlagBits>(stage),
		                                                   reinterpret_cast<vkb::ShaderSource const &>(glsl_source),
		                                                   entry_point,
		                                                   reinterpret_cast<vkb::ShaderVariant const &>(shader_variant));
	}

	void SetGraphicsPipeline(size_t index, const vkb::core::HPPGraphicsPipeline &graphics_pipeline)
	{
		vkb::ResourceRecord::SetGraphicsPipeline(index, reinterpret_cast<vkb::GraphicsPipeline const &>(graphics_pipeline));
	}

	void SetPipelineLayout(size_t index, const vkb::core::HPPPipelineLayout &pipeline_layout)
	{
		vkb::ResourceRecord::SetPipelineLayout(index, reinterpret_cast<vkb::PipelineLayout const &>(pipeline_layout));
	}

	void SetRenderPass(size_t index, const vkb::core::HPPRenderPass &render_pass)
	{
		vkb::ResourceRecord::SetRenderPass(index, reinterpret_cast<vkb::RenderPass const &>(render_pass));
	}

	void SetShaderModule(size_t index, const vkb::core::HPPShaderModule &shader_module)
	{
		vkb::ResourceRecord::SetShaderModule(index, reinterpret_cast<vkb::ShaderModule const &>(shader_module));
	}
};
}        // namespace vkb
