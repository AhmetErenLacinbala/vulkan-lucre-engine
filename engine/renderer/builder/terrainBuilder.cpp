
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

#include "core.h"
#include "renderer/builder/terrainBuilder.h"
#include "auxiliary/file.h"
#include "scene/scene.h"
#include "stb_image.h"

namespace GfxRenderEngine
{

    void TerrainBuilder::PopulateTerrainData(std::vector<std::vector<float>> const& heightMap)
    {
        size_t rows = heightMap.size();
        size_t cols = heightMap.empty() ? 0 : heightMap[0].size();
        m_Vertices.resize(rows * cols);
        size_t vertexCounter = 0;

        // vertices
        for (size_t z = 0; z < rows; ++z)
        {
            for (size_t x = 0; x < cols; ++x)
            {
                Vertex& vertex = m_Vertices[vertexCounter];
                ++vertexCounter;

                float originY = heightMap[z][x];

                vertex.m_Position = glm::vec3(x, originY, z);
                vertex.m_Color = glm::vec4(0.f, 0.f, heightMap[z][x] / 3, 1.0f);
                vertex.m_UV = glm::vec2(0.f, 0.f);
                vertex.m_Tangent = glm::vec3(1.0f);
                vertex.m_JointIds = glm::ivec4(0);
                vertex.m_Weights = glm::vec4(0.0f);

                // compute normals via neighbors
                //     up
                // left O right
                //    down
                glm::vec3 sumNormals(0.0f);
                float leftY = x > 0 ? heightMap[z][x - 1] : 0.0f;
                float rightY = x < cols - 1 ? heightMap[z][x + 1] : 0.0f;
                float upY = z < rows - 1 ? heightMap[z + 1][x] : 0.0f;
                float downY = z > 0 ? heightMap[z - 1][x] : 0.0f;

                float dx = 1.0f;
                float dz = 1.0f;

                glm::vec3 left = glm::vec3(-dx, leftY - originY, 0.0f);
                glm::vec3 right = glm::vec3(dx, rightY - originY, 0.0f);
                glm::vec3 up = glm::vec3(0.0f, upY - originY, dz);
                glm::vec3 down = glm::vec3(0.0f, downY - originY, -dz);

                auto normalComponent = [&](glm::vec3 a, glm::vec3 b)
                {
                    glm::vec3 normal;
                    if (x > 0 && z > 0 && x < cols - 1 && z < rows - 1)
                    {
                        // Cross products to compute normals
                        normal = glm::cross(a, b);
                    }
                    else
                    {
                        normal = glm::vec3(0.0f, 1.0f, 0.0f);
                    }
                    return normal;
                };

                // smoothshading
                sumNormals = normalComponent(left, -down);
                sumNormals += normalComponent(-down, right);
                sumNormals += normalComponent(right, -up);
                sumNormals += normalComponent(-up, left);

                vertex.m_Normal = glm::normalize(sumNormals);
            }
        }

        // indices
        for (size_t z = 0; z < rows - 1; ++z)
        {
            for (size_t x = 0; x < cols - 1; ++x)
            {

                uint32_t topLeft = z * cols + x;

                uint32_t topRight = topLeft + 1;
                uint32_t bottomLeft = (z + 1) * cols + x;
                uint32_t bottomRight = bottomLeft + 1;

                m_Indices.push_back(topLeft);
                m_Indices.push_back(bottomLeft);
                m_Indices.push_back(topRight);
                m_Indices.push_back(topRight);
                m_Indices.push_back(bottomLeft);
                m_Indices.push_back(bottomRight);
            }
        }
    }

    bool TerrainBuilder::LoadTerrainHeightMap(Scene& scene, int instanceCount, Terrain::TerrainSpec const& terrainSpec)
    {
        m_Vertices.clear();
        m_Indices.clear();
        m_Submeshes.clear();
        int width, height, bytesPerPixel;
        stbi_set_flip_vertically_on_load(false);
        std::string const& filepathHeightMap = terrainSpec.m_FilepathHeightMap;
        uchar* localBuffer = stbi_load(filepathHeightMap.c_str(), &width, &height, &bytesPerPixel, 0);
        if (localBuffer)
        {
            std::vector<std::vector<float>> terrainData(height, std::vector<float>(width));
            for (int i = 0; i < height; ++i)
            {
                for (int j = 0; j < width; ++j)
                {
                    terrainData[i][j] = static_cast<float>(localBuffer[i * width + j]) / 127.0f;
                }
            }
            stbi_image_free(localBuffer);

            PopulateTerrainData(terrainData);

            // create game objects for all instances
            {
                auto& registry = scene.GetRegistry();
                auto& sceneGraph = scene.GetSceneGraph();
                auto& dictionary = scene.GetDictionary();

                auto name = EngineCore::GetFilenameWithoutPath(terrainSpec.m_FilepathTerrainDescription);
                name = EngineCore::GetFilenameWithoutExtension(name);
                InstanceTag instanceTag;

                for (int instanceIndex = 0; instanceIndex < instanceCount; ++instanceIndex)
                {
                    // create game object
                    entt::entity entity = registry.create();
                    std::shared_ptr<Model> model;
                    TransformComponent transform{};
                    instanceTag.m_Instances.push_back(entity);

                    // add to scene graph
                    auto instanceStr = std::to_string(instanceIndex);
                    auto shortName = name + "::" + instanceStr;
                    auto longName = terrainSpec.m_FilepathTerrainDescription + "::" + instanceStr;
                    uint newNode = sceneGraph.CreateNode(entity, shortName, longName, dictionary);
                    sceneGraph.GetRoot().AddChild(newNode);

                    // only for the 1st instance
                    if (!instanceIndex)
                    {
                        // create instance buffer
                        instanceTag.m_InstanceBuffer = InstanceBuffer::Create(instanceCount);
                        registry.emplace<InstanceTag>(entity, instanceTag);

                        Submesh submesh{};
                        submesh.m_FirstIndex = 0;
                        submesh.m_FirstVertex = 0;
                        submesh.m_IndexCount = m_Indices.size();
                        submesh.m_VertexCount = m_Vertices.size();
                        submesh.m_InstanceCount = instanceCount;

                        submesh.m_Material.m_PbrMaterial = terrainSpec.m_PbrMaterial;

                        { // create material descriptor
                            Material::MaterialTextures materialTextures;

                            auto materialDescriptor =
                                MaterialDescriptor::Create(MaterialDescriptor::MtPbr, materialTextures);
                            submesh.m_Material.m_MaterialDescriptor = materialDescriptor;
                        }
                        { // create resource descriptor
                            Resources::ResourceBuffers resourceBuffers;

                            resourceBuffers[Resources::INSTANCE_BUFFER_INDEX] = instanceTag.m_InstanceBuffer->GetBuffer();
                            auto resourceDescriptor = ResourceDescriptor::Create(resourceBuffers);
                            submesh.m_Resources.m_ResourceDescriptor = resourceDescriptor;
                        }
                        m_Submeshes.push_back(submesh);
                        model = Engine::m_Engine->LoadModel(*this);

                        PbrMaterialTag pbrMaterialTag{};
                        registry.emplace<PbrMaterialTag>(entity, pbrMaterialTag);
                    }

                    instanceTag.m_InstanceBuffer->SetInstanceData(instanceIndex, transform.GetMat4Global(),
                                                                  transform.GetNormalMatrix());
                    transform.SetInstance(instanceTag.m_InstanceBuffer, instanceIndex);
                    registry.emplace<TransformComponent>(entity, transform);

                    MeshComponent mesh{shortName, model};
                    registry.emplace<MeshComponent>(entity, mesh);
                }
            }

            return true;
        }
        else
        {
            LOG_CORE_CRITICAL("TerrainBuilder::LoadTerrainHeightMap: Couldn't load file {0}", filepathHeightMap);
        }

        return false;
    }

    void TerrainBuilder::CalculateTangents()
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

    void TerrainBuilder::CalculateTangentsFromIndexBuffer(std::vector<uint> const& indices)
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
                    uv1 = vertex.m_UV;
                    vertexIndex1 = index;
                    break;
                case 1:
                    position2 = vertex.m_Position;
                    uv2 = vertex.m_UV;
                    vertexIndex2 = index;
                    break;
                case 2:
                    position3 = vertex.m_Position;
                    uv3 = vertex.m_UV;
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
                    if (tangent.x == 0.0f && tangent.y == 0.0f && tangent.z == 0.0f)
                    {
                        tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                    }

                    m_Vertices[vertexIndex1].m_Tangent = tangent;
                    m_Vertices[vertexIndex2].m_Tangent = tangent;
                    m_Vertices[vertexIndex3].m_Tangent = tangent;

                    break;
            }
            cnt = (cnt + 1) % 3;
        }
    }
} // namespace GfxRenderEngine
