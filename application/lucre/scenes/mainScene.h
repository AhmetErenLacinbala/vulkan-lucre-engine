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
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/

#pragma once


#include "engine.h"
#include "scene/scene.h"
#include "scene/entity.h"
#include "renderer/renderer.h"
#include "renderer/cameraController.h"

#include "lucre.h"
#include "gamepadInputController.h"
#include "keyboardInputController.h"

namespace LucreApp
{
    class MainScene : public Scene
    {

    public:

        MainScene();
        ~MainScene() override {}

        void Start() override;
        void Stop() override;
        void OnUpdate(const Timestep& timestep) override;
        void OnEvent(Event& event) override;
        void OnResize() override;

    private:

        void LoadModels();

    private:

        std::shared_ptr<Renderer> m_Renderer;

        // camera
        std::shared_ptr<CameraController> m_CameraController;
        std::unique_ptr<Entity> m_CameraObject;
        std::shared_ptr<KeyboardInputController> m_KeyboardInputController;

        // game objects
        entt::entity m_Vase0, m_Vase1, m_Ground, m_Sphere;
        std::unique_ptr<GamepadInputController> m_GamepadInputController;
        TransformComponent m_GamepadInput;
    };
}
