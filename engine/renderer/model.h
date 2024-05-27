/* Engine Copyright (c) 2023 Engine Development Team
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

#define GL_BYTE 0x1400           // 5120
#define GL_UNSIGNED_BYTE 0x1401  // 5121
#define GL_SHORT 0x1402          // 5122
#define GL_UNSIGNED_SHORT 0x1403 // 5123
#define GL_INT 0x1404            // 5124
#define GL_UNSIGNED_INT 0x1405   // 5125
#define GL_FLOAT 0x1406          // 5126
#define GL_2_BYTES 0x1407        // 5127
#define GL_3_BYTES 0x1408        // 5128
#define GL_4_BYTES 0x1409        // 5129
#define GL_DOUBLE 0x140A         // 5130

#include <memory>

#include "tinygltf/tiny_gltf.h"

#include "engine.h"
#include "scene/material.h"
#include "scene/sceneGraph.h"
#include "scene/components.h"
#include "scene/dictionary.h"
#include "renderer/skeletalAnimation/skeleton.h"
#include "renderer/skeletalAnimation/skeletalAnimations.h"
#include "renderer/materialDescriptor.h"
#include "renderer/texture.h"
#include "renderer/cubemap.h"
#include "sprite/sprite.h"
#include "entt.hpp"

namespace GfxRenderEngine
{
    struct Vertex // 3D, with animation
    {
        glm::vec3 m_Position;    // layout(location = 0)
        glm::vec4 m_Color;       // layout(location = 1)
        glm::vec3 m_Normal;      // layout(location = 2)
        glm::vec2 m_UV;          // layout(location = 3)
        glm::vec3 m_Tangent;     // layout(location = 4)
        glm::ivec4 m_JointIds;   // layout(location = 5)
        glm::vec4 m_Weights;     // layout(location = 6)
    };

    struct Submesh
    {
        uint m_FirstIndex;
        uint m_FirstVertex;
        uint m_IndexCount;
        uint m_VertexCount;
        uint m_InstanceCount;
        Material m_Material;
    };

    class Model
    {

    public:
        Model() {}
        virtual ~Model() = default;

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;

        virtual void CreateVertexBuffer(const std::vector<Vertex>& vertices) = 0;
        virtual void CreateIndexBuffer(const std::vector<uint>& indices) = 0;

        SkeletalAnimations& GetAnimations();

        static float m_NormalMapIntensity;

    protected:
        std::vector<std::shared_ptr<Texture>> m_Textures;
        std::vector<std::shared_ptr<Cubemap>> m_Cubemaps;

        // skeletal animation
        std::shared_ptr<SkeletalAnimations> m_Animations;
        std::shared_ptr<Armature::Skeleton> m_Skeleton;
        std::shared_ptr<Buffer> m_ShaderDataUbo;
    };
} // namespace GfxRenderEngine
