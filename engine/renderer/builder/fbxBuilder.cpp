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
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"

#include "core.h"
#include "renderer/builder/fbxBuilder.h"
#include "auxiliary/file.h"

#include "VKmodel.h"

namespace GfxRenderEngine
{

    FbxBuilder::FbxBuilder(const std::string& filepath, Scene& scene)
        : m_Filepath{filepath}, m_SkeletalAnimation{0}, m_Registry{scene.GetRegistry()},
          m_SceneGraph{scene.GetSceneGraph()}, m_Dictionary{scene.GetDictionary()},
          m_InstanceCount{0}, m_InstanceIndex{0}, m_FbxScene{nullptr},
          m_FbxNoBuiltInTangents{false}
    {
        m_Basepath = EngineCore::GetPathWithoutFilename(filepath);
    }

    bool FbxBuilder::LoadFbx(uint const instanceCount, int const sceneID)
    {
        Assimp::Importer importer;

        m_FbxScene = importer.ReadFile( m_Filepath,
            aiProcess_CalcTangentSpace       |
            aiProcess_Triangulate            |
            aiProcess_JoinIdenticalVertices  |
            aiProcess_SortByPType);

        if (m_FbxScene == nullptr)
        {
            LOG_CORE_CRITICAL("LoadFbx error: {0}", m_Filepath);
            return Fbx::FBX_LOAD_FAILURE;
        }

        if (!m_FbxScene->mNumMeshes)
        {
            LOG_CORE_CRITICAL("LoadFbx: no meshes found in {0}", m_Filepath);
            return Fbx::FBX_LOAD_FAILURE;
        }

        if (sceneID > Fbx::FBX_NOT_USED) // a scene ID was provided
        {
            LOG_CORE_WARN("LoadFbx: scene ID for fbx not supported (in file {0})", m_Filepath);
        }

        LoadSkeletonsFbx();
        LoadMaterialsFbx();

        // PASS 1
        // mark Fbx nodes to receive a game object ID if they have a mesh or any child has
        // --> create array of flags for all nodes of the Fbx file
        MarkNode(m_FbxScene->mRootNode);

        // PASS 2 (for all instances)
        m_InstanceCount = instanceCount;
        for(m_InstanceIndex = 0; m_InstanceIndex < m_InstanceCount; ++m_InstanceIndex)
        {
            // create group game object(s) for all instances to apply transform from JSON file to
            auto entity = m_Registry.create();

            std::string name = EngineCore::GetFilenameWithoutPathAndExtension(m_Filepath);
            auto shortName = name + "::" + std::to_string(m_InstanceIndex) + "::root";
            auto longName = m_Filepath + "::" + std::to_string(m_InstanceIndex) + "::root";
            uint groupNode = m_SceneGraph.CreateNode(entity, shortName, longName, m_Dictionary);
            m_SceneGraph.GetRoot().AddChild(groupNode);

            {
                TransformComponent transform{};
                m_Registry.emplace<TransformComponent>(entity, transform);
            }

            uint hasMeshIndex = Fbx::FBX_ROOT_NODE;
            ProcessNode(m_FbxScene->mRootNode, groupNode, hasMeshIndex);
        }
        return Fbx::FBX_LOAD_SUCCESS;
    }

    bool FbxBuilder::MarkNode(const aiNode* fbxNodePtr)
    {
        // each recursive call of this function marks a node in "m_HasMesh" if itself or a child has a mesh

        // does this Fbx node have a mesh?
        bool localHasMesh = false;

        // check if at least one mesh is usable, i.e. is a triangle mesh
        for (uint nodeMeshIndex = 0; nodeMeshIndex < fbxNodePtr->mNumMeshes; ++nodeMeshIndex)
        {
            // retrieve index for global/scene mesh array
            uint sceneMeshIndex = fbxNodePtr->mMeshes[nodeMeshIndex];
            aiMesh* mesh = m_FbxScene->mMeshes[sceneMeshIndex];

            // check if triangle mesh
            if (mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)
            {
                localHasMesh = true;
                break;
            }
        }

        int hasMeshIndex = m_HasMesh.size();
        m_HasMesh.push_back(localHasMesh); //reserve space in m_HasMesh, so that ProcessNode can find it

        // do any of the child nodes have a mesh?
        uint childNodeCount = fbxNodePtr->mNumChildren;
        for (uint childNodeIndex = 0; childNodeIndex < childNodeCount; ++childNodeIndex)
        {
            bool childHasMesh = MarkNode(fbxNodePtr->mChildren[childNodeIndex]);
            localHasMesh = localHasMesh || childHasMesh;
        }
        m_HasMesh[hasMeshIndex] = localHasMesh;
        return localHasMesh;
    }

    void FbxBuilder::ProcessNode(const aiNode* fbxNodePtr, uint const parentNode, uint& hasMeshIndex)
    {
        std::string nodeName = std::string(fbxNodePtr->mName.C_Str());
        uint currentNode = parentNode;

        if (m_HasMesh[hasMeshIndex]) 
        {
            if (fbxNodePtr->mNumMeshes)
            {
                currentNode = CreateGameObject(fbxNodePtr, parentNode);
            }
            else // one or more children have a mesh, but not this one --> create group node
            {
                // create game object and transform component
                auto entity = m_Registry.create();
                {
                    TransformComponent transform(LoadTransformationMatrix(fbxNodePtr));
                    if (fbxNodePtr->mParent == m_FbxScene->mRootNode)
                    {
                        auto scale = transform.GetScale();
                        transform.SetScale({scale.x/100.0f, scale.y/100.0f, scale.z/100.0f});
                        auto translation = transform.GetTranslation();
                        transform.SetTranslation({translation.x/100.0f, translation.y/100.0f, translation.z/100.0f});
                    }
                    m_Registry.emplace<TransformComponent>(entity, transform);
                }

                // create scene graph node and add to parent
                auto shortName = "::" + std::to_string(m_InstanceIndex) + "::" + nodeName;
                auto longName = m_Filepath + "::" + std::to_string(m_InstanceIndex) + "::" + nodeName;
                currentNode = m_SceneGraph.CreateNode(entity, shortName, longName, m_Dictionary);
                m_SceneGraph.GetNode(parentNode).AddChild(currentNode);
            }
        }
        ++hasMeshIndex;

        uint childNodeCount = fbxNodePtr->mNumChildren;
        for (uint childNodeIndex = 0; childNodeIndex < childNodeCount; ++childNodeIndex)
        {
            ProcessNode(fbxNodePtr->mChildren[childNodeIndex], currentNode, hasMeshIndex);
        }
    }

    uint FbxBuilder::CreateGameObject(const aiNode* fbxNodePtr, uint const parentNode)
    {
        std::string nodeName = std::string(fbxNodePtr->mName.C_Str());
        LoadVertexDataFbx(fbxNodePtr);

        LOG_CORE_INFO("Vertex count: {0}, Index count: {1} (file: {2}, node: {3})", m_Vertices.size(), m_Indices.size(), m_Filepath, nodeName);

        auto model = Engine::m_Engine->LoadModel(*this);
        auto entity = m_Registry.create();
        auto shortName = EngineCore::GetFilenameWithoutPathAndExtension(m_Filepath) + "::" + std::to_string(m_InstanceIndex) + "::" + nodeName;
        auto longName = m_Filepath + "::" + std::to_string(m_InstanceIndex) + "::" + nodeName;

        uint newNode = m_SceneGraph.CreateNode(entity, shortName, longName, m_Dictionary);
        m_SceneGraph.GetNode(parentNode).AddChild(newNode);

        { // mesh
            MeshComponent mesh{nodeName, model};
            m_Registry.emplace<MeshComponent>(entity, mesh);
        }

        { // transform
            TransformComponent transform(LoadTransformationMatrix(fbxNodePtr));
            if (fbxNodePtr->mParent == m_FbxScene->mRootNode)
            {
                auto scale = transform.GetScale();
                transform.SetScale({scale.x/100.0f, scale.y/100.0f, scale.z/100.0f});
                auto translation = transform.GetTranslation();
                transform.SetTranslation({translation.x/100.0f, translation.y/100.0f, translation.z/100.0f});
            }
            m_Registry.emplace<TransformComponent>(entity, transform);
        }

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

            PbrDiffuseNormalRoughnessMetallic2Tag pbrDiffuseNormalRoughnessMetallic2Tag;
            m_Registry.emplace<PbrDiffuseNormalRoughnessMetallic2Tag>(entity, pbrDiffuseNormalRoughnessMetallic2Tag);
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

    bool FbxBuilder::LoadImageFbx(std::string& filepath, uint& mapIndex, bool useSRGB)
    {
        std::shared_ptr<Texture> texture;
        bool loadSucess = false;

        if (EngineCore::FileExists(filepath) && !EngineCore::IsDirectory(filepath))
        {
            texture = Texture::Create();
            loadSucess = texture->Init(filepath, useSRGB);
        }
        else if (EngineCore::FileExists(m_Basepath+filepath) && !EngineCore::IsDirectory(m_Basepath+filepath))
        {
            texture = Texture::Create();
            loadSucess = texture->Init(m_Basepath+filepath, useSRGB);
        }
        else
        {
            LOG_CORE_CRITICAL("bool FbxBuilder::LoadImageFbx(): file '{0}' not found", filepath);
        }

        if (loadSucess)
        {
            #ifdef DEBUG
                texture->SetFilename(filepath);
            #endif
            mapIndex = m_Images.size();
            m_Images.push_back(texture);
        }
        return loadSucess;
    }

    bool FbxBuilder::LoadMap(const aiMaterial* fbxMaterial, aiTextureType textureType, Material& engineMaterial)
    {
        bool loadedSuccessfully = false;
        uint textureCount = fbxMaterial->GetTextureCount(textureType);
        if (textureCount)
        {
            aiString aiFilepath;
            auto getTexture = fbxMaterial->GetTexture(textureType, 0 /* first map*/, &aiFilepath);
            std::string fbxFilepath(aiFilepath.C_Str());
            std::string filepath(fbxFilepath);

            if (getTexture == aiReturn_SUCCESS)
            {
                bool loadImageFbx = false;
                switch (textureType)
                {
                    case aiTextureType_DIFFUSE:
                    {
                        loadImageFbx = LoadImageFbx(filepath, engineMaterial.m_DiffuseMapIndex, Texture::USE_SRGB);
                        break;
                    }
                    case aiTextureType_NORMALS:
                    {
                        loadImageFbx = LoadImageFbx(filepath, engineMaterial.m_NormalMapIndex, Texture::USE_UNORM);
                        break;
                    }
                    case aiTextureType_SHININESS: // assimp XD
                    {
                        loadImageFbx = LoadImageFbx(filepath, engineMaterial.m_RoughnessMapIndex, Texture::USE_UNORM);
                        break;
                    }
                    case aiTextureType_METALNESS:
                    {
                        loadImageFbx = LoadImageFbx(filepath, engineMaterial.m_MetallicMapIndex, Texture::USE_UNORM);
                        break;
                    }
                    case aiTextureType_EMISSIVE:
                    {
                        loadImageFbx = LoadImageFbx(filepath, engineMaterial.m_EmissiveMapIndex, Texture::USE_SRGB);
                        break;
                    }
                    default:
                    {
                        CORE_ASSERT(false, "texture type not recognized");
                    }
                }
                if (loadImageFbx)
                {
                    loadedSuccessfully = true;
                    switch (textureType)
                    {
                        case aiTextureType_DIFFUSE:
                        {
                            engineMaterial.m_Features |= Material::HAS_DIFFUSE_MAP;
                            break;
                        }
                        case aiTextureType_NORMALS:
                        {
                            engineMaterial.m_Features |= Material::HAS_NORMAL_MAP;
                            break;
                        }
                        case aiTextureType_SHININESS: // assimp XD
                        {
                            engineMaterial.m_Features |= Material::HAS_ROUGHNESS_MAP;
                            break;
                        }
                        case aiTextureType_METALNESS:
                        {
                            engineMaterial.m_Features |= Material::HAS_METALLIC_MAP;
                            break;
                        }
                        case aiTextureType_EMISSIVE:
                        {
                            engineMaterial.m_Features |= Material::HAS_EMISSIVE_MAP;
                            break;
                        }
                        default:
                        {
                            // checked above
                            break;
                        }
                    }
                }
            }
        }
        return loadedSuccessfully;
    }

    void FbxBuilder::LoadProperties(const aiMaterial* fbxMaterial, Material& engineMaterial)
    {
        {  // diffuse
            aiColor3D diffuseColor;
            if (fbxMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == aiReturn_SUCCESS)
            {
                engineMaterial.m_DiffuseColor.r  = diffuseColor.r;
                engineMaterial.m_DiffuseColor.g  = diffuseColor.g;
                engineMaterial.m_DiffuseColor.b  = diffuseColor.b;
            }
            else
            {
                engineMaterial.m_DiffuseColor = glm::vec3(0.5f, 0.5f, 1.0f);
            }
        }
        {  // roughness
            float roughnessFactor;
            if (fbxMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor) == aiReturn_SUCCESS)
            {
                engineMaterial.m_Roughness  = roughnessFactor;
            }
            else
            {
                engineMaterial.m_Roughness = 0.1f;
            }
        }

        {  // metallic
            float metallicFactor;
            if (fbxMaterial->Get(AI_MATKEY_REFLECTIVITY, metallicFactor) == aiReturn_SUCCESS)
            {
                engineMaterial.m_Metallic  = metallicFactor;
            }
            else if (fbxMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor) == aiReturn_SUCCESS)
            {
                engineMaterial.m_Metallic  = metallicFactor;
            }
            else
            {
                engineMaterial.m_Metallic  = 0.886f;
            }
        }

        engineMaterial.m_NormalMapIntensity = 1.0f;
    }

    void FbxBuilder::LoadMaterialsFbx()
    {
        m_Materials.clear();
        uint numMaterials = m_FbxScene->mNumMaterials;
        for (uint fbxMaterialIndex = 0; fbxMaterialIndex < numMaterials; ++fbxMaterialIndex)
        {
            const aiMaterial* fbxMaterial = m_FbxScene->mMaterials[fbxMaterialIndex];
            //PrintMaps(fbxMaterial);

            Material engineMaterial{};
            engineMaterial.m_Features = m_SkeletalAnimation;

            LoadProperties(fbxMaterial, engineMaterial);

            LoadMap(fbxMaterial, aiTextureType_DIFFUSE, engineMaterial);
            LoadMap(fbxMaterial, aiTextureType_NORMALS, engineMaterial);
            LoadMap(fbxMaterial, aiTextureType_SHININESS, engineMaterial);
            LoadMap(fbxMaterial, aiTextureType_METALNESS, engineMaterial);

            if (LoadMap(fbxMaterial, aiTextureType_EMISSIVE, engineMaterial))
            {
                engineMaterial.m_EmissiveStrength = 0.35f;
            }
            else
            {
                engineMaterial.m_EmissiveStrength = 0.0f;
            }

            m_Materials.push_back(engineMaterial);
        }
    }

    void FbxBuilder::LoadVertexDataFbx(const aiNode* fbxNodePtr, int vertexColorSet, uint uvSet)
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

        m_FbxNoBuiltInTangents = false;
        uint numMeshes = fbxNodePtr->mNumMeshes;
        if (numMeshes)
        {
            for (uint meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
            {
                LoadVertexDataFbx(fbxNodePtr, fbxNodePtr->mMeshes[meshIndex], vertexColorSet, uvSet);
            }
            if (m_FbxNoBuiltInTangents) // at least one mesh did not have tangents
            {
                LOG_CORE_CRITICAL("no tangents in fbx file found, calculating tangents manually");
                CalculateTangents();
            }
        }
    }

    void FbxBuilder::LoadVertexDataFbx(const aiNode* fbxNodePtr, uint const meshIndex, int vertexColorSet, uint uvSet)
    {
        const aiMesh* mesh = m_FbxScene->mMeshes[meshIndex];

        // only triangle mesh supported
        if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE))
        {
            LOG_CORE_CRITICAL("FbxBuilder::LoadVertexDataFbx: only triangle meshes are supported");
            return;
        }

        const uint numVertices = mesh->mNumVertices;
        const uint numFaces = mesh->mNumFaces;
        const uint numIndices = numFaces * 3; // 3 indices per triangle a.k.a face

        size_t numVerticesBefore = m_Vertices.size();
        size_t numIndicesBefore = m_Indices.size();
        m_Vertices.resize(numVerticesBefore + numVertices);
        m_Indices.resize(numIndicesBefore + numIndices);

        PrimitiveTmp primitiveTmp;
        primitiveTmp.m_FirstVertex = numVerticesBefore;
        primitiveTmp.m_FirstIndex  = numIndicesBefore;
        primitiveTmp.m_VertexCount = numVertices;
        primitiveTmp.m_IndexCount  = numIndices;

        bool hasPosition = mesh->HasPositions(); 
        bool hasNormals = mesh->HasNormals();
        bool hasTangents = mesh->HasTangentsAndBitangents();
        bool hasUVs = mesh->HasTextureCoords(uvSet);
        bool hasColors = mesh->HasVertexColors(vertexColorSet);

        m_FbxNoBuiltInTangents = m_FbxNoBuiltInTangents || (!hasTangents);

        for (uint vertexIndex = numVerticesBefore; vertexIndex < numVertices; ++vertexIndex)
        {
            Vertex& vertex = m_Vertices[vertexIndex];
            vertex.m_Amplification  = 1.0f;

            if (hasPosition)
            { // position (guaranteed to always be there)
                aiVector3D& positionFbx = mesh->mVertices[vertexIndex];
                vertex.m_Position = glm::vec3(positionFbx.x, positionFbx.y, positionFbx.z);
            }

            if (hasNormals) // normals
            {
                aiVector3D& normalFbx = mesh->mNormals[vertexIndex];
                vertex.m_Normal = glm::normalize(glm::vec3(normalFbx.x, normalFbx.z, -normalFbx.y));
            }

            if (hasTangents) // tangents
            {
                aiVector3D& tangentFbx = mesh->mTangents[vertexIndex];
                vertex.m_Tangent = glm::vec3(tangentFbx.x, tangentFbx.z, -tangentFbx.y);
            }

            if (hasUVs) // uv coordinates
            {
                aiVector3D& uvFbx = mesh->mTextureCoords[uvSet][vertexIndex];
                vertex.m_UV = glm::vec2(uvFbx.x, uvFbx.y);
            }

            if (hasColors) // vertex colors
            {
                aiColor4D& colorFbx = mesh->mColors[vertexColorSet][vertexIndex];
                vertex.m_Color = glm::vec3(colorFbx.r, colorFbx.g, colorFbx.b);
            }
            else
            {
                uint materialIndex = mesh->mMaterialIndex;
                vertex.m_Color = m_Materials[materialIndex].m_DiffuseColor;
            }
        }

        // Indices
        {
            uint index = numIndicesBefore;
            for (uint faceIndex = 0; faceIndex < numFaces; ++faceIndex)
            {
                const aiFace& face = mesh->mFaces[faceIndex];
                m_Indices[index + 0] = face.mIndices[0];
                m_Indices[index + 1] = face.mIndices[1];
                m_Indices[index + 2] = face.mIndices[2];
                index += 3;
            }
        }

        AssignMaterial(primitiveTmp, mesh->mMaterialIndex);
    }

    glm::mat4 FbxBuilder::LoadTransformationMatrix(const aiNode* fbxNodePtr)
    {
        aiMatrix4x4 t = fbxNodePtr->mTransformation;
        glm::mat4 m;

        m[0][0] = (float)t.a1; m[0][1] = (float)t.b1;  m[0][2] = (float)t.c1; m[0][3] = (float)t.d1;
        m[1][0] = (float)t.a2; m[1][1] = (float)t.b2;  m[1][2] = (float)t.c2; m[1][3] = (float)t.d2;
        m[2][0] = (float)t.a3; m[2][1] = (float)t.b3;  m[2][2] = (float)t.c3; m[2][3] = (float)t.d3;
        m[3][0] = (float)t.a4; m[3][1] = (float)t.b4;  m[3][2] = (float)t.c4; m[3][3] = (float)t.d4;

        return m;
    }

    void FbxBuilder::AssignMaterial(const PrimitiveTmp& primitiveTmp, uint const materialIndex)
    {

        if (!m_FbxScene->mNumMaterials)
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
            LOG_CORE_INFO("material assigned: material index {0}, PbrNoMap (no material found)", materialIndex);

            return;
        }

        if (!(static_cast<size_t>(materialIndex) < m_Materials.size()))
        {
            LOG_CORE_CRITICAL("AssignMaterial: materialIndex must be less than m_Materials.size()");
        }

        auto& material = m_Materials[materialIndex];

        uint pbrFeatures = material.m_Features & (
                Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP | Material::HAS_ROUGHNESS_MAP | 
                Material::HAS_METALLIC_MAP | Material::HAS_ROUGHNESS_METALLIC_MAP | 
                Material::HAS_SKELETAL_ANIMATION);
        if (pbrFeatures == Material::HAS_DIFFUSE_MAP)
        {
            PrimitiveDiffuseMap primitiveDiffuseMap{};
            primitiveDiffuseMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex = material.m_DiffuseMapIndex;
            CORE_ASSERT(diffuseMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: diffuseMapIndex < m_Images.size()");

            VK_Model::CreateDescriptorSet(primitiveDiffuseMap.m_PbrDiffuseMaterial, m_Images[diffuseMapIndex]);
            primitiveDiffuseMap.m_PbrDiffuseMaterial.m_Roughness = material.m_Roughness;
            primitiveDiffuseMap.m_PbrDiffuseMaterial.m_Metallic  = material.m_Metallic;

            m_PrimitivesDiffuseMap.push_back(primitiveDiffuseMap);
            LOG_CORE_INFO("material assigned: material index {0}, PbrDiffuse, features: 0x{1:x}", materialIndex, material.m_Features);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_SKELETAL_ANIMATION))
        {
            PrimitiveDiffuseSAMap primitiveDiffuseSAMap{};
            primitiveDiffuseSAMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseSAMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseSAMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseSAMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseSAMapIndex = material.m_DiffuseMapIndex;
            CORE_ASSERT(diffuseSAMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: diffuseSAMapIndex < m_Images.size()");

            VK_Model::CreateDescriptorSet(primitiveDiffuseSAMap.m_PbrDiffuseSAMaterial, m_Images[diffuseSAMapIndex], m_ShaderData);
            primitiveDiffuseSAMap.m_PbrDiffuseSAMaterial.m_Roughness = material.m_Roughness;
            primitiveDiffuseSAMap.m_PbrDiffuseSAMaterial.m_Metallic  = material.m_Metallic;

            m_PrimitivesDiffuseSAMap.push_back(primitiveDiffuseSAMap);
            LOG_CORE_INFO("material assigned: material index {0}, PbrDiffuseSA, features: 0x{1:x}", materialIndex, material.m_Features);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP))
        {
            PrimitiveDiffuseNormalMap primitiveDiffuseNormalMap{};
            primitiveDiffuseNormalMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseNormalMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseNormalMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseNormalMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex = material.m_DiffuseMapIndex;
            uint normalMapIndex  = material.m_NormalMapIndex;
            CORE_ASSERT(diffuseMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: diffuseMapIndex < m_Images.size()");
            CORE_ASSERT(normalMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: normalMapIndex < m_Images.size()");

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalMap.m_PbrDiffuseNormalMaterial, m_Images[diffuseMapIndex], m_Images[normalMapIndex]);
            primitiveDiffuseNormalMap.m_PbrDiffuseNormalMaterial.m_Roughness          = material.m_Roughness;
            primitiveDiffuseNormalMap.m_PbrDiffuseNormalMaterial.m_Metallic           = material.m_Metallic;
            primitiveDiffuseNormalMap.m_PbrDiffuseNormalMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalMap.push_back(primitiveDiffuseNormalMap);
            LOG_CORE_INFO("material assigned: material index {0}, PbrDiffuseNormal, features: 0x{1:x}", materialIndex, material.m_Features);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP | Material::HAS_SKELETAL_ANIMATION))
        {
            PrimitiveDiffuseNormalSAMap primitiveDiffuseNormalSAMap{};
            primitiveDiffuseNormalSAMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseNormalSAMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseNormalSAMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseNormalSAMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex = material.m_DiffuseMapIndex;
            uint normalMapIndex  = material.m_NormalMapIndex;
            CORE_ASSERT(diffuseMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: diffuseMapIndex < m_Images.size()");
            CORE_ASSERT(normalMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: normalMapIndex < m_Images.size()");

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalSAMap.m_PbrDiffuseNormalSAMaterial,
                                          m_Images[diffuseMapIndex],
                                          m_Images[normalMapIndex],
                                          m_ShaderData);
            primitiveDiffuseNormalSAMap.m_PbrDiffuseNormalSAMaterial.m_Roughness          = material.m_Roughness;
            primitiveDiffuseNormalSAMap.m_PbrDiffuseNormalSAMaterial.m_Metallic           = material.m_Metallic;
            primitiveDiffuseNormalSAMap.m_PbrDiffuseNormalSAMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalSAMap.push_back(primitiveDiffuseNormalSAMap);
            LOG_CORE_INFO("material assigned: material index {0}, PbrDiffuseNormalSA, features: 0x{1:x}", materialIndex, material.m_Features);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP | Material::HAS_ROUGHNESS_MAP | Material::HAS_METALLIC_MAP))
        {
            PrimitiveDiffuseNormalRoughnessMetallicMap primitiveDiffuseNormalRoughnessMetallicMap{};
            primitiveDiffuseNormalRoughnessMetallicMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseNormalRoughnessMetallicMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseNormalRoughnessMetallicMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseNormalRoughnessMetallicMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex   = material.m_DiffuseMapIndex;
            uint normalMapIndex    = material.m_NormalMapIndex;
            uint roughnessMapIndex = material.m_RoughnessMapIndex;
            uint metallicMapIndex  = material.m_MetallicMapIndex;

            CORE_ASSERT(diffuseMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: diffuseMapIndex < m_Images.size()");
            CORE_ASSERT(normalMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: normalMapIndex < m_Images.size()");
            CORE_ASSERT(roughnessMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: roughnessMapIndex < m_Images.size()");
            CORE_ASSERT(metallicMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: metallicMapIndex < m_Images.size()");

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalRoughnessMetallicMap.m_PbrDiffuseNormalRoughnessMetallicMaterial,
                                          m_Images[diffuseMapIndex], 
                                          m_Images[normalMapIndex], 
                                          m_Images[roughnessMapIndex],
                                          m_Images[metallicMapIndex]);
            primitiveDiffuseNormalRoughnessMetallicMap.m_PbrDiffuseNormalRoughnessMetallicMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalRoughnessMetallicMap.push_back(primitiveDiffuseNormalRoughnessMetallicMap);
            LOG_CORE_INFO("material assigned: material index {0}, PbrDiffuseNormalRoughnessMetallic, features: 0x{1:x}", materialIndex, material.m_Features);
        }
        else if (pbrFeatures == (Material::HAS_DIFFUSE_MAP | Material::HAS_NORMAL_MAP | Material::HAS_ROUGHNESS_METALLIC_MAP | Material::HAS_SKELETAL_ANIMATION))
        {
            PrimitiveDiffuseNormalRoughnessMetallicSAMap primitiveDiffuseNormalRoughnessMetallicSAMap{};
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex           = material.m_DiffuseMapIndex;
            uint normalMapIndex            = material.m_NormalMapIndex;
            uint roughnessMetallicMapIndex = material.m_RoughnessMetallicMapIndex;

            CORE_ASSERT(diffuseMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: diffuseMapIndex < m_Images.size()");
            CORE_ASSERT(normalMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: normalMapIndex < m_Images.size()");
            CORE_ASSERT(roughnessMetallicMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: roughnessMetallicMapIndex < m_Images.size()");

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalRoughnessMetallicSAMap.m_PbrDiffuseNormalRoughnessMetallicSAMaterial,
                                          m_Images[diffuseMapIndex], 
                                          m_Images[normalMapIndex], 
                                          m_Images[roughnessMetallicMapIndex],
                                          m_ShaderData);
            primitiveDiffuseNormalRoughnessMetallicSAMap.m_PbrDiffuseNormalRoughnessMetallicSAMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalRoughnessMetallicSAMap.push_back(primitiveDiffuseNormalRoughnessMetallicSAMap);
            LOG_CORE_INFO("material assigned: material index {0}, PbrDiffuseNormalRoughnessMetallicSA, features: 0x{1:x}", materialIndex, material.m_Features);
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

            uint diffuseMapIndex           = material.m_DiffuseMapIndex;
            uint normalMapIndex            = material.m_NormalMapIndex;
            uint roughnessMetallicMapIndex = material.m_RoughnessMetallicMapIndex;
            CORE_ASSERT(diffuseMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: diffuseMapIndex < m_Images.size()");
            CORE_ASSERT(normalMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: normalMapIndex < m_Images.size()");
            CORE_ASSERT(roughnessMetallicMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: roughnessMetallicMapIndex < m_Images.size()");

            VK_Model::CreateDescriptorSet(primitiveDiffuseNormalRoughnessMetallicMap.m_PbrDiffuseNormalRoughnessMetallicMaterial, 
                                            m_Images[diffuseMapIndex], m_Images[normalMapIndex], m_Images[roughnessMetallicMapIndex]);
            primitiveDiffuseNormalRoughnessMetallicMap.m_PbrDiffuseNormalRoughnessMetallicMaterial.m_NormalMapIntensity = material.m_NormalMapIntensity;

            m_PrimitivesDiffuseNormalRoughnessMetallicMap.push_back(primitiveDiffuseNormalRoughnessMetallicMap);
            LOG_CORE_INFO("material assigned: material index {0}, PbrDiffuseNormalRoughnessMetallic, features: 0x{1:x}", materialIndex, material.m_Features);
        }
        else if (pbrFeatures & Material::HAS_DIFFUSE_MAP)
        {
            PrimitiveDiffuseMap primitiveDiffuseMap{};
            primitiveDiffuseMap.m_FirstIndex  = primitiveTmp.m_FirstIndex;
            primitiveDiffuseMap.m_FirstVertex = primitiveTmp.m_FirstVertex;
            primitiveDiffuseMap.m_IndexCount  = primitiveTmp.m_IndexCount;
            primitiveDiffuseMap.m_VertexCount = primitiveTmp.m_VertexCount;

            uint diffuseMapIndex = material.m_DiffuseMapIndex;
            CORE_ASSERT(diffuseMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: diffuseMapIndex < m_Images.size()");

            VK_Model::CreateDescriptorSet(primitiveDiffuseMap.m_PbrDiffuseMaterial, m_Images[diffuseMapIndex]);
            primitiveDiffuseMap.m_PbrDiffuseMaterial.m_Roughness = material.m_Roughness;
            primitiveDiffuseMap.m_PbrDiffuseMaterial.m_Metallic  = material.m_Metallic;

            m_PrimitivesDiffuseMap.push_back(primitiveDiffuseMap);
            LOG_CORE_INFO("material assigned: material index {0}, PbrDiffuse, features: 0x{1:x}", materialIndex, material.m_Features);
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
            LOG_CORE_INFO("material assigned: material index {0}, PbrNoMap, features: 0x{1:x}", materialIndex, material.m_Features);
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

                uint emissiveMapIndex = material.m_EmissiveMapIndex;
                CORE_ASSERT(emissiveMapIndex < m_Images.size(), "FbxBuilder::AssignMaterial: emissiveMapIndex < m_Images.size()");

                VK_Model::CreateDescriptorSet(primitiveEmissiveTexture.m_PbrEmissiveTextureMaterial, m_Images[emissiveMapIndex]);

                primitiveEmissiveTexture.m_PbrEmissiveTextureMaterial.m_Roughness = material.m_Roughness;
                primitiveEmissiveTexture.m_PbrEmissiveTextureMaterial.m_Metallic  = material.m_Metallic;
                primitiveEmissiveTexture.m_PbrEmissiveTextureMaterial.m_EmissiveStrength  = material.m_EmissiveStrength;

                m_PrimitivesEmissiveTexture.push_back(primitiveEmissiveTexture);
                LOG_CORE_INFO("material assigned: material index {0}, PbrEmissiveTexture, features: 0x{1:x}", materialIndex, material.m_Features);
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
                LOG_CORE_INFO("material assigned: material index {0}, PbrEmissive, features: 0x{1:x}", materialIndex, material.m_Features);
            }
        }
    }

    void FbxBuilder::CalculateTangents()
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

    void FbxBuilder::CalculateTangentsFromIndexBuffer(const std::vector<uint>& indices)
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

    void FbxBuilder::LoadSkeletonsFbx()
    {
        // to be implemented
    }

    void FbxBuilder::LoadJoint(int globalFbxNodeIndex, int parentJoint)
    {
        // to be implemented
    }

    void FbxBuilder::PrintMaps(const aiMaterial* fbxMaterial)
    {
        auto materialName = fbxMaterial->GetName();
        LOG_CORE_ERROR("material name: {0}", std::string(materialName.C_Str()));
        LOG_CORE_CRITICAL("aiTextureType_NONE                  = {0}", fbxMaterial->GetTextureCount(aiTextureType_NONE             ));
        LOG_CORE_CRITICAL("aiTextureType_DIFFUSE               = {0}", fbxMaterial->GetTextureCount(aiTextureType_DIFFUSE          ));
        LOG_CORE_CRITICAL("aiTextureType_SPECULAR              = {0}", fbxMaterial->GetTextureCount(aiTextureType_SPECULAR         ));
        LOG_CORE_CRITICAL("aiTextureType_AMBIENT               = {0}", fbxMaterial->GetTextureCount(aiTextureType_AMBIENT          ));
        LOG_CORE_CRITICAL("aiTextureType_EMISSIVE              = {0}", fbxMaterial->GetTextureCount(aiTextureType_EMISSIVE         ));
        LOG_CORE_CRITICAL("aiTextureType_HEIGHT                = {0}", fbxMaterial->GetTextureCount(aiTextureType_HEIGHT           ));
        LOG_CORE_CRITICAL("aiTextureType_NORMALS               = {0}", fbxMaterial->GetTextureCount(aiTextureType_NORMALS          ));
        LOG_CORE_CRITICAL("aiTextureType_SHININESS             = {0}", fbxMaterial->GetTextureCount(aiTextureType_SHININESS        ));
        LOG_CORE_CRITICAL("aiTextureType_OPACITY               = {0}", fbxMaterial->GetTextureCount(aiTextureType_OPACITY          ));
        LOG_CORE_CRITICAL("aiTextureType_DISPLACEMENT          = {0}", fbxMaterial->GetTextureCount(aiTextureType_DISPLACEMENT     ));
        LOG_CORE_CRITICAL("aiTextureType_LIGHTMAP              = {0}", fbxMaterial->GetTextureCount(aiTextureType_LIGHTMAP         ));
        LOG_CORE_CRITICAL("aiTextureType_REFLECTION            = {0}", fbxMaterial->GetTextureCount(aiTextureType_REFLECTION       ));
        LOG_CORE_CRITICAL("aiTextureType_BASE_COLOR            = {0}", fbxMaterial->GetTextureCount(aiTextureType_BASE_COLOR       ));
        LOG_CORE_CRITICAL("aiTextureType_NORMAL_CAMERA         = {0}", fbxMaterial->GetTextureCount(aiTextureType_NORMAL_CAMERA    ));
        LOG_CORE_CRITICAL("aiTextureType_EMISSION_COLOR        = {0}", fbxMaterial->GetTextureCount(aiTextureType_EMISSION_COLOR   ));
        LOG_CORE_CRITICAL("aiTextureType_METALNESS             = {0}", fbxMaterial->GetTextureCount(aiTextureType_METALNESS        ));
        LOG_CORE_CRITICAL("aiTextureType_DIFFUSE_ROUGHNESS     = {0}", fbxMaterial->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS));
        LOG_CORE_CRITICAL("aiTextureType_AMBIENT_OCCLUSION     = {0}", fbxMaterial->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION));
        LOG_CORE_CRITICAL("aiTextureType_SHEEN                 = {0}", fbxMaterial->GetTextureCount(aiTextureType_SHEEN            ));
        LOG_CORE_CRITICAL("aiTextureType_CLEARCOAT             = {0}", fbxMaterial->GetTextureCount(aiTextureType_CLEARCOAT        ));
        LOG_CORE_CRITICAL("aiTextureType_TRANSMISSION          = {0}", fbxMaterial->GetTextureCount(aiTextureType_TRANSMISSION     ));
        LOG_CORE_CRITICAL("aiTextureType_UNKNOWN               = {0}", fbxMaterial->GetTextureCount(aiTextureType_UNKNOWN          ));

        float factor;
        LOG_CORE_CRITICAL("AI_MATKEY_BASE_COLOR                = {0}", fbxMaterial->Get(AI_MATKEY_BASE_COLOR, factor) == aiReturn_SUCCESS);
        LOG_CORE_CRITICAL("AI_MATKEY_ROUGHNESS_FACTOR          = {0}", fbxMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == aiReturn_SUCCESS);
        LOG_CORE_CRITICAL("AI_MATKEY_METALLIC_FACTOR           = {0}", fbxMaterial->Get(AI_MATKEY_METALLIC_FACTOR, factor) == aiReturn_SUCCESS);
        LOG_CORE_CRITICAL("AI_MATKEY_COLOR_DIFFUSE             = {0}", fbxMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, factor) == aiReturn_SUCCESS);
        LOG_CORE_CRITICAL("AI_MATKEY_COLOR_EMISSIVE            = {0}", fbxMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, factor) == aiReturn_SUCCESS);
        LOG_CORE_CRITICAL("AI_MATKEY_USE_EMISSIVE_MAP          = {0}", fbxMaterial->Get(AI_MATKEY_USE_EMISSIVE_MAP, factor) == aiReturn_SUCCESS);
        LOG_CORE_CRITICAL("AI_MATKEY_EMISSIVE_INTENSITY        = {0}", fbxMaterial->Get(AI_MATKEY_EMISSIVE_INTENSITY, factor) == aiReturn_SUCCESS);
        LOG_CORE_CRITICAL("AI_MATKEY_COLOR_SPECULAR            = {0}", fbxMaterial->Get(AI_MATKEY_COLOR_SPECULAR, factor) == aiReturn_SUCCESS);
        LOG_CORE_CRITICAL("AI_MATKEY_REFLECTIVITY              = {0}", fbxMaterial->Get(AI_MATKEY_REFLECTIVITY, factor) == aiReturn_SUCCESS);

        uint numProperties = fbxMaterial->mNumProperties;
        for (uint propertyIndex = 0; propertyIndex < numProperties; ++propertyIndex)
        {
            aiMaterialProperty* materialProperty = fbxMaterial->mProperties[propertyIndex];
            std::string key = std::string(materialProperty->mKey.C_Str());
            LOG_CORE_CRITICAL("key: {0}", key);
        }
    }
}
