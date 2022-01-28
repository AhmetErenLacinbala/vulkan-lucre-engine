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

#include "engine.h"

namespace GfxRenderEngine
{

    struct Vertex
    {
        glm::vec3 m_Position;
        glm::vec3 m_Color;
        glm::vec3 m_Normal;
        glm::vec2 m_UV;

        bool operator==(const Vertex& other) const;

    };

    // remember alignment requirements!
    // https://www.oreilly.com/library/view/opengl-programming-guide/9780132748445/app09lev1sec2.html
    struct GlobalUniformBuffer
    {
        glm::mat4 m_ProjectionView{1.0f};

        // point light
        glm::vec4 m_AmbientLightColor{1.0f, 1.0f, 1.0f, 0.02f};
        glm::vec3 m_LightPosition{0.0f, -0.2f, 2.5f};
        alignas(16) glm::vec4 m_LightColor{1.0f};
    };

    struct Builder
    {
        std::vector<Vertex> m_Vertices{};
        std::vector<uint> m_Indices{};

        void LoadModel(const std::string& filepath);
    };

    class Model
    {

    public:

        Model() {}
        virtual ~Model() {}

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;

        virtual void CreateVertexBuffers(const std::vector<Vertex>& vertices) = 0;
        virtual void CreateIndexBuffers(const std::vector<uint>& indices) = 0;

    };
}