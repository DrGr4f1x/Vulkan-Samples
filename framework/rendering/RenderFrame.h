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

#pragma once

#include "buffer_pool.h"
#include "common/helpers.h"
#include "common/resource_caching.h"
#include "common/vk_common.h"
#include "core/buffer.h"
#include "core/command_buffer.h"
#include "core/command_pool.h"
#include "core/device.h"
#include "core/image.h"
#include "core/query_pool.h"
#include "core/queue.h"
#include "fence_pool.h"
#include "rendering/render_target.h"
#include "semaphore_pool.h"

namespace vkb
{
enum BufferAllocationStrategy
{
	OneAllocationPerBuffer,
	MultipleAllocationsPerBuffer
};

enum DescriptorManagementStrategy
{
	StoreInCache,
	CreateDirectly
};

/**
 * @brief RenderFrame is a container for per-frame data, including BufferPool objects,
 * synchronization primitives (semaphores, fences) and the swapchain RenderTarget.
 *
 * When creating a RenderTarget, we need to provide images that will be used as attachments
 * within a RenderPass. The RenderFrame is responsible for creating a RenderTarget using
 * RenderTarget::CreateFunc. A custom RenderTarget::CreateFunc can be provided if a different
 * render target is required.
 *
 * A RenderFrame cannot be destroyed individually since frames are managed by the RenderContext,
 * the whole context must be destroyed. This is because each RenderFrame holds Vulkan objects
 * such as the swapchain image.
 */
class RenderFrame
{
  public:
	/**
	 * @brief Block size of a buffer pool in kilobytes
	 */
	static constexpr uint32_t BUFFER_POOL_BLOCK_SIZE = 256;

	// A map of the supported usages to a multiplier for the BUFFER_POOL_BLOCK_SIZE
	const std::unordered_map<VkBufferUsageFlags, uint32_t> supportedUsageMap = {
	    {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 1},
	    {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 2},        // x2 the size of BUFFER_POOL_BLOCK_SIZE since SSBOs are normally much larger than other types of buffers
	    {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1},
	    {VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 1}};

	RenderFrame(Device& device, std::unique_ptr<RenderTarget>&& renderTarget, size_t threadCount = 1);

	RenderFrame(const RenderFrame&) = delete;

	RenderFrame(RenderFrame&&) = delete;

	RenderFrame& operator=(const RenderFrame&) = delete;

	RenderFrame& operator=(RenderFrame&&) = delete;

	void Reset();

	Device& GetDevice();

	const FencePool& GetFencePool() const;

	VkFence RequestFence();

	const SemaphorePool& GetSemaphorePool() const;

	VkSemaphore RequestSemaphore();
	VkSemaphore RequestSemaphoreWithOwnership();
	void ReleaseOwnedSemaphore(VkSemaphore semaphore);

	/**
	 * @brief Called when the swapchain changes
	 * @param render_target A new render target with updated images
	 */
	void UpdateRenderTarget(std::unique_ptr<RenderTarget>&& renderTarget);

	RenderTarget& GetRenderTarget();

	const RenderTarget& GetRenderTargetConst() const;

	/**
	 * @brief Requests a command buffer to the command pool of the active frame
	 *        A frame should be active at the moment of requesting it
	 * @param queue The queue command buffers will be submitted on
	 * @param reset_mode Indicate how the command buffer will be used, may trigger a
	 *        pool re-creation to set necessary flags
	 * @param level Command buffer level, either primary or secondary
	 * @param thread_index Selects the thread's command pool used to manage the buffer
	 * @return A command buffer related to the current active frame
	 */
	CommandBuffer& RequestCommandBuffer(const Queue& queue,
	                                      CommandBuffer::ResetMode resetMode = CommandBuffer::ResetMode::ResetPool,
	                                      VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	                                      size_t threadIndex = 0);

	VkDescriptorSet RequestDescriptorSet(const DescriptorSetLayout& descriptorSetLayout,
	                                       const BindingMap<VkDescriptorBufferInfo>& bufferInfos,
	                                       const BindingMap<VkDescriptorImageInfo>& imageInfos,
	                                       bool updateAfterBind,
	                                       size_t threadIndex = 0);

	void ClearDescriptors();

	/**
	 * @brief Sets a new buffer allocation strategy
	 * @param new_strategy The new buffer allocation strategy
	 */
	void SetBufferAllocationStrategy(BufferAllocationStrategy newStrategy);

	/**
	 * @brief Sets a new descriptor set management strategy
	 * @param new_strategy The new descriptor set management strategy
	 */
	void SetDescriptorManagementStrategy(DescriptorManagementStrategy newStrategy);

	/**
	 * @param usage Usage of the buffer
	 * @param size Amount of memory required
	 * @param thread_index Index of the buffer pool to be used by the current thread
	 * @return The requested allocation, it may be empty
	 */
	BufferAllocationC AllocateBuffer(VkBufferUsageFlags usage, VkDeviceSize size, size_t threadIndex = 0);

	/**
	 * @brief Updates all the descriptor sets in the current frame at a specific thread index
	 */
	void UpdateDescriptorSets(size_t threadIndex = 0);

  private:
	Device& m_device;

	/**
	 * @brief Retrieve the frame's command pool(s)
	 * @param queue The queue command buffers will be submitted on
	 * @param reset_mode Indicate how the command buffers will be reset after execution,
	 *        may trigger a pool re-creation to set necessary flags
	 * @return The frame's command pool(s)
	 */
	std::vector<std::unique_ptr<CommandPool>>& GetCommandPools(const Queue& queue, CommandBuffer::ResetMode resetMode);

	/// Commands pools associated to the frame
	std::map<uint32_t, std::vector<std::unique_ptr<CommandPool>>> m_commandPools;

	/// Descriptor pools for the frame
	std::vector<std::unique_ptr<std::unordered_map<std::size_t, DescriptorPool>>> m_descriptorPools;

	/// Descriptor sets for the frame
	std::vector<std::unique_ptr<std::unordered_map<std::size_t, DescriptorSet>>> m_descriptorSets;

	FencePool m_fencePool;

	SemaphorePool m_semaphorePool;

	size_t m_threadCount;

	std::unique_ptr<RenderTarget> m_swapchainRenderTarget;

	BufferAllocationStrategy m_bufferAllocationStrategy{BufferAllocationStrategy::MultipleAllocationsPerBuffer};
	DescriptorManagementStrategy m_descriptorManagementStrategy{DescriptorManagementStrategy::StoreInCache};

	std::map<VkBufferUsageFlags, std::vector<std::pair<BufferPoolC, BufferBlockC*>>> m_bufferPools;

	static std::vector<uint32_t> CollectBindingsToUpdate(const DescriptorSetLayout& descriptorSetLayout, const BindingMap<VkDescriptorBufferInfo>& bufferInfos, const BindingMap<VkDescriptorImageInfo>& imageInfos);
};
}        // namespace vkb
