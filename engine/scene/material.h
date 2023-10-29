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

#pragma once

#include <string>
#include <memory>

#include <vulkan/vulkan.h>

#include "engine.h"

namespace GfxRenderEngine
{
    struct PbrNoMapMaterial
    {
        float m_Roughness;
        float m_Metallic;
        glm::vec3 m_Color;
    };

    struct PbrEmissiveMaterial
    {
        float m_Roughness;
        float m_Metallic;
        glm::vec3 m_EmissiveFactor;
        float m_EmissiveStrength;
    };

    struct PbrEmissiveTextureMaterial
    {
        VkDescriptorSet m_DescriptorSet;
        float m_Roughness;
        float m_Metallic;
        float m_EmissiveStrength;
    };

    struct PbrDiffuseMaterial
    {
        VkDescriptorSet m_DescriptorSet;
        float m_Roughness;
        float m_Metallic;
    };

    struct PbrDiffuseSAMaterial
    {
        VkDescriptorSet m_DescriptorSet;
        float m_Roughness;
        float m_Metallic;
    };

    struct PbrDiffuseNormalMaterial
    {
        VkDescriptorSet m_DescriptorSet;
        float m_Roughness;
        float m_Metallic;
        float m_NormalMapIntensity;
    };

    struct PbrDiffuseNormalRoughnessMetallicMaterial
    {
        VkDescriptorSet m_DescriptorSet;
        float m_NormalMapIntensity;
    };

    struct PbrDiffuseNormalRoughnessMetallicSAMaterial
    {
        VkDescriptorSet m_DescriptorSet;
        float m_NormalMapIntensity;
    };

    struct CubemapMaterial
    {
        VkDescriptorSet m_DescriptorSet;
    };
}
