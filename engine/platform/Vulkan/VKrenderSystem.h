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

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "engine.h"
#include "renderer/camera.h"
#include "scene/entity.h"

#include "VKdevice.h"
#include "VKpipeline.h"
#include "VKframeInfo.h"
//#include "VKbuffer.h"
#include "VKdescriptor.h"

namespace GfxRenderEngine
{
    struct VK_SimplePushConstantData
    {
        glm::mat4 m_ModelMatrix{1.0f};
        glm::mat4 m_NormalMatrix{1.0f}; // 4x4 because of alignment
    };

    class VK_RenderSystem
    {

    public:

        VK_RenderSystem(std::shared_ptr<VK_Device> device, VkRenderPass renderPass, VK_DescriptorSetLayout& globalDescriptorSetLayout);
        ~VK_RenderSystem();

        VK_RenderSystem(const VK_RenderSystem&) = delete;
        VK_RenderSystem& operator=(const VK_RenderSystem&) = delete;

        void RenderEntities(const VK_FrameInfo& frameInfo, std::vector<Entity>& entities);

    private:

        void CreatePipelineLayout(VkDescriptorSetLayout globalDescriptorSetLayout);
        void CreatePipeline(VkRenderPass renderPass);

    private:

        std::shared_ptr<VK_Device> m_Device;
        VkPipelineLayout m_PipelineLayout;
        std::unique_ptr<VK_Pipeline> m_Pipeline;

    };
}