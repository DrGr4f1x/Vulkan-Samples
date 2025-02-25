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

#include "DescriptorSet.h"
#include <core/HppDescriptorPool.h>
#include <core/HppDescriptorSetLayout.h>

namespace vkb
{
namespace core
{
/**
 * @brief facade class around vkb::DescriptorSet, providing a vulkan.hpp-based interface
 *
 * See vkb::DescriptorSet for documentation
 */
class HPPDescriptorSet : private vkb::DescriptorSet
{
  public:
	using vkb::DescriptorSet::ApplyWrites;
	using vkb::DescriptorSet::Update;

	HPPDescriptorSet(vkb::core::HPPDevice                       &device,
	                 const vkb::core::HPPDescriptorSetLayout    &descriptor_set_layout,
	                 vkb::core::HPPDescriptorPool               &descriptor_pool,
	                 const BindingMap<vk::DescriptorBufferInfo> &buffer_infos = {},
	                 const BindingMap<vk::DescriptorImageInfo>  &image_infos  = {}) :
	    vkb::DescriptorSet(reinterpret_cast<vkb::Device &>(device),
	                       reinterpret_cast<vkb::DescriptorSetLayout const &>(descriptor_set_layout),
	                       reinterpret_cast<vkb::DescriptorPool &>(descriptor_pool),
	                       reinterpret_cast<BindingMap<VkDescriptorBufferInfo> const &>(buffer_infos),
	                       reinterpret_cast<BindingMap<VkDescriptorImageInfo> const &>(image_infos))
	{}

	BindingMap<vk::DescriptorBufferInfo>& GetBufferInfos()
	{
		return reinterpret_cast<BindingMap<vk::DescriptorBufferInfo> &>(vkb::DescriptorSet::GetBufferInfos());
	}

	vk::DescriptorSet GetHandle() const
	{
		return static_cast<vk::DescriptorSet>(vkb::DescriptorSet::GetHandle());
	}

	BindingMap<vk::DescriptorImageInfo>& GetImageInfos()
	{
		return reinterpret_cast<BindingMap<vk::DescriptorImageInfo> &>(vkb::DescriptorSet::GetImageInfos());
	}

	const vkb::core::HPPDescriptorSetLayout& GetLayout() const
	{
		return reinterpret_cast<vkb::core::HPPDescriptorSetLayout const &>(vkb::DescriptorSet::GetLayout());
	}
};
}        // namespace core
}        // namespace vkb
