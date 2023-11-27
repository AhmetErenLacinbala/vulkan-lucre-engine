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

#include "scene/sceneGraph.h"

namespace GfxRenderEngine
{
    TreeNode::TreeNode(entt::entity gameObject,
                       const std::string& name,
                       const std::string& longName)
        : m_GameObject(gameObject),
          m_LongName(longName),
          m_Name(name)
    {}

    TreeNode::~TreeNode()
    {}

    entt::entity TreeNode::GetGameObject() const
    {
        return m_GameObject;
    }

    const std::string& TreeNode::GetName() const
    {
        return m_Name;
    }

    const std::string& TreeNode::GetLongName() const
    {
        return m_LongName;
    }

    uint TreeNode::Children() const
    {
        return m_Children.size();
    }

    uint TreeNode::GetChild(uint const childIndex)
    {
        return m_Children[childIndex];
    }

    uint TreeNode::AddChild(uint const nodeIndex)
    {
        uint childIndex = m_Children.size();
        m_Children.push_back(nodeIndex);
        return childIndex;
    }

    void TreeNode::SetGameObject(entt::entity gameObject)
    {
        m_GameObject = gameObject;
    }

    uint SceneGraph::CreateNode(entt::entity const gameObject, std::string const& name, std::string const& longName, Dictionary& dictionary)
    {
        uint nodeIndex = m_Nodes.size();
        m_Nodes.push_back({gameObject, name, longName});
        dictionary.InsertShort(name, gameObject);
        dictionary.InsertLong(longName, gameObject);
        m_MapFromGameObjectToNode[gameObject] = nodeIndex;
        return nodeIndex;
    }

    void SceneGraph::TraverseLog(uint nodeIndex, uint indent)
    {
        std::string indentStr(indent, ' ');
        TreeNode& treeNode = m_Nodes[nodeIndex];
        LOG_CORE_INFO("{0}game object `{1}`, name: `{2}`", indentStr, static_cast<uint>(treeNode.GetGameObject()), treeNode.GetName());
        for (uint childIndex = 0; childIndex < treeNode.Children(); ++childIndex)
        {
            TraverseLog(treeNode.GetChild(childIndex), indent + 4);
        }
    }

    TreeNode& SceneGraph::GetNode(uint const nodeIndex)
    {
        return m_Nodes[nodeIndex];
    }

    TreeNode& SceneGraph::GetNodeByGameObject(entt::entity const gameObject)
    {
        uint nodeIndex = m_MapFromGameObjectToNode[gameObject];
        return m_Nodes[nodeIndex];
    }

    TreeNode& SceneGraph::GetRoot()
    {
        CORE_ASSERT(m_Nodes.size(), "SceneGraph::GetRoot(): scene graph is empty");
        return m_Nodes[SceneGraph::ROOT_NODE];
    }

    uint SceneGraph::GetTreeNodeIndex(entt::entity const gameObject)
    {
        uint returnValue = NODE_INVALID;
        
        if (m_MapFromGameObjectToNode.find(gameObject) != m_MapFromGameObjectToNode.end())
        {
            returnValue = m_MapFromGameObjectToNode[gameObject];
        }
        return returnValue;
    }
}
