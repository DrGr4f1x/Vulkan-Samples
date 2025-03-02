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

#include "RenderFrame.h"

#include "common/utils.h"
#include "core/util/logging.hpp"

namespace vkb
{

RenderFrame::RenderFrame(Device& device, std::unique_ptr<RenderTarget>&& renderTarget, size_t threadCount) 
	: m_device{ device }
	, m_fencePool{ device }
	, m_semaphorePool{ device }
	, m_swapchainRenderTarget{ std::move(renderTarget) }
	, m_threadCount{ threadCount }
{
	for (auto& usageIt : supportedUsageMap)
	{
		std::vector<std::pair<BufferPoolC, BufferBlockC*>> usageBufferPools;
		for (size_t i = 0; i < threadCount; ++i)
		{
			usageBufferPools.push_back(std::make_pair(BufferPoolC{ device, BUFFER_POOL_BLOCK_SIZE * 1024 * usageIt.second, usageIt.first }, nullptr));
		}

		auto resInsIt = m_bufferPools.emplace(usageIt.first, std::move(usageBufferPools));

		if (!resInsIt.second)
		{
			throw std::runtime_error("Failed to insert buffer pool");
		}
	}

	for (size_t i = 0; i < threadCount; ++i)
	{
		m_descriptorPools.push_back(std::make_unique<std::unordered_map<std::size_t, DescriptorPool>>());
		m_descriptorSets.push_back(std::make_unique<std::unordered_map<std::size_t, DescriptorSet>>());
	}
}


Device& RenderFrame::GetDevice()
{
	return m_device;
}


void RenderFrame::UpdateRenderTarget(std::unique_ptr<RenderTarget>&& renderTarget)
{
	m_swapchainRenderTarget = std::move(renderTarget);
}


void RenderFrame::Reset()
{
	VK_CHECK(m_fencePool.wait());

	m_fencePool.reset();

	for (auto& commandPoolsPerQueue : m_commandPools)
	{
		for (auto& commandPool : commandPoolsPerQueue.second)
		{
			commandPool->reset_pool();
		}
	}

	for (auto& bufferPoolsPerUsage : m_bufferPools)
	{
		for (auto& bufferPool : bufferPoolsPerUsage.second)
		{
			bufferPool.first.reset();
			bufferPool.second = nullptr;
		}
	}

	m_semaphorePool.reset();

	if (m_descriptorManagementStrategy == vkb::DescriptorManagementStrategy::CreateDirectly)
	{
		ClearDescriptors();
	}
}


std::vector<std::unique_ptr<CommandPool>>& RenderFrame::GetCommandPools(const Queue& queue, CommandBuffer::ResetMode resetMode)
{
	auto commandPoolIt = m_commandPools.find(queue.get_family_index());

	if (commandPoolIt != m_commandPools.end())
	{
		assert(!commandPoolIt->second.empty());
		if (commandPoolIt->second[0]->get_reset_mode() != resetMode)
		{
			m_device.wait_idle();

			// Delete pools
			m_commandPools.erase(commandPoolIt);
		}
		else
		{
			return commandPoolIt->second;
		}
	}

	std::vector<std::unique_ptr<CommandPool>> queueCommandPools;
	for (size_t i = 0; i < m_threadCount; i++)
	{
		queueCommandPools.push_back(std::make_unique<CommandPool>(m_device, queue.get_family_index(), this, i, resetMode));
	}

	auto resInsIt = m_commandPools.emplace(queue.get_family_index(), std::move(queueCommandPools));

	if (!resInsIt.second)
	{
		throw std::runtime_error("Failed to insert command pool");
	}

	commandPoolIt = resInsIt.first;

	return commandPoolIt->second;
}


std::vector<uint32_t> RenderFrame::CollectBindingsToUpdate(const DescriptorSetLayout& descriptorSetLayout, const BindingMap<VkDescriptorBufferInfo>& bufferInfos, const BindingMap<VkDescriptorImageInfo>& imageInfos)
{
	std::vector<uint32_t> bindingsToUpdate;

	bindingsToUpdate.reserve(bufferInfos.size() + imageInfos.size());
	auto aggregateBindingToUpdate = [&bindingsToUpdate, &descriptorSetLayout](const auto& infosMap) {
		for (const auto& pair : infosMap)
		{
			uint32_t bindingIndex = pair.first;
			if (!(descriptorSetLayout.GetLayoutBindingFlag(bindingIndex) & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT) &&
			    std::find(bindingsToUpdate.begin(), bindingsToUpdate.end(), bindingIndex) == bindingsToUpdate.end())
			{
				bindingsToUpdate.push_back(bindingIndex);
			}
		}
	};

	aggregateBindingToUpdate(bufferInfos);
	aggregateBindingToUpdate(imageInfos);

	return bindingsToUpdate;
}


const FencePool& RenderFrame::GetFencePool() const
{
	return m_fencePool;
}


VkFence RenderFrame::RequestFence()
{
	return m_fencePool.request_fence();
}


const SemaphorePool& RenderFrame::GetSemaphorePool() const
{
	return m_semaphorePool;
}


VkSemaphore RenderFrame::RequestSemaphore()
{
	return m_semaphorePool.request_semaphore();
}


VkSemaphore RenderFrame::RequestSemaphoreWithOwnership()
{
	return m_semaphorePool.request_semaphore_with_ownership();
}


void RenderFrame::ReleaseOwnedSemaphore(VkSemaphore semaphore)
{
	m_semaphorePool.release_owned_semaphore(semaphore);
}


RenderTarget& RenderFrame::GetRenderTarget()
{
	return *m_swapchainRenderTarget;
}


const RenderTarget& RenderFrame::GetRenderTargetConst() const
{
	return *m_swapchainRenderTarget;
}


CommandBuffer& RenderFrame::RequestCommandBuffer(const Queue& queue, CommandBuffer::ResetMode resetMode, VkCommandBufferLevel level, size_t threadIndex)
{
	assert(threadIndex < m_threadCount && "Thread index is out of bounds");

	auto& commandPools = GetCommandPools(queue, resetMode);

	auto commandPoolIt = std::find_if(commandPools.begin(), commandPools.end(), [&threadIndex](std::unique_ptr<CommandPool>& cmdPool) { return cmdPool->get_thread_index() == threadIndex; });

	return (*commandPoolIt)->request_command_buffer(level);
}


VkDescriptorSet RenderFrame::RequestDescriptorSet(const DescriptorSetLayout& descriptorSetLayout, const BindingMap<VkDescriptorBufferInfo>& bufferInfos, const BindingMap<VkDescriptorImageInfo>& imageInfos, bool updateAfterBind, size_t threadIndex)
{
	assert(threadIndex < m_threadCount && "Thread index is out of bounds");

	assert(threadIndex < m_descriptorPools.size());
	auto& descriptorPool = request_resource(m_device, nullptr, *m_descriptorPools[threadIndex], descriptorSetLayout);
	if (m_descriptorManagementStrategy == DescriptorManagementStrategy::StoreInCache)
	{
		// The bindings we want to update before binding, if empty we update all bindings
		std::vector<uint32_t> bindingsToUpdate;
		// If update after bind is enabled, we store the binding index of each binding that need to be updated before being bound
		if (updateAfterBind)
		{
			bindingsToUpdate = CollectBindingsToUpdate(descriptorSetLayout, bufferInfos, imageInfos);
		}

		// Request a descriptor set from the render frame, and write the buffer infos and image infos of all the specified bindings
		assert(threadIndex < m_descriptorSets.size());
		auto& descriptorSet = request_resource(m_device, nullptr, *m_descriptorSets[threadIndex], descriptorSetLayout, descriptorPool, bufferInfos, imageInfos);
		descriptorSet.Update(bindingsToUpdate);
		return descriptorSet.GetHandle();
	}
	else
	{
		// Request a descriptor pool, allocate a descriptor set, write buffer and image data to it
		DescriptorSet descriptorSet{ m_device, descriptorSetLayout, descriptorPool, bufferInfos, imageInfos };
		descriptorSet.ApplyWrites();
		return descriptorSet.GetHandle();
	}
}


void RenderFrame::UpdateDescriptorSets(size_t threadIndex)
{
	assert(threadIndex < m_descriptorSets.size());
	auto& threadDescriptorSets = *m_descriptorSets[threadIndex];
	for (auto& descriptorSetIt : threadDescriptorSets)
	{
		descriptorSetIt.second.Update();
	}
}


void RenderFrame::ClearDescriptors()
{
	for (auto& descSetsPerThread : m_descriptorSets)
	{
		descSetsPerThread->clear();
	}

	for (auto& descPoolsPerThread : m_descriptorPools)
	{
		for (auto& descPool : *descPoolsPerThread)
		{
			descPool.second.Reset();
		}
	}
}


void RenderFrame::SetBufferAllocationStrategy(BufferAllocationStrategy newStrategy)
{
	m_bufferAllocationStrategy = newStrategy;
}


void RenderFrame::SetDescriptorManagementStrategy(DescriptorManagementStrategy newStrategy)
{
	m_descriptorManagementStrategy = newStrategy;
}


BufferAllocationC RenderFrame::AllocateBuffer(const VkBufferUsageFlags usage, const VkDeviceSize size, size_t threadIndex)
{
	assert(threadIndex < m_threadCount && "Thread index is out of bounds");

	// Find a pool for this usage
	auto bufferPoolIt = m_bufferPools.find(usage);
	if (bufferPoolIt == m_bufferPools.end())
	{
		LOGE("No buffer pool for buffer usage {}", usage);
		return BufferAllocationC{};
	}

	assert(threadIndex < bufferPoolIt->second.size());
	auto& bufferPool  = bufferPoolIt->second[threadIndex].first;
	auto& bufferBlock = bufferPoolIt->second[threadIndex].second;

	bool wantMinimalBlock = m_bufferAllocationStrategy == BufferAllocationStrategy::OneAllocationPerBuffer;

	if (wantMinimalBlock || !bufferBlock || !bufferBlock->can_allocate(size))
	{
		// If we are creating a buffer for each allocation of there is no block associated with the pool or the current block is too small
		// for this allocation, request a new buffer block
		bufferBlock = &bufferPool.request_buffer_block(size, wantMinimalBlock);
	}

	return bufferBlock->allocate(to_u32(size));
}

} // namespace vkb