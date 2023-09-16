/* Engine Copyright (c) 2022 Engine Development Team 
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

#include "VKcore.h"
#include "VKswapChain.h"
#include "VKrenderPass.h"
#include "VKmodel.h"

#include "systems/VKpostprocessingSys.h"
#include "VKswapChain.h"

namespace GfxRenderEngine
{
    VK_RenderSystemPostProcessing::VK_RenderSystemPostProcessing
    (
        VkRenderPass renderPass,
        std::vector<VkDescriptorSetLayout>& postProcessingDescriptorSetLayouts,
        const VkDescriptorSet* postProcessingDescriptorSet
    )
    {
        CreatePostProcessingPipelineLayout(postProcessingDescriptorSetLayouts);
        m_PostProcessingDescriptorSets = postProcessingDescriptorSet;
        CreatePostProcessingPipeline(renderPass);
    }

    VK_RenderSystemPostProcessing::~VK_RenderSystemPostProcessing()
    {
        vkDestroyPipelineLayout(VK_Core::m_Device->Device(), m_PostProcessingPipelineLayout, nullptr);
    }

    void VK_RenderSystemPostProcessing::CreatePostProcessingPipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSetLayouts)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(VK_PushConstantDataPostProcessing);

        VkPipelineLayoutCreateInfo postProcessingPipelineLayoutInfo{};
        postProcessingPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        postProcessingPipelineLayoutInfo.setLayoutCount = static_cast<uint>(descriptorSetLayouts.size());
        postProcessingPipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        postProcessingPipelineLayoutInfo.pushConstantRangeCount = 1;
        postProcessingPipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        if (vkCreatePipelineLayout(VK_Core::m_Device->Device(), &postProcessingPipelineLayoutInfo, nullptr, &m_PostProcessingPipelineLayout) != VK_SUCCESS)
        {
            LOG_CORE_CRITICAL("failed to create pipeline layout!");
        }
    }

    void VK_RenderSystemPostProcessing::CreatePostProcessingPipeline(VkRenderPass renderPass)
    {
        ASSERT(m_PostProcessingPipelineLayout != nullptr);

        PipelineConfigInfo pipelineConfig{};

        VK_Pipeline::DefaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.pipelineLayout = m_PostProcessingPipelineLayout;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        pipelineConfig.subpass = static_cast<uint>(VK_RenderPass::SubPasses::SUBPASS_LIGHTING);

        // create a pipeline
        m_PostProcessingPipeline = std::make_unique<VK_Pipeline>
        (
            VK_Core::m_Device,
            "bin-int/postprocessing.vert.spv",
            "bin-int/postprocessing.frag.spv",
            pipelineConfig
        );
    }

    void VK_RenderSystemPostProcessing::PostProcessingPass(const VK_FrameInfo& frameInfo)
    {
        m_PostProcessingPipeline->Bind(frameInfo.m_CommandBuffer);

        std::vector<VkDescriptorSet> descriptorSets =
        {
            frameInfo.m_GlobalDescriptorSet,
            m_PostProcessingDescriptorSets[frameInfo.m_ImageIndex]
        };

        vkCmdBindDescriptorSets
        (
            frameInfo.m_CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_PostProcessingPipelineLayout,   // VkPipelineLayout layout
            0,                          // uint32_t         firstSet
            descriptorSets.size(),      // uint32_t         descriptorSetCount
            descriptorSets.data(),      // VkDescriptorSet* pDescriptorSets
            0,                          // uint32_t         dynamicOffsetCount
            nullptr                     // const uint32_t*  pDynamicOffsets
        );

        vkCmdDraw
        (
            frameInfo.m_CommandBuffer,
            3,      // vertexCount
            1,      // instanceCount
            0,      // firstVertex
            0       // firstInstance
        );
    }
}
