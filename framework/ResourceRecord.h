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

#include <vector>

#include "rendering/pipeline_state.h"

namespace vkb
{
class GraphicsPipeline;
class PipelineLayout;
class RenderPass;
class ShaderModule;

enum class ResourceType
{
	ShaderModule,
	PipelineLayout,
	RenderPass,
	GraphicsPipeline
};

/**
 * @brief Writes Vulkan objects in a memory stream.
 */
class ResourceRecord
{
  public:
	void SetData(const std::vector<uint8_t>& data);

	std::vector<uint8_t> GetData();

	const std::ostringstream& GetStream();

	size_t RegisterShaderModule(VkShaderStageFlagBits stage, const ShaderSource& glslSource, const std::string& entryPoint, const ShaderVariant& shaderVariant);

	size_t RegisterPipelineLayout(const std::vector<ShaderModule*>& shaderModules);

	size_t RegisterRenderPass(const std::vector<Attachment>& attachments, const std::vector<LoadStoreInfo>& loadStoreInfos, const std::vector<SubpassInfo>& subpasses);

	size_t RegisterGraphicsPipeline(VkPipelineCache pipelineCache, PipelineState& pipelineState);

	void SetShaderModule(size_t index, const ShaderModule& shaderModule);

	void SetPipelineLayout(size_t index, const PipelineLayout& pipelineLayout);

	void SetRenderPass(size_t index, const RenderPass& renderPass);

	void SetGraphicsPipeline(size_t index, const GraphicsPipeline& graphicsPipeline);

  private:
	std::ostringstream m_stream;

	std::vector<size_t> m_shaderModuleIndices;

	std::vector<size_t> m_pipelineLayoutIndices;

	std::vector<size_t> m_renderPassIndices;

	std::vector<size_t> m_graphicsPipelineIndices;

	std::unordered_map<const ShaderModule*, size_t> m_shaderModuleToIndex;

	std::unordered_map<const PipelineLayout*, size_t> m_pipelineLayoutToIndex;

	std::unordered_map<const RenderPass*, size_t> m_renderPassToIndex;

	std::unordered_map<const GraphicsPipeline*, size_t> m_graphicsPipelineToIndex;
};

}        // namespace vkb
