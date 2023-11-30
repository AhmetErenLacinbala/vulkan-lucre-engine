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

#include "gtc/type_ptr.hpp"
#include "stb_image.h"

#include "core.h"
#include "renderer/builder/gltfBuilder.h"
#include "auxiliary/file.h"

#include "VKmodel.h"

namespace GfxRenderEngine
{

    GltfBuilder::GltfBuilder(const std::string& filepath, Scene& scene)
        : m_Filepath{filepath}, m_SkeletalAnimation{0}, m_Registry{scene.GetRegistry()},
          m_SceneGraph{scene.GetSceneGraph()}, m_Dictionary{scene.GetDictionary()},
          m_InstanceCount{0}, m_InstanceIndex{0}
    {
        m_Basepath = EngineCore::GetPathWithoutFilename(filepath);
    }

    bool GltfBuilder::LoadGltf(uint const instanceCount, int const sceneID)
    {
        { // load ascii from file
            std::string warn, err;

            stbi_set_flip_vertically_on_load(false);
            if (!m_GltfLoader.LoadASCIIFromFile(&m_GltfModel, &err, &warn, m_Filepath))
            {
                LOG_CORE_CRITICAL("LoadGltf errors: {0}, warnings: {1}", err, warn);
                return Gltf::GLTF_LOAD_FAILURE;
            }
        }

        if (!m_GltfModel.meshes.size())
        {
            LOG_CORE_CRITICAL("LoadGltf: no meshes found in {0}", m_Filepath);
            return Gltf::GLTF_LOAD_FAILURE;
        }

        if (sceneID > Gltf::GLTF_NOT_USED) // a scene ID was provided
        {
            // check if valid
            if ((m_GltfModel.scenes.size()-1) < static_cast<size_t>(sceneID))
            {
                LOG_CORE_CRITICAL("LoadGltf: scene not found in {0}", m_Filepath);
                return Gltf::GLTF_LOAD_FAILURE;
            }
        }

        LoadImagesGltf();
        LoadSkeletonsGltf();
        LoadMaterialsGltf();

        // PASS 1
        // mark gltf nodes to receive a game object ID if they have a mesh or any child has
        // --> create array of flags for all nodes of the gltf file
        m_HasMesh.resize(m_GltfModel.nodes.size(), false);
        if (sceneID > Gltf::GLTF_NOT_USED) // a scene ID was provided
        {
            auto& scene = m_GltfModel.scenes[sceneID];
            size_t nodeCount = scene.nodes.size();
            for (size_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
            {
                MarkNode(scene, scene.nodes[nodeIndex]);
            }
        }
        else //no scene ID was provided --> use all scenes
        {
            for (auto& scene : m_GltfModel.scenes)
            {
                size_t nodeCount = scene.nodes.size();
                for (size_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
                {
                    MarkNode(scene, scene.nodes[nodeIndex]);
                }
            }
        }

        // PASS 2 (for all instances)
        m_InstanceCount = instanceCount;
        for(m_InstanceIndex = 0; m_InstanceIndex < m_InstanceCount; ++m_InstanceIndex)
        {
            // create group game object(s) for all instances to apply transform from JSON file to
            auto entity = m_Registry.create();
            TransformComponent transform{};
            m_Registry.emplace<TransformComponent>(entity, transform);

            std::string name = EngineCore::GetFilenameWithoutPathAndExtension(m_Filepath);
            auto shortName = name + "::" + std::to_string(m_InstanceIndex) + "::root";
            auto longName = m_Filepath + "::" + std::to_string(m_InstanceIndex) + "::root";
            uint groupNode = m_SceneGraph.CreateNode(entity, shortName, longName, m_Dictionary);
            m_SceneGraph.GetRoot().AddChild(groupNode);

            // a scene ID was provided
            if (sceneID > Gltf::GLTF_NOT_USED)
            {
                ProcessScene(m_GltfModel.scenes[sceneID], groupNode);
            }
            else //no scene ID was provided --> use all scenes
            {
                for (auto& scene : m_GltfModel.scenes)
                {
                    ProcessScene(scene, groupNode);
                }
            }
        }
        return Gltf::GLTF_LOAD_SUCCESS;
    }

    bool GltfBuilder::MarkNode(tinygltf::Scene& scene, int const gltfNodeIndex)
    {
        // each recursive call of this function marks a node in "m_HasMesh" if itself or a child has a mesh
        auto& node = m_GltfModel.nodes[gltfNodeIndex];

        // does this gltf node have a mesh?
        bool localHasMesh = (node.mesh != Gltf::GLTF_NOT_USED);

        // do any of the child nodes have a mesh?
        size_t childNodeCount = node.children.size();
        for (size_t childNodeIndex = 0; childNodeIndex < childNodeCount; ++childNodeIndex)
        {
            int gltfChildNodeIndex = node.children[childNodeIndex];
            bool childHasMesh = MarkNode(scene, gltfChildNodeIndex);
            localHasMesh = localHasMesh || childHasMesh;
        }
        m_HasMesh[gltfNodeIndex] = localHasMesh;
        return localHasMesh;
    }

    void GltfBuilder::ProcessScene(tinygltf::Scene& scene, uint const parentNode)
    {
        size_t nodeCount = scene.nodes.size();
        if (!nodeCount)
        {
            LOG_CORE_WARN("Builder::ProcessScene: empty scene in {0}", m_Filepath);
            return;
        }

        for (uint nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
        {
            ProcessNode(scene, scene.nodes[nodeIndex], parentNode);
        }
    }

    void GltfBuilder::ProcessNode(tinygltf::Scene& scene, int const gltfNodeIndex, uint const parentNode)
    {
        auto& node = m_GltfModel.nodes[gltfNodeIndex];
        auto& nodeName = node.name;
        auto meshIndex = node.mesh;

        uint currentNode = parentNode;

        if (m_HasMesh[gltfNodeIndex]) 
        {
            if (meshIndex > Gltf::GLTF_NOT_USED)
            {
                currentNode = CreateGameObject(scene, gltfNodeIndex, parentNode);
            }
            else // one or more children have a mesh, but not this one --> create group node
            {
                // create game object and transform component
                auto entity = m_Registry.create();
                TransformComponent transform{};
                LoadTransformationMatrix(transform, gltfNodeIndex);
                m_Registry.emplace<TransformComponent>(entity, transform);

                // create scene graph node and add to parent
                auto shortName = "::" + std::to_string(m_InstanceIndex) + "::" + scene.name + "::" + nodeName;
                auto longName = m_Filepath + "::" + std::to_string(m_InstanceIndex) + "::" + scene.name + "::" + nodeName;
                currentNode = m_SceneGraph.CreateNode(entity, shortName, longName, m_Dictionary);
                m_SceneGraph.GetNode(parentNode).AddChild(currentNode);
            }
        }

        size_t childNodeCount = node.children.size();
        for (size_t childNodeIndex = 0; childNodeIndex < childNodeCount; ++childNodeIndex)
        {
            int gltfChildNodeIndex = node.children[childNodeIndex];
            ProcessNode(scene, gltfChildNodeIndex, currentNode);
        }
    }

    uint GltfBuilder::CreateGameObject(tinygltf::Scene& scene, int const gltfNodeIndex, uint const parentNode)
    {
        auto& node = m_GltfModel.nodes[gltfNodeIndex];
        auto& nodeName = node.name;
        uint meshIndex = node.mesh;

        LoadVertexDataGltf(meshIndex);
        LOG_CORE_INFO("Vertex count: {0}, Index count: {1} (file: {2}, node: {3})", m_Vertices.size(), m_Indices.size(), m_Filepath, nodeName);

        auto model = Engine::m_Engine->LoadModel(*this);
        auto entity = m_Registry.create();
        auto shortName = EngineCore::GetFilenameWithoutPathAndExtension(m_Filepath) + "::" + std::to_string(m_InstanceIndex) + "::" + scene.name + "::" + nodeName;
        auto longName = m_Filepath + "::" + std::to_string(m_InstanceIndex) + "::" + scene.name + "::" + nodeName;

        uint newNode = m_SceneGraph.CreateNode(entity, shortName, longName, m_Dictionary);
        m_SceneGraph.GetNode(parentNode).AddChild(newNode);

        // mesh
        MeshComponent mesh{nodeName, model};
        m_Registry.emplace<MeshComponent>(entity, mesh);

        // transform
        TransformComponent transform{};
        LoadTransformationMatrix(transform, gltfNodeIndex);
        m_Registry.emplace<TransformComponent>(entity, transform);

        // material tags (can have multiple tags)
        bool hasPbrMaterial = false;

        // vertex diffuse color, diffuse map, normal map, roughness/metallic map
        if (m_PrimitivesNoMap.size())
        {
            hasPbrMaterial = true;

            PbrNoMapTag pbrNoMapTag{};
            m_Registry.emplace<PbrNoMapTag>(entity, pbrNoMapTag);
        }
        if (m_PrimitivesDiffuseMap.size())
        {
            hasPbrMaterial = true;

            PbrDiffuseTag pbrDiffuseTag{};
            m_Registry.emplace<PbrDiffuseTag>(entity, pbrDiffuseTag);
        }
        if (m_PrimitivesDiffuseSAMap.size())
        {
            hasPbrMaterial = true;

            PbrDiffuseSATag pbrDiffuseSATag{};
            m_Registry.emplace<PbrDiffuseSATag>(entity, pbrDiffuseSATag);

            SkeletalAnimationTag skeletalAnimationTag{};
            m_Registry.emplace<SkeletalAnimationTag>(entity, skeletalAnimationTag);
        }
        if (m_PrimitivesDiffuseNormalMap.size())
        {
            hasPbrMaterial = true;

            PbrDiffuseNormalTag pbrDiffuseNormalTag;
            m_Registry.emplace<PbrDiffuseNormalTag>(entity, pbrDiffuseNormalTag);
        }
        if (m_PrimitivesDiffuseNormalSAMap.size())
        {
            hasPbrMaterial = true;

            PbrDiffuseNormalSATag pbrDiffuseNormalSATag;
            m_Registry.emplace<PbrDiffuseNormalSATag>(entity, pbrDiffuseNormalSATag);

            SkeletalAnimationTag skeletalAnimationTag{};
            m_Registry.emplace<SkeletalAnimationTag>(entity, skeletalAnimationTag);
        }
        if (m_PrimitivesDiffuseNormalRoughnessMetallicMap.size())
        {
            hasPbrMaterial = true;

            PbrDiffuseNormalRoughnessMetallicTag pbrDiffuseNormalRoughnessMetallicTag;
            m_Registry.emplace<PbrDiffuseNormalRoughnessMetallicTag>(entity, pbrDiffuseNormalRoughnessMetallicTag);
        }

        if (m_PrimitivesDiffuseNormalRoughnessMetallicSAMap.size())
        {
            hasPbrMaterial = true;

            PbrDiffuseNormalRoughnessMetallicSATag pbrDiffuseNormalRoughnessMetallicSATag;
            m_Registry.emplace<PbrDiffuseNormalRoughnessMetallicSATag>(entity, pbrDiffuseNormalRoughnessMetallicSATag);

            SkeletalAnimationTag skeletalAnimationTag{};
            m_Registry.emplace<SkeletalAnimationTag>(entity, skeletalAnimationTag);
        }

        // emissive materials
        if (m_PrimitivesEmissive.size())
        {
            hasPbrMaterial = true;

            PbrEmissiveTag pbrEmissiveTag{};
            m_Registry.emplace<PbrEmissiveTag>(entity, pbrEmissiveTag);
        }

        if (m_PrimitivesEmissiveTexture.size())
        {
            hasPbrMaterial = true;

            PbrEmissiveTextureTag pbrEmissiveTextureTag{};
            m_Registry.emplace<PbrEmissiveTextureTag>(entity, pbrEmissiveTextureTag);
        }

        if (hasPbrMaterial)
        {
            PbrMaterial pbrMaterial{};
            m_Registry.emplace<PbrMaterial>(entity, pbrMaterial);
        }
        return newNode;
    }

    int GltfBuilder::GetMinFilter(uint index)
    {
        int sampler = m_GltfModel.textures[index].sampler;
        int filter = m_GltfModel.samplers[sampler].minFilter;
        std::string& name = m_GltfModel.images[index].name;
        switch (filter)
        {
            case TINYGLTF_TEXTURE_FILTER_NEAREST: { break; }
            case TINYGLTF_TEXTURE_FILTER_LINEAR: { break; }
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: { break; }
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: { break; }
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: { break; }
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: { break; }
            case Gltf::GLTF_NOT_USED:
            {
                // use default filter
                filter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
                break;
            }
            default:
            {
                // use default filter
                filter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
                LOG_CORE_ERROR("minFilter: filter {0} not found, name = {1}", filter, name);
                break;
            }
        }
        return filter;
    }

    int GltfBuilder::GetMagFilter(uint index)
    {
        int sampler = m_GltfModel.textures[index].sampler;
        int filter = m_GltfModel.samplers[sampler].magFilter;
        std::string& name = m_GltfModel.images[index].name;
        switch (filter)
        {
            case TINYGLTF_TEXTURE_FILTER_NEAREST: { break; }
            case TINYGLTF_TEXTURE_FILTER_LINEAR: { break; }
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: { break; }
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: { break; }
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: { break; }
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: { break; }
            case Gltf::GLTF_NOT_USED:
            {
                // use default filter
                filter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
                break;
            }
            default:
            {
                filter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
                LOG_CORE_ERROR("magFilter: filter {0} not found, name = {1}", filter, name);
                break;
            }
        }
        return filter;
    }

    void GltfBuilder::LoadImagesGltf()
    {
        m_ImageOffset = m_Images.size();
        // retrieve all images from the glTF file
        for (uint i = 0; i < m_GltfModel.images.size(); i++)
        {
            std::string imageFilepath = m_Basepath + m_GltfModel.images[i].uri;
            tinygltf::Image& glTFImage = m_GltfModel.images[i];

            // glTFImage.component - the number of channels in each pixel
            // three channels per pixel need to be converted to four channels per pixel
            uchar* buffer;
            uint64 bufferSize;
            if (glTFImage.component == 3)
            {
                bufferSize = glTFImage.width * glTFImage.height * 4;
                std::vector<uchar> imageData(bufferSize, 0x00);

                buffer = (uchar*)imageData.data();
                uchar* rgba = buffer;
                uchar* rgb = &glTFImage.image[0];
                for (int j = 0; j < glTFImage.width * glTFImage.height; ++j)
                {
                    memcpy(rgba, rgb, sizeof(uchar) * 3);
                    rgba += 4;
                    rgb += 3;
                }
            }
            else
            {
                buffer = &glTFImage.image[0];
                bufferSize = glTFImage.image.size();
            }

            auto texture = Texture::Create();
            int minFilter = GetMinFilter(i);
            int magFilter = GetMinFilter(i);
            bool imageFormat = GetImageFormatGltf(i);
            texture->Init(glTFImage.width, glTFImage.height, imageFormat, buffer, minFilter, magFilter);
            #ifdef DEBUG
                texture->SetFilename(imageFilepath);
            #endif
            m_Images.push_back(texture);
        }
    }

    bool GltfBuilder::GetImageFormatGltf(uint const imageIndex)
    {
        for (uint i = 0; i < m_GltfModel.materials.size(); i++)
        {
            tinygltf::Material glTFMaterial = m_GltfModel.materials[i];

            if (static_cast<uint>(glTFMaterial.pbrMetallicRoughness.baseColorTexture.index) == imageIndex)
            {
                return Texture::USE_SRGB;
            }
            else if (static_cast<uint>(glTFMaterial.emissiveTexture.index) == imageIndex)
            {
                return Texture::USE_SRGB;
            }
            else if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end())
            {
                int diffuseTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
                tinygltf::Texture& diffuseTexture = m_GltfModel.textures[diffuseTextureIndex];
                if (static_cast<uint>(diffuseTexture.source) == imageIndex)
                {
                    return Texture::USE_SRGB;
                }
            }
        }
        return Texture::USE_UNORM;
    }

    void GltfBuilder::LoadMaterialsGltf()
    {
        m_Materials.clear();
        for (uint i = 0; i < m_GltfModel.materials.size(); i++)
        {
            tinygltf::Material glTFMaterial = m_GltfModel.materials[i];

            Material material{};
            material.m_Features = m_SkeletalAnimation;
            material.m_DiffuseColor = glm::vec3(0.5f, 0.5f, 1.0f);
            material.m_Roughness = glTFMaterial.pbrMetallicRoughness.roughnessFactor;
            material.m_Metallic  = glTFMaterial.pbrMetallicRoughness.metallicFactor;
            material.m_NormalMapIntensity = glTFMaterial.normalTexture.scale;
            material.m_EmissiveStrength = 0;
            if (glTFMaterial.emissiveFactor.size() == 3)
            {
                glm::vec3 emissiveFactor = glm::make_vec3(glTFMaterial.emissiveFactor.data());
                if (emissiveFactor != glm::vec3(0,0,0))
                {
                    material.m_EmissiveFactor = emissiveFactor;
                    material.m_EmissiveStrength = 1;
                }
            }
            if (glTFMaterial.emissiveTexture.index != Gltf::GLTF_NOT_USED)
            {
                int emissiveTextureIndex = glTFMaterial.emissiveTexture.index;
                tinygltf::Texture& emissiveTexture = m_GltfModel.textures[emissiveTextureIndex];
                material.m_EmissiveMapIndex = emissiveTexture.source;
                material.m_Features |= Material::HAS_EMISSIVE_MAP;
                material.m_EmissiveStrength = 1;
            }
            {
                auto it = glTFMaterial.extensions.find("KHR_materials_emissive_strength");
                if (it != glTFMaterial.extensions.end())
                {
                    auto extension = it->second;
                    if (extension.IsObject())
                    {
                        auto emissiveStrength = extension.Get("emissiveStrength");
                        if (emissiveStrength.IsReal())
                        {
                            material.m_EmissiveStrength = emissiveStrength.GetNumberAsDouble();
                        }
                    }
                }
            }

            if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end())
            {
                material.m_DiffuseColor = glm::make_vec3(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
            }
            if (glTFMaterial.pbrMetallicRoughness.baseColorTexture.index != Gltf::GLTF_NOT_USED)
            {
                int diffuseTextureIndex = glTFMaterial.pbrMetallicRoughness.baseColorTexture.index;
                tinygltf::Texture& diffuseTexture = m_GltfModel.textures[diffuseTextureIndex];
                material.m_DiffuseMapIndex = diffuseTexture.source;
                material.m_Features |= Material::HAS_DIFFUSE_MAP;
            }
            else if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end())
            {
                LOG_CORE_WARN("using legacy field values/baseColorTexture");
                int diffuseTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
                tinygltf::Texture& diffuseTexture = m_GltfModel.textures[diffuseTextureIndex];
                material.m_DiffuseMapIndex = diffuseTexture.source;
                material.m_Features |= Material::HAS_DIFFUSE_MAP;
            }
            if (glTFMaterial.normalTexture.index != Gltf::GLTF_NOT_USED)
            {
                int normalTextureIndex = glTFMaterial.normalTexture.index;
                tinygltf::Texture& normalTexture = m_GltfModel.textures[normalTextureIndex];
                material.m_NormalMapIndex = normalTexture.source;
                material.m_Features |= Material::HAS_NORMAL_MAP;
            }
            if (glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index != Gltf::GLTF_NOT_USED)
            {
                int mettalicRoughnessTextureIndex = glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
                tinygltf::Texture& mettalicRoughnessTexture = m_GltfModel.textures[mettalicRoughnessTextureIndex];
                material.m_RoughnessMettalicMapIndex = mettalicRoughnessTexture.source;
                material.m_Features |= Material::HAS_ROUGHNESS_METALLIC_MAP;
            }

            m_Materials.push_back(material);
        }
    }

    void GltfBuilder::LoadVertexDataGltf(uint const meshIndex)
    {
        // handle vertex data
        m_Vertices.clear();
        m_Indices.clear();

        m_PrimitivesNoMap.clear();
        m_PrimitivesEmissive.clear();
        m_PrimitivesDiffuseMap.clear();
        m_PrimitivesDiffuseSAMap.clear();
        m_PrimitivesEmissiveTexture.clear();
        m_PrimitivesDiffuseNormalMap.clear();
        m_PrimitivesDiffuseNormalSAMap.clear();
        m_PrimitivesDiffuseNormalRoughnessMetallicMap.clear();
        m_PrimitivesDiffuseNormalRoughnessMetallicSAMap.clear();

        for (const auto& glTFPrimitive : m_GltfModel.meshes[meshIndex].primitives)
        {

            PrimitiveTmp primitiveTmp;
            primitiveTmp.m_FirstVertex = static_cast<uint32_t>(m_Vertices.size());
            primitiveTmp.m_FirstIndex  = static_cast<uint32_t>(m_Indices.size());

            uint vertexCount = 0;
            uint indexCount  = 0;

            glm::vec3 diffuseColor = glm::vec3(0.5f, 0.5f, 1.0f);
            if (glTFPrimitive.material != Gltf::GLTF_NOT_USED)
            {
                if (!(static_cast<size_t>(glTFPrimitive.material) < m_Materials.size()))
                {
                    LOG_CORE_CRITICAL("LoadVertexDataGltf: glTFPrimitive.material must be less than m_Materials.size()");
                }
                diffuseColor = m_Materials[glTFPrimitive.material].m_DiffuseColor;
            }
            // Vertices
            {
                const float* positionBuffer  = nullptr;
                const float* normalsBuffer   = nullptr;
                const float* tangentsBuffer  = nullptr;
                const float* texCoordsBuffer = nullptr;
                const uint*  jointsBuffer    = nullptr;
                const float* weightsBuffer   = nullptr;

                int jointsBufferDataType = 0;

                // Get buffer data for vertex positions
                if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end())
                {
                    auto componentType = LoadAccessor<float>
                    (
                        m_GltfModel.accessors[glTFPrimitive.attributes.find("POSITION")->second],
                        positionBuffer,
                        &vertexCount
                    );
                    CORE_ASSERT(componentType == GL_FLOAT, "unexpected component type");
                }
                // Get buffer data for vertex normals
                if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end())
                {
                    auto componentType = LoadAccessor<float>
                    (
                        m_GltfModel.accessors[glTFPrimitive.attributes.find("NORMAL")->second],
                        normalsBuffer
                    );
                    CORE_ASSERT(componentType == GL_FLOAT, "unexpected component type");
                }
                #define USE_TINYGLTF_TANGENTS
                #ifdef USE_TINYGLTF_TANGENTS
                    // Get buffer data for vertex tangents
                    if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end())
                    {
                        auto componentType = LoadAccessor<float>
                        (
                            m_GltfModel.accessors[glTFPrimitive.attributes.find("TANGENT")->second],
                            tangentsBuffer
                        );
                        CORE_ASSERT(componentType == GL_FLOAT, "unexpected component type");
                    }
                #endif
                // Get buffer data for vertex texture coordinates
                // glTF supports multiple sets, we only load the first one
                if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end())
                {
                    auto componentType = LoadAccessor<float>
                    (
                        m_GltfModel.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second],
                        texCoordsBuffer
                    );
                    CORE_ASSERT(componentType == GL_FLOAT, "unexpected component type");
                }

                // Get buffer data for joints
                if (glTFPrimitive.attributes.find("JOINTS_0") != glTFPrimitive.attributes.end())
                {
                    jointsBufferDataType = LoadAccessor<uint>
                    (
                        m_GltfModel.accessors[glTFPrimitive.attributes.find("JOINTS_0")->second],
                        jointsBuffer
                    );
                    CORE_ASSERT((jointsBufferDataType == GL_BYTE) || (jointsBufferDataType == GL_UNSIGNED_BYTE), "unexpected component type");
                }
                // Get buffer data for joint weights
                if (glTFPrimitive.attributes.find("WEIGHTS_0") != glTFPrimitive.attributes.end())
                {
                    auto componentType = LoadAccessor<float>
                    (
                        m_GltfModel.accessors[glTFPrimitive.attributes.find("WEIGHTS_0")->second],
                        weightsBuffer
                    );
                    CORE_ASSERT(componentType == GL_FLOAT, "unexpected component type");
                }
                // Append data to model's vertex buffer
                for (size_t v = 0; v < vertexCount; v++)
                {
                    Vertex vertex{};
                    vertex.m_Amplification  = 1.0f;
                    auto position           = positionBuffer ? glm::make_vec3(&positionBuffer[v * 3]) : glm::vec3(0.0f);
                    vertex.m_Position       = glm::vec4(position.x, position.y, position.z, 1.0f);
                    vertex.m_Normal         = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));

                    glm::vec4 t             = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f);
                    vertex.m_Tangent = glm::vec3(t.x, t.y, t.z) * t.w;

                    vertex.m_UV             = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                    vertex.m_Color          = diffuseColor;
                    if (jointsBuffer && weightsBuffer)
                    {
                        switch (jointsBufferDataType)
                        {
                            case GL_BYTE:
                            case GL_UNSIGNED_BYTE:
                                vertex.m_JointIds = glm::ivec4(glm::make_vec4(&(reinterpret_cast<const int8_t*>(jointsBuffer)[v * 4])));
                                break;
                            case GL_SHORT:
                            case GL_UNSIGNED_SHORT:
                                vertex.m_JointIds = glm::ivec4(glm::make_vec4(&(reinterpret_cast<const int16_t*>(jointsBuffer)[v * 4])));
                                break;
                            case GL_INT:
                            case GL_UNSIGNED_INT:
                                vertex.m_JointIds = glm::ivec4(glm::make_vec4(&(reinterpret_cast<const int32_t*>(jointsBuffer)[v * 4])));
                                break;
                            default:
                                LOG_CORE_CRITICAL("data type of joints buffer not found");
                                break;
                        }

                        vertex.m_Weights        = glm::make_vec4(&weightsBuffer[v * 4]);
                    }
                    m_Vertices.push_back(vertex);
                }

                // calculate tangents
                if (!tangentsBuffer)
                {
                    CalculateTangents();
                }

            }
            // Indices
            {
                const uint32_t* buffer;
                uint count = 0;
                auto componentType = LoadAccessor<uint32_t>
                (
                    m_GltfModel.accessors[glTFPrimitive.indices],
                    buffer,
                    &count
                );

                indexCount += count;

                // glTF supports different component types of indices
                switch (componentType)
                {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: 
                    {
                        const uint32_t* buf = buffer;
                        for (size_t index = 0; index < count; index++)
                        {
                            m_Indices.push_back(buf[index]);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                    {
                        const uint16_t* buf = reinterpret_cast<const uint16_t*>(buffer);
                        for (size_t index = 0; index < count; index++) 
                        {
                            m_Indices.push_back(buf[index]);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                    {
                        const uint8_t* buf = reinterpret_cast<const uint8_t*>(buffer);
                        for (size_t index = 0; index < count; index++)
                        {
                            m_Indices.push_back(buf[index]);
                        }
                        break;
                    }
                    default:
                    {
                        CORE_ASSERT(false, "unexpected component type, index component type not supported!");
                        return;
                    }
                }
            }

            primitiveTmp.m_VertexCount = vertexCount;
            primitiveTmp.m_IndexCount  = indexCount;

            AssignMaterial(primitiveTmp, glTFPrimitive.material);
        }
    }

    void GltfBuilder::LoadTransformationMatrix(TransformComponent& transform, int const gltfNodeIndex)
    {
        auto& node = m_GltfModel.nodes[gltfNodeIndex];

        if (node.matrix.size() == 16)
        {
            transform.SetMat4(glm::make_mat4x4(node.matrix.data()));
        }
        else
        {
            if (node.rotation.size() == 4)
            {
                float x = node.rotation[0];
                float y = node.rotation[1];
                float z = node.rotation[2];
                float w = node.rotation[3];

                transform.SetRotation({w, x, y, z});

            }
            if (node.scale.size() == 3)
            {
                transform.SetScale({node.scale[0], node.scale[1], node.scale[2]});
            }
            if (node.translation.size() == 3)
            {
                transform.SetTranslation({node.translation[0], node.translation[1], node.translation[2]});
            }
        }
    }

    void GltfBuilder::AssignMaterial(const PrimitiveTmp& primitiveTmp, int const materialIndex)
    {
        if (materialIndex == Gltf::GLTF_NOT_USED)
        {
            PrimitiveNoMap primitiveNoMap{};
            primitiveNoMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveNoMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveNoMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveNoMap.m_VertexCount = primitiveTmp.m_VertexCount;

            primitiveNoMap.m_PbrNoMapMaterial.m_Roughness = 0.5f;
            primitiveNoMap.m_PbrNoMapMaterial.m_Metallic  = 0.1f;
            primitiveNoMap.m_PbrNoMapMaterial.m_Color     = glm::vec3(0.5f, 0.5f, 1.0f);

            m_PrimitivesNoMap.push_back(primitiveNoMap);

            return;
        }

        if (!(static_cast<size_t>(materialIndex) < m_Materials.size()))
        {
            LOG_CORE_CRITICAL("AssignMaterial: materialIndex must be less than m_Materials.size()");
        }

        auto& material = m_Materials[materialIndex];

        uint pbrFeatures = material.m_Features & (
                Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP | Material::HAS_ROUGHNESS_METALLIC_MAP | Material::HAS_SKELETAL_ANIMATION);
        if (pbrFeatures == Material::HAS_DIFFUSE_MAP)
        {
            PrimitiveDiffuseMap primitiveDiffuseMap{};
            primitiveDiffuseMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex = m_ImageOffset + material.m_DiffuseMapIndex;
            ASSERT(diffuseMapIndex < m_Images.size());

            VK_Model::CreateDescriptorSet(primitiveDiffuseMap.m_PbrDiffuseMaterial, m_Images[diffuseMapIndex]);
            primitiveDiffuseMap.m_PbrDiffuseMaterial.m_Roughness = material.m_Roughness;
            primitiveDiffuseMap.m_PbrDiffuseMaterial.m_Metallic  = material.m_Metallic;

            m_PrimitivesDiffuseMap.push_back(primitiveDiffuseMap);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_SKELETAL_ANIMATION))
        {
            PrimitiveDiffuseSAMap primitiveDiffuseSAMap{};
            primitiveDiffuseSAMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseSAMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseSAMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseSAMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseSAMapIndex = m_ImageOffset + material.m_DiffuseMapIndex;
            ASSERT(diffuseSAMapIndex < m_Images.size());

            VK_Model::CreateDescriptorSet(primitiveDiffuseSAMap.m_PbrDiffuseSAMaterial, m_Images[diffuseSAMapIndex], m_ShaderData);
            primitiveDiffuseSAMap.m_PbrDiffuseSAMaterial.m_Roughness = material.m_Roughness;
            primitiveDiffuseSAMap.m_PbrDiffuseSAMaterial.m_Metallic  = material.m_Metallic;

            m_PrimitivesDiffuseSAMap.push_back(primitiveDiffuseSAMap);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP))
        {
            PrimitiveDiffuseNormalMap primitiveDiffuseNormalMap{};
            primitiveDiffuseNormalMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseNormalMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseNormalMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseNormalMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex = m_ImageOffset + material.m_DiffuseMapIndex;
            uint normalMapIndex  = m_ImageOffset + material.m_NormalMapIndex;
            ASSERT(diffuseMapIndex < m_Images.size());
            ASSERT(normalMapIndex < m_Images.size());

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalMap.m_PbrDiffuseNormalMaterial, m_Images[diffuseMapIndex], m_Images[normalMapIndex]);
            primitiveDiffuseNormalMap.m_PbrDiffuseNormalMaterial.m_Roughness          = material.m_Roughness;
            primitiveDiffuseNormalMap.m_PbrDiffuseNormalMaterial.m_Metallic           = material.m_Metallic;
            primitiveDiffuseNormalMap.m_PbrDiffuseNormalMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalMap.push_back(primitiveDiffuseNormalMap);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP | Material::HAS_SKELETAL_ANIMATION))
        {
            PrimitiveDiffuseNormalSAMap primitiveDiffuseNormalSAMap{};
            primitiveDiffuseNormalSAMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseNormalSAMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseNormalSAMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseNormalSAMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex = m_ImageOffset + material.m_DiffuseMapIndex;
            uint normalMapIndex  = m_ImageOffset + material.m_NormalMapIndex;
            ASSERT(diffuseMapIndex < m_Images.size());
            ASSERT(normalMapIndex < m_Images.size());

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalSAMap.m_PbrDiffuseNormalSAMaterial,
                                          m_Images[diffuseMapIndex],
                                          m_Images[normalMapIndex],
                                          m_ShaderData);
            primitiveDiffuseNormalSAMap.m_PbrDiffuseNormalSAMaterial.m_Roughness          = material.m_Roughness;
            primitiveDiffuseNormalSAMap.m_PbrDiffuseNormalSAMaterial.m_Metallic           = material.m_Metallic;
            primitiveDiffuseNormalSAMap.m_PbrDiffuseNormalSAMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalSAMap.push_back(primitiveDiffuseNormalSAMap);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP | Material::HAS_ROUGHNESS_METALLIC_MAP))
        {
            PrimitiveDiffuseNormalRoughnessMetallicMap primitiveDiffuseNormalRoughnessMetallicMap{};
            primitiveDiffuseNormalRoughnessMetallicMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseNormalRoughnessMetallicMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseNormalRoughnessMetallicMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseNormalRoughnessMetallicMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex           = m_ImageOffset + material.m_DiffuseMapIndex;
            uint normalMapIndex            = m_ImageOffset + material.m_NormalMapIndex;
            uint roughnessMettalicMapIndex = m_ImageOffset + material.m_RoughnessMettalicMapIndex;

            ASSERT(diffuseMapIndex            < m_Images.size());
            ASSERT(normalMapIndex             < m_Images.size());
            ASSERT(roughnessMettalicMapIndex  < m_Images.size());

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalRoughnessMetallicMap.m_PbrDiffuseNormalRoughnessMetallicMaterial,
                                          m_Images[diffuseMapIndex], 
                                          m_Images[normalMapIndex], 
                                          m_Images[roughnessMettalicMapIndex]);
            primitiveDiffuseNormalRoughnessMetallicMap.m_PbrDiffuseNormalRoughnessMetallicMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalRoughnessMetallicMap.push_back(primitiveDiffuseNormalRoughnessMetallicMap);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP | Material::HAS_ROUGHNESS_METALLIC_MAP | Material::HAS_SKELETAL_ANIMATION))
        {
            PrimitiveDiffuseNormalRoughnessMetallicSAMap primitiveDiffuseNormalRoughnessMetallicSAMap{};
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex           = m_ImageOffset + material.m_DiffuseMapIndex;
            uint normalMapIndex            = m_ImageOffset + material.m_NormalMapIndex;
            uint roughnessMettalicMapIndex = m_ImageOffset + material.m_RoughnessMettalicMapIndex;

            ASSERT(diffuseMapIndex            < m_Images.size());
            ASSERT(normalMapIndex             < m_Images.size());
            ASSERT(roughnessMettalicMapIndex  < m_Images.size());

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalRoughnessMetallicSAMap.m_PbrDiffuseNormalRoughnessMetallicSAMaterial,
                                          m_Images[diffuseMapIndex], 
                                          m_Images[normalMapIndex], 
                                          m_Images[roughnessMettalicMapIndex],
                                          m_ShaderData);
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_PbrDiffuseNormalRoughnessMetallicSAMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalRoughnessMetallicSAMap.push_back(primitiveDiffuseNormalRoughnessMetallicSAMap);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_ROUGHNESS_METALLIC_MAP))
        {
            LOG_CORE_CRITICAL("material diffuseRoughnessMetallic not supported");
        }
        else if (pbrFeatures & (Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP | Material::HAS_ROUGHNESS_METALLIC_MAP))
        {
            PrimitiveDiffuseNormalRoughnessMetallicMap primitiveDiffuseNormalRoughnessMetallicMap{};
            primitiveDiffuseNormalRoughnessMetallicMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseNormalRoughnessMetallicMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseNormalRoughnessMetallicMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseNormalRoughnessMetallicMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex           = m_ImageOffset + material.m_DiffuseMapIndex;
            uint normalMapIndex            = m_ImageOffset + material.m_NormalMapIndex;
            uint roughnessMettalicMapIndex = m_ImageOffset + material.m_RoughnessMettalicMapIndex;
            ASSERT(diffuseMapIndex            < m_Images.size());
            ASSERT(normalMapIndex             < m_Images.size());
            ASSERT(roughnessMettalicMapIndex  < m_Images.size());

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalRoughnessMetallicMap.m_PbrDiffuseNormalRoughnessMetallicMaterial, 
                                            m_Images[diffuseMapIndex], m_Images[normalMapIndex], m_Images[roughnessMettalicMapIndex]);
            primitiveDiffuseNormalRoughnessMetallicMap.m_PbrDiffuseNormalRoughnessMetallicMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalRoughnessMetallicMap.push_back(primitiveDiffuseNormalRoughnessMetallicMap);
        }
        else if (pbrFeatures & Material::HAS_DIFFUSE_MAP)
        {
            PrimitiveDiffuseMap primitiveDiffuseMap{};
            primitiveDiffuseMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex = m_ImageOffset + material.m_DiffuseMapIndex;
            ASSERT(diffuseMapIndex < m_Images.size());

            VK_Model::CreateDescriptorSet(primitiveDiffuseMap.m_PbrDiffuseMaterial, m_Images[diffuseMapIndex]);
            primitiveDiffuseMap.m_PbrDiffuseMaterial.m_Roughness = material.m_Roughness;
            primitiveDiffuseMap.m_PbrDiffuseMaterial.m_Metallic  = material.m_Metallic;

            m_PrimitivesDiffuseMap.push_back(primitiveDiffuseMap);
        }
        else
        {
            PrimitiveNoMap primitiveNoMap{};
            primitiveNoMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveNoMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveNoMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveNoMap.m_VertexCount = primitiveTmp.m_VertexCount;

            primitiveNoMap.m_PbrNoMapMaterial.m_Roughness = material.m_Roughness;
            primitiveNoMap.m_PbrNoMapMaterial.m_Metallic  = material.m_Metallic;
            primitiveNoMap.m_PbrNoMapMaterial.m_Color     = material.m_DiffuseColor;

            m_PrimitivesNoMap.push_back(primitiveNoMap);
        }

        // emissive materials
        if (material.m_EmissiveStrength != 0)
        {
            // emissive texture
            if (material.m_Features & Material::HAS_EMISSIVE_MAP)
            {
                PrimitiveEmissiveTexture primitiveEmissiveTexture{};
                primitiveEmissiveTexture.m_FirstIndex  = primitiveTmp.m_FirstIndex;
                primitiveEmissiveTexture.m_FirstVertex = primitiveTmp.m_FirstVertex;
                primitiveEmissiveTexture.m_IndexCount  = primitiveTmp.m_IndexCount;
                primitiveEmissiveTexture.m_VertexCount = primitiveTmp.m_VertexCount;

                uint emissiveMapIndex = m_ImageOffset + material.m_EmissiveMapIndex;
                ASSERT(emissiveMapIndex < m_Images.size());

                VK_Model::CreateDescriptorSet(primitiveEmissiveTexture.m_PbrEmissiveTextureMaterial, m_Images[emissiveMapIndex]);

                primitiveEmissiveTexture.m_PbrEmissiveTextureMaterial.m_Roughness = material.m_Roughness;
                primitiveEmissiveTexture.m_PbrEmissiveTextureMaterial.m_Metallic  = material.m_Metallic;
                primitiveEmissiveTexture.m_PbrEmissiveTextureMaterial.m_EmissiveStrength  = material.m_EmissiveStrength;

                m_PrimitivesEmissiveTexture.push_back(primitiveEmissiveTexture);
            }
            else // emissive vertex color
            {
                PrimitiveEmissive primitiveEmissive{};
                primitiveEmissive.m_FirstIndex  = primitiveTmp.m_FirstIndex;
                primitiveEmissive.m_FirstVertex = primitiveTmp.m_FirstVertex;
                primitiveEmissive.m_IndexCount  = primitiveTmp.m_IndexCount;
                primitiveEmissive.m_VertexCount = primitiveTmp.m_VertexCount;

                primitiveEmissive.m_PbrEmissiveMaterial.m_Roughness = material.m_Roughness;
                primitiveEmissive.m_PbrEmissiveMaterial.m_Metallic  = material.m_Metallic;
                primitiveEmissive.m_PbrEmissiveMaterial.m_EmissiveFactor = material.m_EmissiveFactor;
                primitiveEmissive.m_PbrEmissiveMaterial.m_EmissiveStrength  = material.m_EmissiveStrength;

                m_PrimitivesEmissive.push_back(primitiveEmissive);
            }
        }
    }

    void GltfBuilder::CalculateTangents()
    {
        if (m_Indices.size())
        {
            CalculateTangentsFromIndexBuffer(m_Indices);
        }
        else
        {
            uint vertexCount = m_Vertices.size();
            if (vertexCount)
            {
                std::vector<uint> indices;
                indices.resize(vertexCount);
                for (uint i = 0; i < vertexCount; i++)
                {
                    indices[i] = i;
                }
                CalculateTangentsFromIndexBuffer(indices);
            }
        }
    }

    void GltfBuilder::CalculateTangentsFromIndexBuffer(const std::vector<uint>& indices)
    {
        uint cnt = 0;
        uint vertexIndex1 = 0;
        uint vertexIndex2 = 0;
        uint vertexIndex3 = 0;
        glm::vec3 position1 = glm::vec3{0.0f};
        glm::vec3 position2 = glm::vec3{0.0f};
        glm::vec3 position3 = glm::vec3{0.0f};
        glm::vec2 uv1 = glm::vec2{0.0f};
        glm::vec2 uv2 = glm::vec2{0.0f};
        glm::vec2 uv3 = glm::vec2{0.0f};

        for (uint index : indices)
        {
            auto& vertex = m_Vertices[index];

            switch (cnt)
            {
                case 0:
                    position1 = vertex.m_Position;
                    uv1  = vertex.m_UV;
                    vertexIndex1 = index;
                    break;
                case 1:
                    position2 = vertex.m_Position;
                    uv2  = vertex.m_UV;
                    vertexIndex2 = index;
                    break;
                case 2:
                    position3 = vertex.m_Position;
                    uv3  = vertex.m_UV;
                    vertexIndex3 = index;

                    glm::vec3 edge1 = position2 - position1;
                    glm::vec3 edge2 = position3 - position1;
                    glm::vec2 deltaUV1 = uv2 - uv1;
                    glm::vec2 deltaUV2 = uv3 - uv1;

                    float dU1 = deltaUV1.x;
                    float dU2 = deltaUV2.x;
                    float dV1 = deltaUV1.y;
                    float dV2 = deltaUV2.y;
                    float E1x = edge1.x;
                    float E2x = edge2.x;
                    float E1y = edge1.y;
                    float E2y = edge2.y;
                    float E1z = edge1.z;
                    float E2z = edge2.z;

                    float factor;
                    if ((dU1 * dV2 - dU2 * dV1) > std::numeric_limits<float>::epsilon())
                    {
                        factor = 1.0f / (dU1 * dV2 - dU2 * dV1);
                    }
                    else
                    {
                        factor = 100000.0f;
                    }

                    glm::vec3 tangent;

                    tangent.x = factor * (dV2 * E1x - dV1 * E2x);
                    tangent.y = factor * (dV2 * E1y - dV1 * E2y);
                    tangent.z = factor * (dV2 * E1z - dV1 * E2z);
                    if (tangent.x==0.0f && tangent.y==0.0f && tangent.z==0.0f) tangent = glm::vec3(1.0f, 0.0f, 0.0f);

                    m_Vertices[vertexIndex1].m_Tangent = tangent;
                    m_Vertices[vertexIndex2].m_Tangent = tangent;
                    m_Vertices[vertexIndex3].m_Tangent = tangent;

                    break;
            }
            cnt = (cnt + 1) % 3;
        }
    }
}
