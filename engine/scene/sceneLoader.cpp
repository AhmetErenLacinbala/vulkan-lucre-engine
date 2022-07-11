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

#include "auxiliary/file.h"
#include "auxiliary/debug.h"
#include "scene/components.h"
#include "scene/sceneLoader.h"

namespace GfxRenderEngine
{
    SceneLoader::SceneLoader(Scene& scene)
        : m_Scene(scene)
    {
    }

    void SceneLoader::Deserialize(entt::entity& maxGameObjects)
    {

        YAML::Node yamlNode;
        auto& filepath = m_Scene.m_Filepath;

        if (EngineCore::FileExists(filepath))
        {
            LOG_CORE_WARN("Loading scene {0}", filepath);
            yamlNode = YAML::LoadFile(filepath);
        }
        else
        {
            LOG_CORE_CRITICAL("Scene loader could not find file {0}", filepath);
            return;
        }

        for(const auto& gltfFile : yamlNode["glTF-files"])
        {
            std::string filename = gltfFile.first.as<std::string>();
            if (EngineCore::FileExists(filename))
            {
                LOG_CORE_WARN("Scene loader found {0}", filename);
                Builder builder{filename};
                auto entity = builder.LoadGLTF(m_Scene.m_Registry, m_Scene.m_SceneHierarchy, m_Scene.m_Dictionary);

                switch (gltfFile.second.Type())
                {
                    case YAML::NodeType::Null:
                        break;
                    case YAML::NodeType::Scalar:
                        break;
                    case YAML::NodeType::Sequence:
                        break;
                    case YAML::NodeType::Map:
                    {
                        auto& transform = m_Scene.m_Registry.get<TransformComponent>(entity);
                        for(const auto& attribute : gltfFile.second)
                        {
                            if (attribute.first.as<std::string>() == "translation")
                            {
                                auto translation = ConvertToVec3(attribute.second);
                                transform.SetTranslation(translation);
                            } else
                            if (attribute.first.as<std::string>() == "scale")
                            {
                                auto scale = ConvertToVec3(attribute.second);
                                transform.SetScale(scale);
                            } else
                            if (attribute.first.as<std::string>() == "rotation")
                            {
                                auto rotation = ConvertToVec3(attribute.second);
                                transform.SetRotation(rotation);
                            } 
                        }
                        break;
                    }
                    case YAML::NodeType::Undefined:
                        break;
                    default:
                        break;
                }
            }
            else
            {
                LOG_CORE_CRITICAL("Scene loader could not find file {0}", gltfFile.as<std::string>());
            }
        }

        if (yamlNode["prefabs"])
        {
            const auto& prefabsFileList = yamlNode["prefabs"];
            for (const auto& prefab : prefabsFileList)
            {
                LoadPrefab(prefab.as<std::string>(), maxGameObjects);
            }
        }

        if (yamlNode["script-components"])
        {
            const auto& scriptFileList = yamlNode["script-components"];
            for(YAML::const_iterator it=scriptFileList.begin();it!=scriptFileList.end();++it)
            {
                std::string entityName = it->first.as<std::string>();
                std::string filepath = it->second.as<std::string>();
                LOG_CORE_INFO("found script '{0} for entity '{1}' in scene description", filepath, entityName);
                entt::entity gameObject = m_Scene.m_Dictionary.Retrieve(entityName);

                ScriptComponent scriptComponent(filepath);
                m_Scene.m_Registry.emplace<ScriptComponent>(gameObject, scriptComponent);
            }
        }
    }

    glm::vec3 SceneLoader::ConvertToVec3(const YAML::Node& node)
    {
        float values[3] = {0.0f, 0.0f, 0.0f};

        uint i = 0;
        for(const auto& vectorElement : node)
        {
            values[i] = vectorElement.as<float>();
            i++;
        }
        return glm::vec3(values[0], values[1], values[2]);
    }

    void SceneLoader::LoadPrefab(const std::string& filepath, entt::entity& maxGameObjects)
    {
        YAML::Node yamlNode;

        if (EngineCore::FileExists(filepath))
        {
            LOG_CORE_WARN("Scene loader found {0}", filepath);
            yamlNode = YAML::LoadFile(filepath);
        }
        else
        {
            LOG_CORE_CRITICAL("Scene loader could not find file {0}", filepath);
            return;
        }

        if (yamlNode["glTF-files"])
        {
            const auto& gltfFileList = yamlNode["glTF-files"];
            for (const auto& gltfFile : gltfFileList)
            {
                if (EngineCore::FileExists(gltfFile.as<std::string>()))
                {
                    LOG_CORE_WARN("Scene loader found {0}", gltfFile.as<std::string>());
                    Builder builder{gltfFile.as<std::string>()};
                    auto entity = builder.LoadGLTF(m_Scene.m_Registry, m_Scene.m_SceneHierarchy, m_Scene.m_Dictionary);
                    if ((entity != entt::null) && (maxGameObjects < entity)) maxGameObjects = entity;
                }
                else
                {
                    LOG_CORE_CRITICAL("Scene loader could not find file {0}", gltfFile.as<std::string>());
                }
            }
        }

        if (yamlNode["prefabs"])
        {
            const auto& prefabsFileList = yamlNode["prefabs"];
            for (const auto& prefab : prefabsFileList)
            {
                LoadPrefab(prefab.as<std::string>(), maxGameObjects);
            }
        }

        if (yamlNode["script-components"])
        {
            const auto& scriptFileList = yamlNode["script-components"];
            for(YAML::const_iterator it=scriptFileList.begin();it!=scriptFileList.end();++it)
            {
                std::string entityName = it->first.as<std::string>();
                std::string filepath = it->second.as<std::string>();
                LOG_CORE_INFO("found script '{0} for entity '{1}' in prefab", filepath, entityName);
                entt::entity gameObject = m_Scene.m_Dictionary.Retrieve(entityName);

                ScriptComponent scriptComponent(filepath);
                m_Scene.m_Registry.emplace<ScriptComponent>(gameObject, scriptComponent);
            }
        }
    }

    void SceneLoader::Serialize()
    {
    }
}
