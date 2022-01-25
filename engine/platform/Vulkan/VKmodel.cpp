/* Engine Copyright (c) 2022 Engine Development Team 
   https://github.com/beaumanvienna/gfxRenderEngine

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

#include "VKmodel.h"

namespace GfxRenderEngine
{

    // Vertex
    std::vector<VkVertexInputBindingDescription> VK_Model::VK_Vertex::GetBindingDescriptions()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);

        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride  = sizeof(Vertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> VK_Model::VK_Vertex::GetAttributeDescriptions()
    {
        std::vector<VkVertexInputAttributeDescription>  attributeDescriptions(2);

        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);//sizeof(Vertex::position);

        return attributeDescriptions;
    }

    // VK_Model
    VK_Model::VK_Model(std::shared_ptr<VK_Device> device, const Builder& builder)
        : m_Device(device), m_HasIndexBuffer{false}
    {
        CreateVertexBuffers(builder.m_Vertices);
        CreateIndexBuffers(builder.m_Indices);
    }

    VK_Model::~VK_Model()
    {
        vkDestroyBuffer(m_Device->Device(), m_VertexBuffer, nullptr);
        vkFreeMemory(m_Device->Device(), m_VertexBufferMemory, nullptr);

        if (m_HasIndexBuffer)
        {
            vkDestroyBuffer(m_Device->Device(), m_IndexBuffer, nullptr);
            vkFreeMemory(m_Device->Device(), m_IndexBufferMemory, nullptr);
        }
    }

    void VK_Model::CreateVertexBuffers(const std::vector<Vertex>& vertices)
    {
        m_VertexCount = static_cast<uint>(vertices.size());
        ASSERT(m_VertexCount >= 3); // at least one triangle
        VkDeviceSize bufferSize = sizeof(Vertex) * m_VertexCount;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        m_Device->CreateBuffer
        (
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        void* data;
        vkMapMemory
        (
            m_Device->Device(),
            stagingBufferMemory,
            0,
            bufferSize,
            0,
            &data
        );
        memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(m_Device->Device(), stagingBufferMemory);

        m_Device->CreateBuffer
        (
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_VertexBuffer,
            m_VertexBufferMemory
        );
        m_Device->CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

        vkDestroyBuffer(m_Device->Device(), stagingBuffer, nullptr);
        vkFreeMemory(m_Device->Device(), stagingBufferMemory, nullptr);
    }

    void VK_Model::CreateIndexBuffers(const std::vector<uint>& indices)
    {
        m_IndexCount = static_cast<uint>(indices.size());
        VkDeviceSize bufferSize = sizeof(uint) * m_IndexCount;
        m_HasIndexBuffer = ( m_IndexCount > 0);

        if (!m_HasIndexBuffer)
        {
            return;
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        m_Device->CreateBuffer
        (
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        void* data;
        vkMapMemory
        (
            m_Device->Device(),
            stagingBufferMemory,
            0,
            bufferSize,
            0,
            &data
        );
        memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(m_Device->Device(), stagingBufferMemory);

        m_Device->CreateBuffer
        (
            bufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_IndexBuffer,
            m_IndexBufferMemory
        );
        m_Device->CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

        vkDestroyBuffer(m_Device->Device(), stagingBuffer, nullptr);
        vkFreeMemory(m_Device->Device(), stagingBufferMemory, nullptr);
    }

    void VK_Model::Bind(VkCommandBuffer commandBuffer)
    {
        VkBuffer buffers[] = {m_VertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

        if (m_HasIndexBuffer)
        {
            vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void VK_Model::Draw(VkCommandBuffer commandBuffer)
    {
        if (m_HasIndexBuffer)
        {
            vkCmdDrawIndexed
            (
                commandBuffer,      // VkCommandBuffer commandBuffer
                m_IndexCount,       // uint32_t        indexCount
                1,                  // uint32_t        instanceCount
                0,                  // uint32_t        firstIndex
                0,                  // int32_t         vertexOffset
                0                   // uint32_t        firstInstance
            );
        }
        else
        {
            vkCmdDraw
            (
                commandBuffer,      // VkCommandBuffer commandBuffer
                m_VertexCount,      // uint32_t        vertexCount
                1,                  // uint32_t        instanceCount
                0,                  // uint32_t        firstVertex
                0                   // uint32_t        firstInstance
            );
        }
    }
}
