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

#include "entt.hpp"

#include "engine.h"

namespace GfxRenderEngine
{
    namespace Fbx
    {

        static constexpr int  FBX_NOT_USED = -1;
        static constexpr bool FBX_LOAD_SUCCESS = true;
        static constexpr bool FBX_LOAD_FAILURE = false;
        static constexpr int  FBX_ROOT_NODE = 0;

        struct Instance
        {
            entt::entity m_Entity;

            Instance() = default;
            Instance(entt::entity entity) : m_Entity{entity} {}
        };

        struct FbxFile
        {
            std::string m_Filename;
            std::vector<Instance> m_Instances;

            FbxFile() = default;
            FbxFile(std::string& filename) : m_Filename{filename} {}
        };

        struct FbxFiles
        {
            std::vector<FbxFile> m_FbxFilesFromScene;
            std::vector<FbxFile> m_FbxFilesFromPreFabs;
        };
    }
}