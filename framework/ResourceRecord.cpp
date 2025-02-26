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

#include "ResourceRecord.h"

#include "core/pipeline.h"
#include "core/pipeline_layout.h"
#include "core/render_pass.h"
#include "core/shader_module.h"
#include "ResourceCache.h"

namespace vkb
{

namespace
{

inline void WriteSubpassInfo(std::ostringstream& os, const std::vector<SubpassInfo>& value)
{
	write(os, value.size());
	for (const SubpassInfo& item : value)
	{
		write(os, item.input_attachments);
		write(os, item.output_attachments);
	}
}


inline void WriteProcesses(std::ostringstream& os, const std::vector<std::string>& value)
{
	write(os, value.size());
	for (const std::string& item : value)
	{
		write(os, item);
	}
}

} // anonymous namespace


void ResourceRecord::SetData(const std::vector<uint8_t>& data)
{
	m_stream.str(std::string{data.begin(), data.end()});
}


std::vector<uint8_t> ResourceRecord::GetData()
{
	std::string str = m_stream.str();

	return std::vector<uint8_t>{ str.begin(), str.end() };
}


const std::ostringstream &ResourceRecord::GetStream()
{
	return m_stream;
}


size_t ResourceRecord::RegisterShaderModule(VkShaderStageFlagBits stage, const ShaderSource& glslSource, const std::string& entryPoint, const ShaderVariant& shaderVariant)
{
	m_shaderModuleIndices.push_back(m_shaderModuleIndices.size());

	write(m_stream, ResourceType::ShaderModule, stage, glslSource.get_source(), entryPoint, shaderVariant.get_preamble());

	WriteProcesses(m_stream, shaderVariant.get_processes());

	return m_shaderModuleIndices.back();
}


size_t ResourceRecord::RegisterPipelineLayout(const std::vector<ShaderModule*>& shaderModules)
{
	m_pipelineLayoutIndices.push_back(m_pipelineLayoutIndices.size());

	std::vector<size_t> shaderIndices(shaderModules.size());
	std::transform(shaderModules.begin(), shaderModules.end(), shaderIndices.begin(),
	               [this](ShaderModule* shaderModule) { return m_shaderModuleToIndex.at(shaderModule); });

	write(m_stream, ResourceType::PipelineLayout, shaderIndices);

	return m_pipelineLayoutIndices.back();
}


size_t ResourceRecord::RegisterRenderPass(const std::vector<Attachment>& attachments, const std::vector<LoadStoreInfo>& loadStoreInfos, const std::vector<SubpassInfo>& subpasses)
{
	m_renderPassIndices.push_back(m_renderPassIndices.size());

	write(m_stream, ResourceType::RenderPass, attachments, loadStoreInfos);

	WriteSubpassInfo(m_stream, subpasses);

	return m_renderPassIndices.back();
}


size_t ResourceRecord::RegisterGraphicsPipeline(VkPipelineCache /*pipeline_cache*/, PipelineState& pipelineState)
{
	m_graphicsPipelineIndices.push_back(m_graphicsPipelineIndices.size());

	auto& pipelineLayout = pipelineState.get_pipeline_layout();
	auto renderPass = pipelineState.get_render_pass();

	write(m_stream,
	      ResourceType::GraphicsPipeline,
	      m_pipelineLayoutToIndex.at(&pipelineLayout),
	      m_renderPassToIndex.at(renderPass),
	      pipelineState.get_subpass_index());

	auto& specializationConstantState = pipelineState.get_specialization_constant_state().get_specialization_constant_state();

	write(m_stream, specializationConstantState);

	auto& vertexInputState = pipelineState.get_vertex_input_state();

	write(m_stream, vertexInputState.attributes, vertexInputState.bindings);

	write(m_stream,
	      pipelineState.get_input_assembly_state(),
	      pipelineState.get_rasterization_state(),
	      pipelineState.get_viewport_state(),
	      pipelineState.get_multisample_state(),
	      pipelineState.get_depth_stencil_state());

	auto& colorBlendState = pipelineState.get_color_blend_state();

	write(m_stream,
	      colorBlendState.logic_op,
	      colorBlendState.logic_op_enable,
	      colorBlendState.attachments);

	return m_graphicsPipelineIndices.back();
}


void ResourceRecord::SetShaderModule(size_t index, const ShaderModule& shaderModule)
{
	m_shaderModuleToIndex[&shaderModule] = index;
}


void ResourceRecord::SetPipelineLayout(size_t index, const PipelineLayout& pipelineLayout)
{
	m_pipelineLayoutToIndex[&pipelineLayout] = index;
}


void ResourceRecord::SetRenderPass(size_t index, const RenderPass& renderPass)
{
	m_renderPassToIndex[&renderPass] = index;
}


void ResourceRecord::SetGraphicsPipeline(size_t index, const GraphicsPipeline& graphicsPipeline)
{
	m_graphicsPipelineToIndex[&graphicsPipeline] = index;
}

} // namespace vkb