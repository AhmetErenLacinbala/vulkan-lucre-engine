/* Engine Copyright (c) 2024 Engine Development Team
   https://github.com/beaumanvienna/vulkan

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation files
   (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <vulkan/vulkan.h>

#include "VKpool.h"
#include "VKswapChain.h"

namespace GfxRenderEngine
{
    VK_Pool::VK_Pool(VkDevice& device, QueueFamilyIndices& queueFamilyIndices, ThreadPool& threadPoolPrimary,
                     ThreadPool& threadPoolSecondary)
        : m_Device{device}, m_QueueFamilyIndices{queueFamilyIndices}, m_PoolPrimary(threadPoolPrimary),
          m_PoolSecondary(threadPoolSecondary)
    {
        // helper lambdas
        auto createCommandPool = [this]()
        {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = m_QueueFamilyIndices.m_GraphicsFamily;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            VkCommandPool commandPool;
            if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
            {
                LOG_CORE_CRITICAL("failed to create graphics command pool!");
            }
            return commandPool;
        };

        auto createDescriptorPool = [device]()
        {
            static constexpr uint POOL_SIZE = 5000;
            std::unique_ptr<VK_DescriptorPool> descriptorPool =
                VK_DescriptorPool::Builder(device)
                    .SetMaxSets(VK_SwapChain::MAX_FRAMES_IN_FLIGHT * POOL_SIZE)
                    .AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SwapChain::MAX_FRAMES_IN_FLIGHT * 500)
                    .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SwapChain::MAX_FRAMES_IN_FLIGHT * 500)
                    .AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SwapChain::MAX_FRAMES_IN_FLIGHT * 3500)
                    .AddPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SwapChain::MAX_FRAMES_IN_FLIGHT * 500)
                    .Build();
            return descriptorPool;
        };

        auto loopOverWorkerThreads = [this, createCommandPool, createDescriptorPool](ThreadPool& pool)
        {
            // loop over all worker threads and fill the unordered maps for
            // thread id -> command pool and
            // thread id -> descriptor pool
            for (auto& threadID : pool.GetThreadIDs())
            {
                // Hash for thread id (created from a hasher class object and the thread id).
                uint64 hash = std::hash<std::thread::id>()(threadID);

                // create command pools for the thread pool
                m_CommandPools.emplace(hash, createCommandPool());

                // create descriptor pools for the thread pool
                m_DescriptorPools.emplace(hash, createDescriptorPool());
            }
        };

        loopOverWorkerThreads(m_PoolPrimary);
        loopOverWorkerThreads(m_PoolSecondary);

        // fill the unordered maps for also for main thread
        {
            // Calling get() on the future blocks until the worker thread from above terminates.
            std::thread::id threadID = std::this_thread::get_id();

            // Hash for thread id (created from a hasher class object and the thread id).
            uint64 hash = std::hash<std::thread::id>()(threadID);
            // create command pools for the thread pool
            m_CommandPools.emplace(hash, createCommandPool());

            // create descriptor pools for the thread pool
            m_DescriptorPools.emplace(hash, createDescriptorPool());
        }
    }

    VK_Pool::~VK_Pool()
    {
        for (auto& commandPool : m_CommandPools)
        {
            vkDestroyCommandPool(m_Device, commandPool.second, nullptr);
        }
    }

    VkCommandPool& VK_Pool::GetCommandPool()
    {
        std::thread::id threadID = std::this_thread::get_id();
        uint64 hash = std::hash<std::thread::id>()(threadID);
        VkCommandPool& pool = m_CommandPools[hash];
        CORE_ASSERT(pool != nullptr, "no command pool found!");
        if (!pool)
        {
            std::cout << "terminating because of thread id:" << threadID << "\n";
            exit(1);
        }
        return pool;
    }

    VK_DescriptorPool& VK_Pool::GetDescriptorPool()
    {
        std::thread::id threadID = std::this_thread::get_id();
        uint64 hash = std::hash<std::thread::id>()(threadID);
        CORE_ASSERT(m_DescriptorPools[hash] != nullptr, "no command pool found!");
        if (!m_DescriptorPools[hash])
        {
            std::cout << "thread id:" << threadID << "\n";
            exit(1);
        }
        return *m_DescriptorPools[hash];
    }

    void VK_Pool::ResetCommandPool()
    {
        VkCommandPoolResetFlags flags{VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT};
        vkResetCommandPool(m_Device, GetCommandPool(), flags);
    }

    void VK_Pool::ResetCommandPools(ThreadPool& threadpool)
    {
        VkCommandPoolResetFlags flags{VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT};
        for (auto& threadID : threadpool.GetThreadIDs())
        {
            std::cout << "VK_Pool::ResetAllCommandPools: " << "):" << threadID << "\n";
            uint64 hash = std::hash<std::thread::id>()(threadID);
            vkResetCommandPool(m_Device, m_CommandPools[hash], flags);
        }
    }

    void VK_Pool::ResetDescriptorPool() { GetDescriptorPool().ResetPool(); }

    void VK_Pool::ResetDescriptorPools(ThreadPool& threadpool)
    {
        for (auto& threadID : threadpool.GetThreadIDs())
        {
            std::cout << "VK_Pool::ResetAllCommandPools: " << "):" << threadID << "\n";
            uint64 hash = std::hash<std::thread::id>()(threadID);
            m_DescriptorPools[hash]->ResetPool();
        }
    }
} // namespace GfxRenderEngine
